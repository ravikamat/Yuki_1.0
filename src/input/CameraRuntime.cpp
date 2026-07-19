// CameraRuntime.cpp
// Yuki_1.0 — Real-Time Camera Vision

#include "input/CameraRuntime.h"
#include "input/PerceptionLayer.h"
#include "input/VisionSystem.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

static const char* VISION_SERVER_PY   = "data/vision/yuki_vision_server.py";
static const DWORD FRAME_INTERVAL_MS  = 100;  // 10 fps polling rate

// ── Constructor / Destructor ────────────────────────────────────────────────

CameraRuntime::CameraRuntime(SubsystemControl& control)
    : control_(control),
      state_(SubsystemRuntimeState::STOPPED) {}

CameraRuntime::~CameraRuntime() { stop(); }

// ── Public API ──────────────────────────────────────────────────────────────

void CameraRuntime::start() {
    if (!stop_.load() && worker_.joinable()) return;
    
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        deviceName_ = "Initializing camera sensor...";
        latestFrame_.details = "Camera initializing — vision server starting.";
    }
    stop_.store(false);
    state_ = SubsystemRuntimeState::STARTING;

    std::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    worker_ = std::thread([this, p = std::move(readyPromise)]() mutable {
        p.set_value();
        this->captureLoop();
    });

    if (readyFuture.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }
}

void CameraRuntime::stop() {
    stop_.store(true);
    stopVisionServer();
    if (worker_.joinable()) worker_.join();
    state_ = SubsystemRuntimeState::STOPPED;
}

SubsystemRuntimeState CameraRuntime::reportState() const { return state_.load(); }

CameraFrameSnapshot CameraRuntime::getLatestFrame() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestFrame_;
}

std::string CameraRuntime::getDeviceName() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return deviceName_;
}

bool CameraRuntime::isVisionServerActive() const {
    return visionServerRunning_.load();
}

// ── Python Vision Server ─────────────────────────────────────────────────────

void CameraRuntime::startVisionServer() {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;

    HANDLE hStdinRead  = INVALID_HANDLE_VALUE;
    HANDLE hStdoutWrite= INVALID_HANDLE_VALUE;

    if (!CreatePipe(&hStdinRead,  &hChildStdinWrite_,  &sa, 0) ||
        !CreatePipe(&hChildStdoutRead_, &hStdoutWrite, &sa, 0)) {
        std::cerr << "[CameraRuntime] Pipe creation failed.\n";
        return;
    }
    SetHandleInformation(hChildStdinWrite_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hChildStdoutRead_, HANDLE_FLAG_INHERIT, 0);

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
        std::cerr << "[CameraRuntime] Failed to launch vision server. Error="
                  << GetLastError() << "\n";
        CloseHandle(hStdinRead); CloseHandle(hStdoutWrite);
        return;
    }

    CloseHandle(hStdinRead); CloseHandle(hStdoutWrite);
    CloseHandle(pi.hThread);
    hProcess_ = pi.hProcess;

    // Verify process didn't immediately exit
    DWORD waitResult = WaitForSingleObject(hProcess_, 2000); // 2s grace
    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess_, &exitCode);
        CloseHandle(hProcess_);
        hProcess_ = INVALID_HANDLE_VALUE;
        std::cerr << "[Camera] Vision server exited immediately (code " << exitCode << ")\n";
        return;
    }

    // Drain the ready line
    std::string ready;
    sendCommand("", ready);
    visionServerRunning_ = true;
    std::cout << "[CameraRuntime] Vision server online.\n";
}

void CameraRuntime::stopVisionServer() {
    if (!visionServerRunning_.load()) return;
    visionServerRunning_ = false;
    std::string unused;
    sendCommand("{\"cmd\":\"quit\"}\n", unused);
    if (hChildStdinWrite_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hChildStdinWrite_); hChildStdinWrite_ = INVALID_HANDLE_VALUE;
    }
    if (hChildStdoutRead_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hChildStdoutRead_); hChildStdoutRead_ = INVALID_HANDLE_VALUE;
    }
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(hProcess_, 1000);
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_); hProcess_ = INVALID_HANDLE_VALUE;
    }
}

bool CameraRuntime::sendCommand(const std::string& jsonCmd, std::string& outJson) {
    std::lock_guard<std::mutex> lock(pipeMutex_);

    if (!jsonCmd.empty() && hChildStdinWrite_ != INVALID_HANDLE_VALUE) {
        std::string line = jsonCmd;
        if (line.back() != '\n') line += '\n';
        DWORD written = 0;
        if (!WriteFile(hChildStdinWrite_, line.c_str(),
                       static_cast<DWORD>(line.size()), &written, nullptr))
            return false;
    }

    if (hChildStdoutRead_ == INVALID_HANDLE_VALUE) return false;

    std::string buf;
    char ch = 0; DWORD read = 0;
    auto deadline = GetTickCount64() + 5000; // 5s timeout for camera (init is slow)

    while (GetTickCount64() < deadline) {
        DWORD avail = 0;
        if (!PeekNamedPipe(hChildStdoutRead_, nullptr, 0, nullptr, &avail, nullptr)) break;
        if (avail > 0) {
            if (!ReadFile(hChildStdoutRead_, &ch, 1, &read, nullptr) || read == 0) break;
            if (ch == '\n') break;
            buf += ch;
        } else {
            Sleep(5);
        }
    }

    outJson = buf;
    return !buf.empty();
}

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string jStr(const std::string& json, const std::string& key) {
    std::string s = "\"" + key + "\":\"";
    auto pos = json.find(s);
    if (pos == std::string::npos) return "";
    pos += s.size();
    auto end = json.find('"', pos);
    return (end != std::string::npos) ? json.substr(pos, end - pos) : "";
}

static double jDbl(const std::string& json, const std::string& key) {
    std::string s = "\"" + key + "\":";
    auto pos = json.find(s);
    if (pos == std::string::npos) return 0.0;
    pos += s.size();
    try { return std::stod(json.substr(pos)); } catch (...) { return 0.0; }
}

static bool jBool(const std::string& json, const std::string& key) {
    std::string s = "\"" + key + "\":";
    auto pos = json.find(s);
    if (pos == std::string::npos) return false;
    pos += s.size();
    return json.substr(pos, 4) == "true";
}

static int jInt(const std::string& json, const std::string& key) {
    return static_cast<int>(jDbl(json, key));
}

bool CameraRuntime::parseCameraResult(const std::string& json, CameraAnalysis& out) {
    if (json.find("\"ok\": true") == std::string::npos &&
        json.find("\"ok\":true") == std::string::npos) return false;
    if (json.find("\"type\": \"camera\"") == std::string::npos &&
        json.find("\"type\":\"camera\"") == std::string::npos) return false;

    out.hardwarePresent   = jBool(json, "hardware_present");
    out.brightness        = jDbl(json,  "brightness");
    out.lighting          = jStr(json,  "lighting");
    out.faceCount         = jInt(json,  "face_count");
    out.motionDetected    = jBool(json, "motion_detected");
    out.pixelHash         = jStr(json,  "pixel_hash");
    out.visionServerActive= true;

    // Parse face_positions array — find all quoted strings inside it
    auto arrStart = json.find("\"face_positions\":");
    if (arrStart != std::string::npos) {
        auto bracket = json.find('[', arrStart);
        auto bracket_end = json.find(']', bracket);
        if (bracket != std::string::npos && bracket_end != std::string::npos) {
            std::string arr = json.substr(bracket, bracket_end - bracket);
            size_t p = 0;
            while (true) {
                auto q1 = arr.find('"', p);
                if (q1 == std::string::npos) break;
                auto q2 = arr.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                FaceDetection fd;
                fd.position = arr.substr(q1 + 1, q2 - q1 - 1);
                out.faces.push_back(fd);
                p = q2 + 1;
            }
        }
    }

    return true;
}

// ── Details builder ──────────────────────────────────────────────────────────

std::string CameraRuntime::buildDetails(const CameraFrameSnapshot& frame) const {
    const auto& a = frame.analysis;
    std::ostringstream ss;

    if (!a.hardwarePresent) {
        ss << "No camera hardware detected.";
        return ss.str();
    }

    ss << "Camera " << frame.width << "x" << frame.height << ". ";

    if (!a.lighting.empty()) ss << a.lighting << ". ";

    if (a.faceCount == 0) {
        ss << "No faces in view.";
    } else if (a.faceCount == 1) {
        ss << "1 face detected";
        if (!a.faces.empty()) ss << " (" << a.faces[0].position << " of frame)";
        ss << ".";
    } else {
        ss << a.faceCount << " faces detected.";
    }

    if (a.motionDetected) ss << " Motion detected.";

    return ss.str();
}

// ── Main Capture Loop ────────────────────────────────────────────────────────

void CameraRuntime::captureLoop() {
    // First check if hardware exists (fast probe)
    bool hwPresent = vision().isCameraHardwarePresent();
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        deviceName_ = hwPresent ? "Webcam (OpenCV)" : "No camera hardware";
    }

    startVisionServer();
    state_ = SubsystemRuntimeState::RUNNING;

    uint64_t lastFrameMs = 0;
    const uint64_t CAPTURE_INTERVAL_MS = hwPresent ? 100 : 1000; // 10fps if hardware, 1fps poll otherwise

    while (!stop_.load()) {
        if (!control_.isActive(SubsystemName::WORLD_EYE)) {
            Sleep(200);
            continue;
        }

        uint64_t now = GetTickCount64();
        if (now - lastFrameMs < CAPTURE_INTERVAL_MS) {
            Sleep(10);
            continue;
        }
        lastFrameMs = now;

        CameraFrameSnapshot frame;
        frame.width  = 640;
        frame.height = 480;

        // Timestamp
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        frame.timestamp = ts.str();

        // Request camera analysis from Python server
        if (visionServerRunning_.load()) {
            std::string jsonResult;
            if (sendCommand("{\"cmd\":\"camera\"}\n", jsonResult)) {
                CameraAnalysis analysis;
                if (parseCameraResult(jsonResult, analysis)) {
                    frame.analysis = analysis;
                    if (analysis.hardwarePresent) {
                        std::lock_guard<std::mutex> lock(dataMutex_);
                        deviceName_ = "Webcam (OpenCV @ 640x480)";
                    }
                }
            }
        }

        frame.details = buildDetails(frame);

        // Metadata
        frame.metadata["hardware_present"] = frame.analysis.hardwarePresent ? "true" : "false";
        frame.metadata["face_count"]       = std::to_string(frame.analysis.faceCount);
        frame.metadata["motion"]           = frame.analysis.motionDetected ? "true" : "false";
        frame.metadata["brightness"]       = std::to_string(static_cast<int>(frame.analysis.brightness));
        frame.metadata["lighting"]         = frame.analysis.lighting;
        frame.metadata["pixel_hash"]       = frame.analysis.pixelHash;

        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            latestFrame_ = frame;
        }

        // Emit perception event — only if hardware present or motion change
#ifdef YUKI_DIRECT_PERCEPTION_EMIT
        // DEPRECATED: Direct emission bypasses SignalConditioningLayer.
        if (frame.analysis.hardwarePresent || frame.analysis.motionDetected) {
            UnifiedPerceptionLayer::instance().submitEvent(
                UnifiedPerceptionLayer::fromCamera(frame.details, frame.metadata, "WORLD_EYE:ON")
            );
        }
#endif
    }

    stopVisionServer();
}
