// ScreenRuntime.cpp
// Yuki_1.0 — Real-Time Screen Vision
//
// C++ capture loop (foreground window, 200ms) + Python vision server pipe
// for heavyweight CV analysis (brightness, edge density, OCR).

#include "input/ScreenRuntime.h"
#include "input/PerceptionLayer.h"
#include "brain/core/SystemConfig.h"
#include <psapi.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

// Path to the Python vision server (relative to process CWD)
static const char* VISION_SERVER_PY = "data/vision/yuki_vision_server.py";

// Frame interval — 200ms ≈ 5 fps (fast enough to notice window switches instantly)
static const DWORD FRAME_INTERVAL_MS = 200;

// ── Constructor / Destructor ────────────────────────────────────────────────

ScreenRuntime::ScreenRuntime(SubsystemControl& control)
    : control_(control),
      state_(SubsystemRuntimeState::STOPPED) {}

ScreenRuntime::~ScreenRuntime() {
    stop();
}

// ── Public API ──────────────────────────────────────────────────────────────

std::string ScreenRuntime::getLastException() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastException_;
}

void ScreenRuntime::start() {
    std::lock_guard<std::mutex> lock(startStopMutex_);

    if (!stop_.load() && worker_.joinable()) {
        std::cerr << "[ScreenRuntime] start() ignored: already running.\n";
        return;
    }

    // CRITICAL FIX: Join any stale thread before creating a new one.
    // std::thread assignment on a joinable thread calls std::terminate().
    if (worker_.joinable()) {
        std::cerr << "[ScreenRuntime] start() joining stale thread...\n";
        stop_.store(true);
        try {
            worker_.join();
        } catch (const std::exception& e) {
            std::cerr << "[ScreenRuntime] join() exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[ScreenRuntime] join() unknown exception.\n";
        }
    }

    stop_.store(false);
    state_ = SubsystemRuntimeState::STARTING;
    lastException_.clear();

    try {
        worker_ = std::thread([this]() {
            try {
                this->captureLoop();
            } catch (const std::exception& e) {
                this->lastException_ = std::string("captureLoop exception: ") + e.what();
                std::cerr << "[ScreenRuntime] " << this->lastException_ << "\n";
            } catch (...) {
                this->lastException_ = "captureLoop unknown exception";
                std::cerr << "[ScreenRuntime] " << this->lastException_ << "\n";
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "[ScreenRuntime] thread creation failed: " << e.what() << "\n";
        state_ = SubsystemRuntimeState::STOPPED;
        stop_.store(true);
    }
}

void ScreenRuntime::stop() {
    std::lock_guard<std::mutex> lock(startStopMutex_);
    stop_.store(true);

    // ── Kill the child process FIRST (no mutex needed) ──────────────────────
    // This is the key ordering fix: TerminateProcess causes the child's
    // stdout write-end to close, which makes any active PeekNamedPipe/ReadFile
    // in sendCommand() return ERROR_BROKEN_PIPE *immediately*.  sendCommand()
    // then releases pipeMutex_ and returns false.  Without this, acquiring
    // pipeMutex_ in stopVisionServer() deadlocks while sendCommand() waits
    // for pipe data that will never arrive.
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess_, 0);
    }

    // ── Join the worker synchronously ───────────────────────────────────────
    // After TerminateProcess, sendCommand() unblocks in <50ms.  The while
    // loop sees stop_==true and exits.  captureLoop() returns.  Join is fast.
    // DO NOT detach — a detached thread becomes a zombie if start() resets
    // stop_ to false before the old thread checks it.
    if (worker_.joinable()) {
        try { worker_.join(); } catch (...) {}
    }

    // ── Now clean up handles (child is dead, worker has exited) ─────────────
    stopVisionServer();
    state_ = SubsystemRuntimeState::STOPPED;
}


SubsystemRuntimeState ScreenRuntime::reportState() const {
    return state_.load();
}

ScreenFrameSnapshot ScreenRuntime::getLatestFrame() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestFrame_;
}

bool ScreenRuntime::isVisionServerActive() const {
    return visionServerRunning_.load();
}

void ScreenRuntime::requestCapture() {
    captureRequested_ = true;
}

// ── Python Vision Server Lifecycle ──────────────────────────────────────────

void ScreenRuntime::startVisionServer() {
    if (visionServerRunning_.load()) {
        std::cerr << "[ScreenRuntime] startVisionServer: already running.\n";
        return;
    }

    // Close any stale handles first
    if (hChildStdinWrite_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hChildStdinWrite_);
        hChildStdinWrite_ = INVALID_HANDLE_VALUE;
    }
    if (hChildStdoutRead_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hChildStdoutRead_);
        hChildStdoutRead_ = INVALID_HANDLE_VALUE;
    }
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess_, 0);
        WaitForSingleObject(hProcess_, 1000);
        CloseHandle(hProcess_);
        hProcess_ = INVALID_HANDLE_VALUE;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinRead  = INVALID_HANDLE_VALUE;
    HANDLE hStdoutWrite= INVALID_HANDLE_VALUE;

    if (!CreatePipe(&hStdinRead,  &hChildStdinWrite_,  &sa, 0) ||
        !CreatePipe(&hChildStdoutRead_, &hStdoutWrite, &sa, 0)) {
        std::cerr << "[ScreenRuntime] Failed to create pipes. GLE=" << GetLastError() << "\n";
        return;
    }

    SetHandleInformation(hChildStdinWrite_,  HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hChildStdoutRead_,  HANDLE_FLAG_INHERIT, 0);

    char cmdLine[512];
    std::snprintf(cmdLine, sizeof(cmdLine), "python \"%s\"", VISION_SERVER_PY);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.hStdInput   = hStdinRead;
    si.hStdOutput  = hStdoutWrite;
    si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags     = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmdLine, nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[ScreenRuntime] Failed to launch vision server. Error=" << GetLastError() << "\n";
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hChildStdinWrite_); hChildStdinWrite_ = INVALID_HANDLE_VALUE;
        CloseHandle(hChildStdoutRead_); hChildStdoutRead_ = INVALID_HANDLE_VALUE;
        return;
    }

    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(pi.hThread);
    hProcess_ = pi.hProcess;

    visionServerRunning_ = true;
    std::cout << "[ScreenRuntime] Vision server launched (PID " << pi.dwProcessId << ").\n";
}

void ScreenRuntime::stopVisionServer() {
    // Called AFTER the worker thread has been joined and the child process
    // has been terminated (by stop()).  No concurrent pipe access is possible,
    // so pipeMutex_ is acquired purely for formality / defensive coding.
    visionServerRunning_ = false;

    {
        std::lock_guard<std::mutex> pipeLk(pipeMutex_);
        if (hChildStdinWrite_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hChildStdinWrite_);
            hChildStdinWrite_ = INVALID_HANDLE_VALUE;
        }
        if (hChildStdoutRead_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hChildStdoutRead_);
            hChildStdoutRead_ = INVALID_HANDLE_VALUE;
        }
    }

    if (hProcess_ != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(hProcess_, 500);
        CloseHandle(hProcess_);
        hProcess_ = INVALID_HANDLE_VALUE;
    }
}

bool ScreenRuntime::sendCommand(const std::string& jsonCmd, std::string& outJson) {
    std::lock_guard<std::mutex> lock(pipeMutex_);

    if (hChildStdinWrite_ == INVALID_HANDLE_VALUE || hChildStdoutRead_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!jsonCmd.empty()) {
        std::string line = jsonCmd;
        if (line.back() != '\n') line += '\n';
        DWORD written = 0;
        if (!WriteFile(hChildStdinWrite_, line.c_str(),
                       static_cast<DWORD>(line.size()), &written, nullptr)) {
            return false;
        }
    }

    std::string buf;
    char ch = 0;
    DWORD read = 0;
    DWORD avail = 0;
    auto deadline = GetTickCount64() + 1500;  // 1.5s (was 3s — shorter is safer for UI responsiveness)

    while (GetTickCount64() < deadline && !stop_.load()) {
        if (!PeekNamedPipe(hChildStdoutRead_, nullptr, 0, nullptr, &avail, nullptr)) {
            break; // Pipe broken — child died
        }
        if (avail > 0) {
            if (!ReadFile(hChildStdoutRead_, &ch, 1, &read, nullptr) || read == 0)
                break;
            if (ch == '\n') break;
            buf += ch;
        } else {
            Sleep(5);
        }
    }

    outJson = buf;
    return !buf.empty();
}

// ── JSON field extraction (no external dependency) ──────────────────────────

static std::string jsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static double jsonDouble(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return 0.0;
    pos += search.size();
    try { return std::stod(json.substr(pos)); } catch (...) { return 0.0; }
}

bool ScreenRuntime::parseScreenResult(const std::string& json, ScreenAnalysis& out) {
    if (json.find("\"ok\": true") == std::string::npos &&
        json.find("\"ok\":true") == std::string::npos) return false;
    if (json.find("\"type\": \"screen\"") == std::string::npos &&
        json.find("\"type\":\"screen\"") == std::string::npos) return false;

    out.brightness     = jsonDouble(json, "brightness");
    out.edgeDensity    = jsonDouble(json, "edge_density");
    out.activityLevel  = jsonString(json, "activity_level");
    out.activityDesc   = jsonString(json, "activity_description");
    out.dominantColour = jsonString(json, "dominant_colour");
    out.pixelHash      = jsonString(json, "pixel_hash");
    out.ocrText        = jsonString(json, "ocr_text");
    out.visionServerActive = true;
    return true;
}

// ── Win32 foreground window ──────────────────────────────────────────────────

void ScreenRuntime::readWin32ForegroundWindow(ScreenFrameSnapshot& frame) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;

    char title[512] = "";
    char cls[256] = "";
    GetWindowTextA(hwnd, title, sizeof(title));
    GetClassNameA(hwnd, cls, sizeof(cls));

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    frame.foregroundPid = pid;

    char procName[512] = "unknown";
    if (pid && pid != GetCurrentProcessId()) {
        HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hp) {
            GetModuleBaseNameA(hp, nullptr, procName, sizeof(procName));
            CloseHandle(hp);
        }
    } else if (pid == GetCurrentProcessId()) {
        strcpy_s(procName, "yuki.exe");
        strcpy_s(title, "Yuki Presence Shell");
    }

    frame.foregroundTitle   = title;
    frame.foregroundClass   = cls;
    frame.foregroundProcess = procName;
}

// ── Details builder ──────────────────────────────────────────────────────────

std::string ScreenRuntime::buildDetails(const ScreenFrameSnapshot& frame) const {
    std::ostringstream ss;
    ss << frame.width << "x" << frame.height << " screen";

    if (!frame.foregroundTitle.empty()) {
        ss << ". Focused: \"" << frame.foregroundTitle << "\" ("
           << frame.foregroundProcess << ")";
    }

    if (frame.analysis.visionServerActive) {
        ss << ". Brightness " << static_cast<int>(frame.analysis.brightness) << "/255";
        ss << ". " << frame.analysis.activityDesc;
        if (!frame.analysis.dominantColour.empty())
            ss << " Dominant colour: " << frame.analysis.dominantColour << ".";
        if (!frame.analysis.ocrText.empty())
            ss << " Text visible: \"" << frame.analysis.ocrText.substr(0, 80) << "\".";
    }

    if (frame.screenChanged) ss << " [CHANGED]";

    return ss.str();
}

// ── Main Capture Loop ────────────────────────────────────────────────────────

void ScreenRuntime::captureLoop() {
    try {
        startVisionServer();
    } catch (const std::exception& e) {
        std::cerr << "[ScreenRuntime] startVisionServer exception: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[ScreenRuntime] startVisionServer unknown exception.\n";
    }

    state_ = SubsystemRuntimeState::RUNNING;

    uint64_t lastVisionMs = 0;
    const uint64_t VISION_INTERVAL_MS = 1000;

    while (!stop_.load()) {
        try {
            if (!control_.isActive(SubsystemName::SCREEN_EYE)) {
                Sleep(200);
                continue;
            }

            ScreenFrameSnapshot frame;
            frame.width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            frame.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

            readWin32ForegroundWindow(frame);

            std::time_t t = std::time(nullptr);
            std::tm tm{};
            localtime_s(&tm, &t);
            std::ostringstream ts;
            ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            frame.timestamp = ts.str();

            uint64_t now = GetTickCount64();
            bool doVision = visionServerRunning_.load() &&
                            ((now - lastVisionMs >= VISION_INTERVAL_MS) ||
                             captureRequested_.exchange(false));

            if (doVision) {
                std::string jsonResult;
                if (sendCommand("{\"cmd\":\"screen\"}\n", jsonResult)) {
                    ScreenAnalysis analysis;
                    if (parseScreenResult(jsonResult, analysis)) {
                        frame.analysis  = analysis;
                        frame.pixelHash = analysis.pixelHash;
                        frame.screenChanged = (analysis.pixelHash != lastPixelHash_);
                        lastPixelHash_  = analysis.pixelHash;
                    }
                }
                lastVisionMs = GetTickCount64();
            } else {
                std::lock_guard<std::mutex> lock(dataMutex_);
                frame.analysis    = latestFrame_.analysis;
                frame.pixelHash   = latestFrame_.pixelHash;
                frame.screenChanged = false;
            }

            frame.metadata["window_title"]   = frame.foregroundTitle;
            frame.metadata["window_class"]   = frame.foregroundClass;
            frame.metadata["process_name"]   = frame.foregroundProcess;
            frame.metadata["width"]          = std::to_string(frame.width);
            frame.metadata["height"]         = std::to_string(frame.height);
            frame.metadata["brightness"]     = std::to_string(static_cast<int>(frame.analysis.brightness));
            frame.metadata["edge_density"]   = std::to_string(frame.analysis.edgeDensity);
            frame.metadata["activity"]       = frame.analysis.activityLevel;
            frame.metadata["dominant_color"] = frame.analysis.dominantColour;
            frame.metadata["screen_changed"] = frame.screenChanged ? "true" : "false";
            if (!frame.analysis.ocrText.empty())
                frame.metadata["ocr_text"]   = frame.analysis.ocrText;

            frame.details = buildDetails(frame);

            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                latestFrame_ = frame;
            }

            Sleep(FRAME_INTERVAL_MS);
        } catch (const std::exception& e) {
            std::cerr << "[ScreenRuntime] captureLoop iteration exception: " << e.what() << "\n";
            Sleep(yuki::config::kScreenCaptureDelayMs);
        } catch (...) {
            std::cerr << "[ScreenRuntime] captureLoop iteration unknown exception.\n";
            Sleep(yuki::config::kScreenCaptureDelayMs);
        }
    }

    try {
        stopVisionServer();
    } catch (...) {
        std::cerr << "[ScreenRuntime] stopVisionServer exception in captureLoop exit.\n";
    }
}
