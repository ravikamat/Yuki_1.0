// KnowledgeDaemon.cpp
// Yuki_1.0 — Self-Learning Knowledge Layer
#define NOMINMAX   // prevent windows.h min/max macros

#include "brain/learning/KnowledgeDaemon.h"
#include "brain/learning/LearningIngestor.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <memory>
#include <fstream>
#include "brain/memory/CognitiveMemoryFabric.h"

// ── Constructor / Destructor ──────────────────────────────────────────────────

KnowledgeDaemon::KnowledgeDaemon() = default;

KnowledgeDaemon::~KnowledgeDaemon() {
    stop();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool KnowledgeDaemon::start() {
    if (running_) return true;

    const char* script = "data\\brain\\yuki_knowledge_daemon.py";
    DWORD attr = GetFileAttributesA(script);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "[Knowledge] Daemon script not found: " << script << "\n";
        return false;
    }

    // Create stdin pipe (C++ writes → daemon reads)
    HANDLE hChildReadStdin  = INVALID_HANDLE_VALUE;
    HANDLE hParentWriteStdin = INVALID_HANDLE_VALUE;
    // Create stdout pipe (daemon writes → C++ reads)
    HANDLE hChildWriteStdout  = INVALID_HANDLE_VALUE;
    HANDLE hParentReadStdout  = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hChildReadStdin,  &hParentWriteStdin, &sa, 0) ||
        !CreatePipe(&hParentReadStdout, &hChildWriteStdout, &sa, 0)) {
        std::cerr << "[Knowledge] Pipe creation failed\n";
        return false;
    }

    // Don't inherit parent ends
    SetHandleInformation(hParentWriteStdin,  HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hParentReadStdout,  HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = hChildReadStdin;
    si.hStdOutput  = hChildWriteStdout;
    si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    char cmd[] = "python data\\brain\\yuki_knowledge_daemon.py";

    BOOL ok = CreateProcessA(nullptr, cmd, nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                              &si, &pi);

    CloseHandle(hChildReadStdin);
    CloseHandle(hChildWriteStdout);

    if (!ok) {
        std::cerr << "[Knowledge] Failed to launch Python daemon\n";
        CloseHandle(hParentWriteStdin);
        CloseHandle(hParentReadStdout);
        return false;
    }

    hProc_   = pi.hProcess;
    hThread_ = pi.hThread;
    hWrite_  = hParentWriteStdin;
    hRead_   = hParentReadStdout;

    running_ = true;

    std::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    worker_ = std::thread([this, p = std::move(readyPromise)]() mutable {
        p.set_value();
        this->readLoop();
    });

    if (readyFuture.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
        return false;
    }
    
    // Fallback: wait for actual python daemon readiness
    if (!waitForReady(8000)) {
        std::cerr << "[Knowledge] Daemon ready-wait timed out — continuing anyway\n";
    }

    return true;
}

void KnowledgeDaemon::stop() {
    if (!running_) return;
    stop_.store(true);
    running_ = false;

    sendLine("{\"cmd\":\"quit\"}");

    // Give daemon 2 seconds to exit cleanly
    if (hProc_ != INVALID_HANDLE_VALUE) {
        if (WaitForSingleObject(hProc_, 2000) != WAIT_OBJECT_0)
            TerminateProcess(hProc_, 0);
        CloseHandle(hProc_);
        CloseHandle(hThread_);
        hProc_   = INVALID_HANDLE_VALUE;
        hThread_ = INVALID_HANDLE_VALUE;
    }
    if (hWrite_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hWrite_);
        hWrite_ = INVALID_HANDLE_VALUE;
    }
    if (hRead_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hRead_);
        hRead_ = INVALID_HANDLE_VALUE;
    }
    // worker_ join is handled by RuntimeWorkerBase destructor or explicitly below
    if (worker_.joinable())
        worker_.join();
}

// ── Query ─────────────────────────────────────────────────────────────────────

KnowledgeAnswer KnowledgeDaemon::query(const std::string& question, int timeoutMs) {
    if (!running_ || !ready_) return {};

    int id = nextId_.fetch_add(1);

    // Register pending query with shared_ptr to avoid use-after-free
    auto pq = std::make_shared<PendingQuery>();
    {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_[id] = pq;
    }

    // Send query
    std::ostringstream j;
    j << "{\"cmd\":\"query\",\"id\":" << id << ",\"text\":" << jStr(question) << "}";
    if (!sendLine(j.str())) {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_.erase(id);
        return {};
    }

    // Wait for response using condition_variable (no busy-spin)
    {
        std::unique_lock<std::mutex> ul(pq->mtx);
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
        pq->cv.wait_until(ul, deadline, [pq]{ return pq->resolved; });
    }

    KnowledgeAnswer ans = pq->answer;
    {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_.erase(id);
    }
    return ans;
}

// ── Translate ─────────────────────────────────────────────────────────────────

std::string KnowledgeDaemon::translate(const std::string& text, int timeoutMs) {
    if (!running_ || !ready_ || text.empty()) return text;

    int id = nextId_.fetch_add(1);

    // Register pending query
    auto pq = std::make_shared<PendingQuery>();
    {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_[id] = pq;
    }

    // Send query
    std::ostringstream j;
    j << "{\"cmd\":\"translate\",\"id\":" << id << ",\"text\":" << jStr(text) << "}";
    if (!sendLine(j.str())) {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_.erase(id);
        return "__TRANSLATE_ERROR__";
    }

    // Wait for response
    {
        std::unique_lock<std::mutex> ul(pq->mtx);
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
        pq->cv.wait_until(ul, deadline, [pq]{ return pq->resolved; });
    }

    std::string ans = pq->answer.text;
    {
        std::lock_guard<std::mutex> lock(queryMutex_);
        pending_.erase(id);
    }
    return ans.empty() ? "__TRANSLATE_ERROR__" : ans;
}

// ── Learn topic ───────────────────────────────────────────────────────────────

void KnowledgeDaemon::learnTopic(const std::string& topic, LearnPriority priority) {
    if (!running_) return;
    // Section 8: Skip topics irrelevant to current user context
    if (!isRelevantToContext(topic)) {
        std::cerr << "[KnowledgeDaemon] SKIP: " << topic << " (irrelevant to context)\n";
        return;
    }
    const char* pStr = (priority == LearnPriority::P0_URGENT)   ? "p0" :
                       (priority == LearnPriority::P1_INTEREST)  ? "p1" : "p2";
    std::ostringstream j;
    j << "{\"cmd\":\"learn\",\"topic\":" << jStr(topic)
      << ",\"priority\":\"" << pStr << "\"}";
    sendLine(j.str());
}

// ── Layer 2/5: Interests request ──────────────────────────────────────────────

void KnowledgeDaemon::requestInterests() {
    if (!running_) return;
    sendLine("{\"cmd\":\"interests\"}");
}

InterestProfile KnowledgeDaemon::getInterestProfile() const {
    std::lock_guard<std::mutex> lock(interestMutex_);
    return interestProfile_;
}

// ── Callback ──────────────────────────────────────────────────────────────────

void KnowledgeDaemon::setLearningCallback(LearningCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    learningCb_ = std::move(cb);
}

// ── Internal: send line to daemon stdin ───────────────────────────────────────

bool KnowledgeDaemon::sendLine(const std::string& json) {
    if (hWrite_ == INVALID_HANDLE_VALUE) return false;
    std::string line = json + "\n";
    DWORD w;
    return WriteFile(hWrite_, line.c_str(), (DWORD)line.size(), &w, nullptr) != 0;
}

// ── Internal: wait for ready ──────────────────────────────────────────────────

bool KnowledgeDaemon::waitForReady(int timeoutMs) {
    std::unique_lock<std::mutex> lock(readyMutex_);
    return readyCv_.wait_for(lock,
                             std::chrono::milliseconds(timeoutMs),
                             [this]{ return ready_.load(); });
}

// ── Internal: reader thread ───────────────────────────────────────────────────

void KnowledgeDaemon::readLoop() {
    const size_t MAX_RESPONSE_SIZE = 65536;
    // Read in 256-byte chunks for efficiency (vs 1 byte at a time)
    std::string lineBuf;
    lineBuf.reserve(512);
    char buf[256];
    DWORD bytesRead;

    while (running_) {
        BOOL ok = ReadFile(hRead_, buf, sizeof(buf), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) break;

        if (lineBuf.size() + bytesRead > MAX_RESPONSE_SIZE) {
            std::cerr << "[KnowledgeDaemon] Response exceeded " << MAX_RESPONSE_SIZE << " bytes, discarding\n";
            lineBuf.clear();
            continue;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            char ch = buf[i];
            if (ch == '\n') {
                if (!lineBuf.empty()) {
                    std::string type, text, topic;
                    float confidence = 0.0f;
                    bool  found = false;
                    int   id = 0, count = 0;
                    std::vector<std::string> related;

                    if (parseJson(lineBuf, type, text, topic, confidence,
                                   found, id, count, related)) {

                        if (type == "ready") {
                            factsLearned_.store(count);
                            ready_ = true;
                            readyCv_.notify_all();   // wake waitForReady()
                            std::cout << "[Knowledge] Daemon ready. Facts in DB: " << count << "\n";

                        } else if (type == "learning") {
                            topicsLearned_.fetch_add(1);
                            factsLearned_.store(count);
                            std::cout << "[Knowledge] Learned: " << topic
                                      << " (total=" << count << ")\n";
                            LearningCallback cb;
                            {
                                std::lock_guard<std::mutex> lock(cbMutex_);
                                cb = learningCb_;
                            }
                            if (cb) cb(topic, count);

                            // Forward structured fields directly to Ingestor
                            if (!text.empty() && confidence >= 0.40f) {
                                std::string relatedStr;
                                for (size_t ri = 0; ri < related.size(); ++ri) {
                                    if (ri) relatedStr += '|';
                                    relatedStr += related[ri];
                                }
                                LearningIngestor::instance().submitFromDaemon(
                                    topic, text, confidence, relatedStr);

                                if (cmf_) {
                                    yuki::memory::MemoryPacket pkt;
                                    pkt.type = yuki::memory::MemoryPacket::KNOWLEDGE_FACT;
                                    pkt.timestamp_ms = GetTickCount64();
                                    pkt.source = "knowledge_daemon";
                                    pkt.text = text;
                                    pkt.confidence = confidence;
                                    pkt.topic_tag = topic;
                                    cmf_->ingest(pkt);
                                }
                                // Push into packet ring buffer for BLE/BabyMode consumers
                                {
                                    std::lock_guard<std::mutex> lock(packet_mutex_);
                                    KnowledgePacket kp;
                                    kp.topic         = topic;
                                    kp.summary       = text;
                                    kp.confidence    = confidence;
                                    kp.timestamp_ms  = GetTickCount64();
                                    kp.key_entities  = related;
                                    recent_packets_.push_back(std::move(kp));
                                    if (recent_packets_.size() > MAX_PACKETS)
                                        recent_packets_.pop_front();
                                }
                            }

                        } else if (type == "answer") {
                            std::shared_ptr<PendingQuery> pq = nullptr;
                            {
                                std::lock_guard<std::mutex> lock(queryMutex_);
                                auto it = pending_.find(id);
                                if (it != pending_.end()) pq = it->second;
                            }
                            if (pq) {
                                {
                                    std::lock_guard<std::mutex> ul(pq->mtx);
                                    pq->answer.found      = found;
                                    pq->answer.text       = text;
                                    pq->answer.topic      = topic;
                                    pq->answer.confidence = confidence;
                                    pq->answer.related    = related;
                                    pq->resolved          = true;
                                }
                                pq->cv.notify_one();
                            }
                            // Feed daemon answers into background learning pipeline
                            if (found && !text.empty() && confidence >= 0.40f) {
                                std::string relatedStr;
                                for (size_t ri = 0; ri < related.size(); ++ri) {
                                    if (ri) relatedStr += '|';
                                    relatedStr += related[ri];
                                }
                                LearningIngestor::instance().submitFromDaemon(
                                    topic, text, confidence, relatedStr);

                                if (cmf_) {
                                    yuki::memory::MemoryPacket pkt;
                                    pkt.type = yuki::memory::MemoryPacket::KNOWLEDGE_FACT;
                                    pkt.timestamp_ms = GetTickCount64();
                                    pkt.source = "knowledge_daemon";
                                    pkt.text = text;
                                    pkt.confidence = confidence;
                                    pkt.topic_tag = topic;
                                    cmf_->ingest(pkt);
                                }
                            }

                        } else if (type == "translate") {
                            std::shared_ptr<PendingQuery> pq = nullptr;
                            {
                                std::lock_guard<std::mutex> lock(queryMutex_);
                                auto it = pending_.find(id);
                                if (it != pending_.end()) pq = it->second;
                            }
                            if (pq) {
                                {
                                    std::lock_guard<std::mutex> ul(pq->mtx);
                                    pq->answer.text = text;
                                    pq->resolved    = true;
                                }
                                pq->cv.notify_one();
                            }

                        } else if (type == "status") {
                            factsLearned_.store(count);
                            std::cout << "[Knowledge] Status: facts=" << count << "\n";

                        } else if (type == "error") {
                            std::cerr << "[Knowledge] Daemon error: " << text << "\n";

                        } else if (type == "interests") {
                            // Layer 2/5: cache domain interest profile
                            // text = self_summary, topic = top_domain
                            std::lock_guard<std::mutex> ilock(interestMutex_);
                            interestProfile_.selfSummary = text;   // reused field
                            interestProfile_.topDomain   = topic;  // reused field
                            interestProfile_.totalFacts  = count;  // reused field
                            interestProfile_.loaded      = true;
                            std::cout << "[Knowledge] Interests cached. Top domain: "
                                      << topic << ", summary: "
                                      << text.substr(0, 60) << "...\n";
                        }
                    }
                    lineBuf.clear();
                }
            } else {
                lineBuf += ch;
            }
        }
    }
    running_ = false;

    // ── Auto-restart if Python died unexpectedly ───────────────────────
    // stopRequested_ is only set true by stop(). If Python crashed on its
    // own, restart after a brief delay.
    if (!stop_.load()) {
        // Detach THIS thread before start() reassigns worker_.
        worker_.detach();

        // Close dead process handles.
        if (hProc_   != INVALID_HANDLE_VALUE) { CloseHandle(hProc_);   hProc_   = INVALID_HANDLE_VALUE; }
        if (hThread_ != INVALID_HANDLE_VALUE) { CloseHandle(hThread_); hThread_ = INVALID_HANDLE_VALUE; }
        if (hWrite_  != INVALID_HANDLE_VALUE) { CloseHandle(hWrite_);  hWrite_  = INVALID_HANDLE_VALUE; }
        if (hRead_   != INVALID_HANDLE_VALUE) { CloseHandle(hRead_);   hRead_   = INVALID_HANDLE_VALUE; }
        ready_ = false;

        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (!stop_.load()) {
            std::cerr << "[Knowledge] Daemon exited unexpectedly - restarting...\n";
            start();   // assigns a new readThread_ safely (old one is detached)
        }
        // Old detached thread exits here.
    }
}

// ── JSON helpers ──────────────────────────────────────────────────────────────

std::string KnowledgeDaemon::jStr(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else                r += c;
    }
    return r + "\"";
}

// Minimal JSON parser for the small set of fields we use
bool KnowledgeDaemon::parseJson(const std::string& line,
                                  std::string& typeOut,
                                  std::string& textOut,
                                  std::string& topicOut,
                                  float&       confidenceOut,
                                  bool&        foundOut,
                                  int&         idOut,
                                  int&         countOut,
                                  std::vector<std::string>& relatedOut) {
    // Extract quoted string value for a key: "key":"value"
    auto extractStr = [&](const std::string& key) -> std::string {
        auto kpos = line.find("\"" + key + "\"");
        if (kpos == std::string::npos) return "";
        auto vstart = line.find('"', kpos + key.size() + 2);
        if (vstart == std::string::npos) return "";
        ++vstart;
        auto vend = line.find('"', vstart);
        // Handle escaped quotes
        while (vend != std::string::npos && line[vend-1] == '\\')
            vend = line.find('"', vend + 1);
        if (vend == std::string::npos) return "";
        std::string val = line.substr(vstart, vend - vstart);
        // Unescape basic sequences
        std::string out;
        for (size_t i = 0; i < val.size(); ++i) {
            if (val[i] == '\\' && i+1 < val.size()) {
                if      (val[i+1] == 'n')  { out += '\n'; ++i; }
                else if (val[i+1] == '"')  { out += '"';  ++i; }
                else if (val[i+1] == '\\') { out += '\\'; ++i; }
                else out += val[i];
            } else {
                out += val[i];
            }
        }
        return out;
    };

    auto extractNum = [&](const std::string& key) -> double {
        auto kpos = line.find("\"" + key + "\"");
        if (kpos == std::string::npos) return 0.0;
        auto colon = line.find(':', kpos);
        if (colon == std::string::npos) return 0.0;
        size_t vs = colon + 1;
        while (vs < line.size() && line[vs] == ' ') ++vs;
        return std::stod(line.substr(vs));
    };

    auto extractBool = [&](const std::string& key) -> bool {
        auto kpos = line.find("\"" + key + "\"");
        if (kpos == std::string::npos) return false;
        auto colon = line.find(':', kpos);
        if (colon == std::string::npos) return false;
        size_t vs = colon + 1;
        while (vs < line.size() && line[vs] == ' ') ++vs;
        return line.substr(vs, 4) == "true";
    };

    // Level 3: parse JSON string array "key":["a","b","c"]
    auto extractArray = [&](const std::string& key) -> std::vector<std::string> {
        std::vector<std::string> result;
        auto kpos   = line.find("\"" + key + "\"");
        if (kpos == std::string::npos) return result;
        auto astart = line.find('[', kpos);
        auto aend   = line.find(']', kpos);
        if (astart == std::string::npos || aend == std::string::npos) return result;
        std::string arr = line.substr(astart + 1, aend - astart - 1);
        size_t pos = 0;
        while (pos < arr.size()) {
            auto q1 = arr.find('"', pos);
            if (q1 == std::string::npos) break;
            auto q2 = arr.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            result.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
            pos = q2 + 1;
        }
        return result;
    };

    if (line.find('{') == std::string::npos) return false;

    typeOut       = extractStr("type");
    textOut       = extractStr("text");
    topicOut      = extractStr("topic");
    try { confidenceOut = (float)extractNum("confidence"); } catch (...) {}
    try { idOut         = (int)extractNum("id");           } catch (...) {}
    try { countOut      = (int)extractNum("count");        } catch (...) {}
    if (countOut == 0) try { countOut = (int)extractNum("facts"); } catch (...) {}
    foundOut   = extractBool("found");
    relatedOut = extractArray("related");  // Level 3: graph edges

    return !typeOut.empty();
}

void KnowledgeDaemon::setMemoryFabric(std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf) {
    cmf_ = std::move(cmf);
}

std::vector<KnowledgeDaemon::KnowledgePacket> KnowledgeDaemon::getRecentPackets(size_t n) const {
    std::lock_guard<std::mutex> lock(packet_mutex_);
    size_t count = std::min(n, recent_packets_.size());
    return std::vector<KnowledgePacket>(recent_packets_.begin(),
                                       recent_packets_.begin() + static_cast<std::ptrdiff_t>(count));
}
