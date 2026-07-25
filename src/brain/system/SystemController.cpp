#include "brain/system/SystemController.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/security/ApprovalGate.h"
#include "brain/system/ResourceMonitor.h"
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace yuki::system {

SystemController::SystemController(yuki::security::SecuritySandbox* sandbox,
                                   yuki::security::ApprovalGate* gate,
                                   yuki::system::ResourceMonitor* monitor)
    : sandbox_(sandbox), gate_(gate), monitor_(monitor) {}

SystemController::~SystemController() = default;

void SystemController::setSandbox(yuki::security::SecuritySandbox* sandbox) {
    std::lock_guard<std::mutex> lock(mutex_);
    sandbox_ = sandbox;
}

void SystemController::setApprovalGate(yuki::security::ApprovalGate* gate) {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_ = gate;
}

void SystemController::setResourceMonitor(yuki::system::ResourceMonitor* monitor) {
    std::lock_guard<std::mutex> lock(mutex_);
    monitor_ = monitor;
}

float SystemController::clamp01(float v) const {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

bool SystemController::validatePath(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "Path is empty";
        return false;
    }
    if (sandbox_) {
        auto decision = sandbox_->validateWrite(path);
        if (!decision.allowed()) {
            error = "SecuritySandbox denied path write";
            return false;
        }
    }
    return true;
}

bool SystemController::validateUrl(const std::string& url, std::string& error) {
    if (url.empty()) {
        error = "URL is empty";
        return false;
    }
    if (url.find("javascript:") == 0 || url.find("file://") == 0) {
        error = "Disallowed URL scheme";
        return false;
    }
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        error = "URL must start with http:// or https://";
        return false;
    }
    return true;
}

bool SystemController::screenshot(const std::string& path, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validatePath(path, error)) return false;

    if (gate_ && !gate_->requestApproval("screenshot", 0.60f)) {
        error = "ApprovalGate denied screenshot execution";
        return false;
    }

#ifdef _WIN32
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    DeleteObject(hBitmap);
#endif

    std::ofstream dummy(path, std::ios::binary);
    if (!dummy) {
        error = "Failed to open target path for writing";
        return false;
    }
    dummy << "BMP_HEADER_DUMMY_SCREENSHOT_DATA";
    return true;
}

bool SystemController::setVolume(float level) {
    std::lock_guard<std::mutex> lock(mutex_);
    volume_level_ = clamp01(level);
    muted_ = false;
    return true;
}

bool SystemController::mute() {
    std::lock_guard<std::mutex> lock(mutex_);
    muted_ = true;
    return true;
}

bool SystemController::unmute() {
    std::lock_guard<std::mutex> lock(mutex_);
    muted_ = false;
    return true;
}

bool SystemController::setClipboardText(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    clipboard_data_ = text;
#ifdef _WIN32
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                std::memcpy(pMem, text.c_str(), text.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        CloseClipboard();
    }
#endif
    return true;
}

bool SystemController::getClipboardText(std::string& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    out = clipboard_data_;
    return true;
}

bool SystemController::openUrl(const std::string& url, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!validateUrl(url, error)) return false;

    if (gate_ && !gate_->requestApproval("openUrl:" + url, 0.40f)) {
        error = "ApprovalGate denied openUrl";
        return false;
    }

#ifdef _WIN32
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
    return true;
}

bool SystemController::openApplication(const std::string& app_name, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (app_name.empty()) {
        error = "App name is empty";
        return false;
    }

    if (sandbox_) {
        auto decision = sandbox_->validateExecute(app_name);
        if (!decision.allowed()) {
            error = "SecuritySandbox denied app execution";
            return false;
        }
    }

    if (gate_ && !gate_->requestApproval("openApp:" + app_name, 0.70f)) {
        error = "ApprovalGate denied application launch";
        return false;
    }

#ifdef _WIN32
    ShellExecuteA(NULL, "open", app_name.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
    return true;
}

SystemController::MetricsSnapshot SystemController::getMetricsSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    MetricsSnapshot snap;
    snap.volume_level = muted_ ? 0.0f : volume_level_;
    if (monitor_) {
        auto h = monitor_->sampleMetrics();
        snap.cpu_percent = clamp01(h.cpuPercent / 100.0f);
        snap.ram_percent = clamp01(h.ramUsedMb / h.ramTotalMb);
        snap.disk_percent = clamp01(h.diskIoRate / 100.0f);
    }
    return snap;
}

} // namespace yuki::system
