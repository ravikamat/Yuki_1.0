// BackgroundAgents.cpp — Background task manager + browser agent (merged)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "BackgroundAgents.h"
#include <iostream>
#include <chrono>
#include <sstream>

// ══════════════════════════════════════════════════════════════════════════════
// BackgroundTaskManager
// ══════════════════════════════════════════════════════════════════════════════

std::string BackgroundTaskManager::makeId(int n) { return "task_" + std::to_string(n); }
BackgroundTaskManager::BackgroundTaskManager() = default;
BackgroundTaskManager::~BackgroundTaskManager() { stop(); }

void BackgroundTaskManager::start() {
    stopping_ = false;
    supervisorStopping_ = false;
    {
        std::lock_guard<std::mutex> lock(workersMu_);
        for (size_t i = 0; i < TARGET_WORKERS; ++i) {
            workers_.emplace_back([this]{ workerLoop(); });
        }
    }
    supervisor_ = std::thread([this]{ supervisorLoop(); });
    std::cout << "[BGTask] " << TARGET_WORKERS << " worker threads + supervisor started\n";
}

void BackgroundTaskManager::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stopping_ = true;
    }
    cv_.notify_all();
    
    {
        std::lock_guard<std::mutex> lock(workersMu_);
        supervisorStopping_ = true;
    }
    
    if (supervisor_.joinable()) supervisor_.join();
    
    std::lock_guard<std::mutex> lock(workersMu_);
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void BackgroundTaskManager::supervisorLoop() {
    while (!supervisorStopping_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::lock_guard<std::mutex> lock(workersMu_);
        if (supervisorStopping_) break;
        
        // Count alive workers
        size_t alive = 0;
        for (auto& t : workers_) {
            if (t.joinable()) ++alive;
        }
        
        if (alive < TARGET_WORKERS) {
            std::cout << "[BGTask] Supervisor detected " << (TARGET_WORKERS - alive) 
                      << " dead worker(s). Restarting...\n";
            
            // Remove dead threads (not joinable = finished/crashed)
            workers_.erase(
                std::remove_if(workers_.begin(), workers_.end(),
                    [](std::thread& t) { return !t.joinable(); }),
                workers_.end());
            
            // Spawn replacements up to target
            while (workers_.size() < TARGET_WORKERS) {
                workers_.emplace_back([this]{ workerLoop(); });
                std::cout << "[BGTask] Restarted worker. Total alive: " << workers_.size() << "\n";
            }
        }
    }
}

void BackgroundTaskManager::workerLoop() {
    while (true) {
        QueueItem item;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this]{ return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) break;
            item = std::move(queue_.front()); queue_.pop();
        }
        {
            std::lock_guard<std::mutex> lock(statusMu_);
            auto it = tasks_.find(item.taskId);
            if (it != tasks_.end() && it->second.state == TaskState::CANCELLED) continue;
            if (it != tasks_.end()) it->second.state = TaskState::RUNNING;
        }
        std::cout << "[BGTask:" << item.taskId << "] Starting: " << item.description << "\n";
        bool success = false; std::string result;
        try { result = item.fn(); success = true; }
        catch (const std::exception& e) { result = std::string("Exception: ") + e.what(); }
        catch (...) { result = "Unknown exception"; }
        {
            std::lock_guard<std::mutex> lock(statusMu_);
            auto it = tasks_.find(item.taskId);
            if (it != tasks_.end() && it->second.state != TaskState::CANCELLED) {
                it->second.state    = success ? TaskState::COMPLETED : TaskState::FAILED;
                it->second.result   = result;
                it->second.progress = 100;
            }
        }
        std::cout << "[BGTask:" << item.taskId << "] " << (success ? "COMPLETED" : "FAILED")
                  << ": " << result.substr(0, 80) << "\n";
        if (item.onDone) item.onDone(item.taskId, success, result);
    }
}

std::string BackgroundTaskManager::submit(const std::string& description, TaskFn fn, CompletionFn onDone) {
    std::string id;
    { std::lock_guard<std::mutex> lock(mu_); id = makeId(nextId_++); queue_.push({id, description, std::move(fn), std::move(onDone)}); }
    { std::lock_guard<std::mutex> lock(statusMu_); BackgroundTask t; t.taskId = id; t.description = description; t.state = TaskState::QUEUED; tasks_[id] = t; }
    cv_.notify_one();
    std::cout << "[BGTask] Queued " << id << ": " << description << "\n";
    return id;
}

void BackgroundTaskManager::cancel(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(statusMu_);
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) it->second.state = TaskState::CANCELLED;
}

TaskState BackgroundTaskManager::getState(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(statusMu_);
    auto it = tasks_.find(taskId);
    return (it != tasks_.end()) ? it->second.state : TaskState::CANCELLED;
}

std::string BackgroundTaskManager::getResult(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(statusMu_);
    auto it = tasks_.find(taskId);
    return (it != tasks_.end()) ? it->second.result : "";
}

std::string BackgroundTaskManager::describeAllTasks() const {
    std::lock_guard<std::mutex> lock(statusMu_);
    if (tasks_.empty()) return "No background tasks.";
    auto stateStr = [](TaskState s) -> const char* {
        switch (s) {
            case TaskState::QUEUED:    return "queued";
            case TaskState::RUNNING:   return "running";
            case TaskState::COMPLETED: return "done";
            case TaskState::FAILED:    return "failed";
            case TaskState::CANCELLED: return "cancelled";
            default: return "unknown";
        }
    };
    int running = 0, done = 0, failed = 0;
    for (const auto& [id, t] : tasks_) {
        if (t.state == TaskState::RUNNING)   ++running;
        if (t.state == TaskState::COMPLETED) ++done;
        if (t.state == TaskState::FAILED)    ++failed;
    }
    std::ostringstream ss;
    ss << "Background tasks: " << running << " running, " << done << " done";
    if (failed) ss << ", " << failed << " failed";
    ss << ".\n";
    for (const auto& [id, t] : tasks_)
        if (t.state == TaskState::RUNNING || t.state == TaskState::QUEUED)
            ss << "  [" << id << "] " << stateStr(t.state) << " — " << t.description << "\n";
    return ss.str();
}

// ══════════════════════════════════════════════════════════════════════════════
// BrowserAgent
// ══════════════════════════════════════════════════════════════════════════════

BrowserAgent::BrowserAgent() = default;
BrowserAgent::~BrowserAgent() { stop(); }

bool BrowserAgent::start() {
    if (running_) return true;
    const char* script = "data\\brain\\yuki_browser_agent.py";
    if (GetFileAttributesA(script) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "[BrowserAgent] Script not found: " << script << "\n"; return false;
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hChildStdin = INVALID_HANDLE_VALUE, hChildStdout = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&hChildStdin, &hWrite_, &sa, 0) || !CreatePipe(&hRead_, &hChildStdout, &sa, 0)) {
        std::cerr << "[BrowserAgent] Pipe creation failed\n"; return false;
    }
    SetHandleInformation(hWrite_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hRead_,  HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hChildStdin; si.hStdOutput = hChildStdout;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    char cmd[] = "python data\\brain\\yuki_browser_agent.py";
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hChildStdin); CloseHandle(hChildStdout);
    if (!ok) {
        CloseHandle(hWrite_); CloseHandle(hRead_);
        hWrite_ = hRead_ = INVALID_HANDLE_VALUE;
        std::cerr << "[BrowserAgent] Failed to start Python agent\n"; return false;
    }
    hProc_ = pi.hProcess; hThread_ = pi.hThread;
    running_ = true;
    readThread_ = std::thread([this]{ readLoop(); });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready_ && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[BrowserAgent] Ready: " << (ready_ ? "yes" : "timeout") << "\n";
    return true;
}

void BrowserAgent::stop() {
    if (!running_) return;
    running_ = false;
    sendLine("{\"cmd\":\"quit\"}");
    if (hProc_ != INVALID_HANDLE_VALUE) { WaitForSingleObject(hProc_, 2000); CloseHandle(hProc_); CloseHandle(hThread_); hProc_ = hThread_ = INVALID_HANDLE_VALUE; }
    if (hWrite_ != INVALID_HANDLE_VALUE) { CloseHandle(hWrite_); hWrite_ = INVALID_HANDLE_VALUE; }
    if (hRead_  != INVALID_HANDLE_VALUE) { CloseHandle(hRead_);  hRead_  = INVALID_HANDLE_VALUE; }
    if (readThread_.joinable()) readThread_.join();
}

void BrowserAgent::readLoop() {
    std::string buf; char ch; DWORD n;
    while (running_) {
        if (!ReadFile(hRead_, &ch, 1, &n, nullptr) || n == 0) break;
        if (ch == '\n') {
            if (!buf.empty()) {
                bool isResult  = buf.find("\"result\"")  != std::string::npos;
                bool isReady   = buf.find("\"ready\"")   != std::string::npos;
                bool isSuccess = buf.find("\"success\":true") != std::string::npos;
                auto getStr = [&](const std::string& key) -> std::string {
                    auto kpos = buf.find("\"" + key + "\"");
                    if (kpos == std::string::npos) return "";
                    auto vstart = buf.find('"', kpos + key.size() + 2);
                    if (vstart == std::string::npos) return "";
                    ++vstart; auto vend = vstart;
                    while (vend < buf.size() && buf[vend] != '"') ++vend;
                    return buf.substr(vstart, vend - vstart);
                };
                if (isReady) {
                    ready_ = true;
                    std::cout << "[BrowserAgent] Agent ready\n";
                } else if (isResult) {
                    std::lock_guard<std::mutex> lock(resultMu_);
                    lastResult_.success = isSuccess;
                    lastResult_.detail  = getStr("detail");
                    resultReady_ = true;
                    resultCv_.notify_one();
                    std::cout << "[BrowserAgent] Result: " << (isSuccess?"OK":"FAIL") << " " << lastResult_.detail.substr(0, 80) << "\n";
                }
                buf.clear();
            }
        } else { buf += ch; }
    }
}

BrowserResult BrowserAgent::sendAndWait(const std::string& jsonCmd, int timeoutMs) {
    if (!running_) return {false, "Browser agent not running"};
    resultReady_ = false;
    if (!sendLine(jsonCmd)) return {false, "Failed to send command to browser agent"};
    {
        std::unique_lock<std::mutex> lock(resultMu_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!resultReady_) {
            if (resultCv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return {false, "Browser automation timeout after 30s"};
            }
        }
        return lastResult_;
    }
}

BrowserResult BrowserAgent::sendWhatsApp(const std::string& contact, const std::string& message, int timeoutMs) {
    std::ostringstream j;
    j << "{\"cmd\":\"whatsapp_msg\",\"contact\":" << jStr(contact) << ",\"message\":" << jStr(message) << "}";
    return sendAndWait(j.str(), timeoutMs);
}
BrowserResult BrowserAgent::openUrl(const std::string& url, int timeoutMs) {
    std::ostringstream j; j << "{\"cmd\":\"open_url\",\"url\":" << jStr(url) << "}";
    return sendAndWait(j.str(), timeoutMs);
}
BrowserResult BrowserAgent::findTab(const std::string& keyword, int timeoutMs) {
    std::ostringstream j; j << "{\"cmd\":\"find_tab\",\"keyword\":" << jStr(keyword) << "}";
    return sendAndWait(j.str(), timeoutMs);
}
bool BrowserAgent::sendLine(const std::string& json) {
    if (hWrite_ == INVALID_HANDLE_VALUE) return false;
    std::string line = json + "\n"; DWORD w;
    return WriteFile(hWrite_, line.c_str(), (DWORD)line.size(), &w, nullptr) != 0;
}
std::string BrowserAgent::jStr(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else                r += c;
    }
    return r + "\"";
}
