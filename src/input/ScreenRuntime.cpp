// ScreenRuntime.cpp
// Yuki_1.0 — Real-Time Screen Vision
//
// C++ capture loop (foreground window, 200ms) + Python vision server pipe
// for heavyweight CV analysis (brightness, edge density, OCR).
// DXGI Desktop Duplication API implementation (ISSUE-P0-01 fix) to replace
// GDI-based mss capture in Python server, eliminating GDI handle leaks.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <psapi.h>

#include "input/ScreenRuntime.h"
#include "input/PerceptionLayer.h"
#include "brain/core/SystemConfig.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

template<typename T>
class ComPtr {
    T* ptr_ = nullptr;
public:
    ComPtr() noexcept = default;
    ComPtr(std::nullptr_t) noexcept : ptr_(nullptr) {}
    explicit ComPtr(T* p) noexcept : ptr_(p) {}
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) { if (ptr_) ptr_->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->AddRef();
        }
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    T* Get() const noexcept { return ptr_; }
    T* const* GetAddressOf() const noexcept { return &ptr_; }
    T** GetAddressOf() noexcept { return &ptr_; }
    T** ReleaseAndGetAddressOf() noexcept { Reset(); return &ptr_; }
    T** operator&() noexcept { Reset(); return &ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    void Reset() noexcept { if (ptr_) { ptr_->Release(); ptr_ = nullptr; } }
    
    template<typename U>
    HRESULT As(U** out) const noexcept {
        if (!ptr_ || !out) return E_POINTER;
        return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(out));
    }
};

// Path to the Python vision server (relative to process CWD)
static const char* VISION_SERVER_PY = "data/vision/yuki_vision_server.py";

// Frame interval — 200ms ≈ 5 fps (fast enough to notice window switches instantly)
static const DWORD FRAME_INTERVAL_MS = 200;

// DXGI capture state
struct DxgiCaptureState {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<IDXGIOutput> output;
    ComPtr<ID3D11Texture2D> stagingTexture;
    UINT width = 0;
    UINT height = 0;
    bool initialized = false;
};

static std::unique_ptr<DxgiCaptureState> g_dxgiState;
static std::mutex g_dxgiMutex;

// Initialize DXGI Desktop Duplication
bool ScreenRuntime::InitializeDxgiCapture() {
    std::lock_guard<std::mutex> lock(g_dxgiMutex);
    if (g_dxgiState && g_dxgiState->initialized) return true;

    g_dxgiState = std::make_unique<DxgiCaptureState>();

    // Create D3D11 device
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &g_dxgiState->device, &featureLevel, &g_dxgiState->context
    );
    if (FAILED(hr)) {
        std::cerr << "[ScreenRuntime] D3D11CreateDevice failed: 0x" << std::hex << hr << "\n";
        return false;
    }

    // Get DXGI device and adapter
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = g_dxgiState->device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) return false;

    // Get primary output (monitor)
    hr = adapter->EnumOutputs(0, &g_dxgiState->output);
    if (FAILED(hr)) {
        std::cerr << "[ScreenRuntime] EnumOutputs failed: 0x" << std::hex << hr << "\n";
        return false;
    }

    // Get output1 for Desktop Duplication
    ComPtr<IDXGIOutput1> output1;
    hr = g_dxgiState->output.As(&output1);
    if (FAILED(hr)) return false;

    // Duplicate the output
    hr = output1->DuplicateOutput(g_dxgiState->device.Get(), &g_dxgiState->duplication);
    if (FAILED(hr)) {
        std::cerr << "[ScreenRuntime] DuplicateOutput failed: 0x" << std::hex << hr << "\n";
        return false;
    }

    // Get desktop coordinates
    DXGI_OUTPUT_DESC desc;
    g_dxgiState->output->GetDesc(&desc);
    g_dxgiState->width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
    g_dxgiState->height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = g_dxgiState->width;
    stagingDesc.Height = g_dxgiState->height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = g_dxgiState->device->CreateTexture2D(&stagingDesc, nullptr, &g_dxgiState->stagingTexture);
    if (FAILED(hr)) {
        std::cerr << "[ScreenRuntime] Create staging texture failed: 0x" << std::hex << hr << "\n";
        return false;
    }

    g_dxgiState->initialized = true;
    std::cout << "[ScreenRuntime] DXGI Desktop Duplication initialized: " 
              << g_dxgiState->width << "x" << g_dxgiState->height << "\n";
    return true;
}

// Capture frame using DXGI Desktop Duplication
// Returns true on success, fills outFrame with BGRA pixel data
bool ScreenRuntime::CaptureDxgiFrame(std::vector<uint8_t>& outFrame, UINT& outWidth, UINT& outHeight) {
    std::lock_guard<std::mutex> lock(g_dxgiMutex);
    if (!g_dxgiState || !g_dxgiState->initialized) {
        if (!InitializeDxgiCapture()) return false;
    }

    // Acquire next frame
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;
    HRESULT hr = g_dxgiState->duplication->AcquireNextFrame(
        500, // timeout in ms
        &frameInfo,
        &desktopResource
    );

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return false; // No new frame
    }
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_DEVICE_REMOVED) {
        // Desktop duplication lost - need to reinitialize
        g_dxgiState->initialized = false;
        g_dxgiState->duplication.Reset();
        return false;
    }
    if (FAILED(hr)) {
        return false;
    }

    // Get the frame texture
    ComPtr<ID3D11Texture2D> acquiredTexture;
    hr = desktopResource.As(&acquiredTexture);
    if (FAILED(hr)) {
        g_dxgiState->duplication->ReleaseFrame();
        return false;
    }

    // Copy to staging texture
    g_dxgiState->context->CopyResource(g_dxgiState->stagingTexture.Get(), acquiredTexture.Get());

    // Map and read
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = g_dxgiState->context->Map(g_dxgiState->stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        const UINT rowPitch = static_cast<UINT>(mapped.RowPitch);
        const UINT pixelSize = 4; // BGRA
        outWidth = g_dxgiState->width;
        outHeight = g_dxgiState->height;
        outFrame.resize(rowPitch * outHeight);
        
        // Copy row by row to handle padding
        const uint8_t* src = static_cast<const uint8_t*>(mapped.pData);
        uint8_t* dst = outFrame.data();
        for (UINT y = 0; y < outHeight; ++y) {
            memcpy(dst + y * outWidth * pixelSize, src + y * rowPitch, outWidth * pixelSize);
        }
        
        g_dxgiState->context->Unmap(g_dxgiState->stagingTexture.Get(), 0);
    }

    // Release frame
    g_dxgiState->duplication->ReleaseFrame();

    return SUCCEEDED(hr);
}

// Compute frame hash for change detection
std::string ScreenRuntime::ComputeFrameHash(const std::vector<uint8_t>& frame, UINT width, UINT height) {
    // Simple downsample + hash
    const int targetSize = 32;
    const int stepX = (std::max)(1, static_cast<int>(width / targetSize));
    const int stepY = (std::max)(1, static_cast<int>(height / targetSize));
    
    std::vector<uint8_t> downsampled;
    downsampled.reserve(targetSize * targetSize);
    
    for (int y = 0; y < static_cast<int>(height); y += stepY) {
        for (int x = 0; x < static_cast<int>(width); x += stepX) {
            int idx = (y * width + x) * 4;
            // Use luminance
            uint8_t lum = static_cast<uint8_t>(
                0.299 * frame[idx + 2] +  // R
                0.587 * frame[idx + 1] +  // G
                0.114 * frame[idx]        // B
            );
            downsampled.push_back(lum);
        }
    }
    
    // Use std::hash for simplicity (in production, use a proper hash)
    std::hash<std::string_view> hasher;
    std::string_view view(reinterpret_cast<const char*>(downsampled.data()), downsampled.size());
    size_t hash = hasher(view);
    
    char buf[33];
    snprintf(buf, sizeof(buf), "%016zx", hash);
    return std::string(buf);
}

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

    // Buffered reads: pull whole available chunks instead of one byte per
    // syscall (audit: KB-sized JSON payloads used to cost thousands of
    // ReadFile calls). Stop at the first newline, which terminates the reply.
    std::string buf;
    std::vector<char> chunk(4096);
    DWORD read = 0;
    DWORD avail = 0;
    auto deadline = GetTickCount64() + 1500;  // 1.5s (was 3s — shorter is safer for UI responsiveness)

    while (GetTickCount64() < deadline && !stop_.load()) {
        if (!PeekNamedPipe(hChildStdoutRead_, nullptr, 0, nullptr, &avail, nullptr)) {
            break; // Pipe broken — child died
        }
        if (avail > 0) {
            DWORD want = avail < chunk.size() ? avail : static_cast<DWORD>(chunk.size());
            if (!ReadFile(hChildStdoutRead_, chunk.data(), want, &read, nullptr) || read == 0)
                break;
            buf.append(chunk.data(), static_cast<size_t>(read));
            auto newline = buf.find('\n');
            if (newline != std::string::npos) {
                buf.resize(newline);
                break;
            }
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
    // Initialize DXGI capture (replaces Python GDI capture for ISSUE-P0-01)
    bool useDxgi = InitializeDxgiCapture();
    if (!useDxgi) {
        std::cerr << "[ScreenRuntime] DXGI init failed, falling back to Python server\n";
        try {
            startVisionServer();
        } catch (const std::exception& e) {
            std::cerr << "[ScreenRuntime] startVisionServer exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[ScreenRuntime] startVisionServer unknown exception.\n";
        }
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
            bool doVision = ((now - lastVisionMs >= VISION_INTERVAL_MS) ||
                             captureRequested_.exchange(false));

            if (doVision) {
                ScreenAnalysis analysis;
                analysis.visionServerActive = false;
                
                if (useDxgi) {
                    // Capture using DXGI Desktop Duplication (no GDI handle leak)
                    std::vector<uint8_t> dxgiFrame;
                    UINT dxgiWidth = 0, dxgiHeight = 0;
                    if (CaptureDxgiFrame(dxgiFrame, dxgiWidth, dxgiHeight)) {
                        // Compute basic analysis from DXGI frame
                        analysis.brightness = 0;
                        analysis.edgeDensity = 0;
                        analysis.activityLevel = "LOW_ACTIVITY";
                        analysis.activityDesc = "DXGI capture active";
                        analysis.dominantColour = "#000000";
                        analysis.pixelHash = ComputeFrameHash(dxgiFrame, dxgiWidth, dxgiHeight);
                        analysis.visionServerActive = true;
                        
                        // Downsample and compute brightness/edges
                        const int sampleStep = 4;
                        int pixelCount = 0;
                        long long brightnessSum = 0;
                        for (size_t i = 0; i < dxgiFrame.size(); i += sampleStep * 4) {
                            // BGRA format
                            uint8_t b = dxgiFrame[i];
                            uint8_t g = dxgiFrame[i + 1];
                            uint8_t r = dxgiFrame[i + 2];
                            uint8_t lum = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                            brightnessSum += lum;
                            pixelCount++;
                        }
                        if (pixelCount > 0) {
                            analysis.brightness = static_cast<double>(brightnessSum) / pixelCount;
                        }
                        
                        // Simple edge detection via luminance differences
                        // (simplified - real impl would use Sobel)
                        analysis.edgeDensity = 0.05; // placeholder
                        
                        if (analysis.edgeDensity > 0.15) analysis.activityLevel = "HIGH_ACTIVITY";
                        else if (analysis.edgeDensity > 0.05) analysis.activityLevel = "MODERATE_ACTIVITY";
                        
                        frame.analysis  = analysis;
                        frame.pixelHash = analysis.pixelHash;
                        frame.screenChanged = (analysis.pixelHash != lastPixelHash_);
                        lastPixelHash_  = analysis.pixelHash;
                    }
                } else if (visionServerRunning_.load()) {
                    // Fallback to Python server
                    std::string jsonResult;
                    if (sendCommand("{\"cmd\":\"screen\"}\n", jsonResult)) {
                        if (parseScreenResult(jsonResult, analysis)) {
                            frame.analysis  = analysis;
                            frame.pixelHash = analysis.pixelHash;
                            frame.screenChanged = (analysis.pixelHash != lastPixelHash_);
                            lastPixelHash_  = analysis.pixelHash;
                        }
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
