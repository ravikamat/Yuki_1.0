// VisionSystem.cpp — Screen perception + vision management (merged)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <wininet.h>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#include "input/VisionSystem.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

// ══════════════════════════════════════════════════════════════════════════════
// ScreenEyeReader
// ══════════════════════════════════════════════════════════════════════════════

ScreenSnapshot ScreenEyeReader::capture(const SubsystemControl& control) const {
    ScreenSnapshot snap;
    try {
        SubsystemStatus status = control.getStatus(SubsystemName::SCREEN_EYE);
        snap.allowed = status.active; snap.subsystem_available = status.available; snap.subsystem_active = status.active;
        if (!snap.subsystem_active) { snap.summary = "ScreenEye blocked or unavailable."; return snap; }

        snap.screen_width  = GetSystemMetrics(SM_CXSCREEN);
        snap.screen_height = GetSystemMetrics(SM_CYSCREEN);

        HWND hwnd = GetForegroundWindow();
        if (hwnd == NULL) {
            snap.foreground_window_present = false;
            snap.summary = "Screen active but no foreground window detected.";
            return snap;
        }

        snap.foreground_window_present = true;
        DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
        char title[512] = "";
        if (pid == GetCurrentProcessId()) strcpy_s(title, "Yuki Presence Shell");
        else GetWindowTextA(hwnd, title, sizeof(title));
        snap.foreground_window_title = title;
        char className[256] = "";
        if (GetClassNameA(hwnd, className, sizeof(className)) > 0) snap.foreground_window_class = className;
        if (pid == GetCurrentProcessId()) { snap.foreground_process_name = "yuki.exe"; }
        else {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
            if (hProcess != NULL) {
                char processName[MAX_PATH] = "";
                if (GetModuleBaseNameA(hProcess, NULL, processName, sizeof(processName)) > 0)
                    snap.foreground_process_name = processName;
                CloseHandle(hProcess);
            }
        }
        std::ostringstream ss;
        ss << "Screen focused: ";
        if      (!snap.foreground_window_title.empty())   ss << snap.foreground_window_title;
        else if (!snap.foreground_process_name.empty())   ss << snap.foreground_process_name;
        else                                              ss << "foreground window detected";
        ss << " on " << snap.screen_width << "x" << snap.screen_height << " display.";
        snap.summary = ss.str();
    }
    catch (const std::exception& e) {
        std::cerr << "[ScreenEyeReader] capture exception: " << e.what() << "\n";
        snap.foreground_window_present = false;
    }
    catch (...) {
        std::cerr << "[ScreenEyeReader] capture unknown exception\n";
        snap.foreground_window_present = false;
    }
    return snap;
}


// ══════════════════════════════════════════════════════════════════════════════
// VisionManager
// ══════════════════════════════════════════════════════════════════════════════

VisionManager& VisionManager::instance() { static VisionManager inst; return inst; }
VisionManager::VisionManager()  {}
VisionManager::~VisionManager() {}

void VisionManager::initialize(SubsystemControl* control, CameraRuntime* camera, ScreenRuntime* screen) {
    std::lock_guard<std::mutex> lock(mutex_);
    control_=control; camera_=camera; screen_=screen;
}

void VisionManager::setMode(VisionMode mode) {
    std::lock_guard<std::mutex> lock(mutex_); if (!control_) return;
    if (mode==VisionMode::CAMERA) { control_->setMode(SubsystemName::WORLD_EYE,SubsystemMode::AUTO); control_->setAvailable(SubsystemName::WORLD_EYE,true); }
    else if (mode==VisionMode::SCREEN) { control_->setMode(SubsystemName::SCREEN_EYE,SubsystemMode::AUTO); control_->setAvailable(SubsystemName::SCREEN_EYE,true); }
    else { control_->setMode(SubsystemName::WORLD_EYE,SubsystemMode::FORCED_OFF); control_->setMode(SubsystemName::SCREEN_EYE,SubsystemMode::FORCED_OFF); }
    control_->refresh();
}

VisionMode VisionManager::getMode() const {
    std::lock_guard<std::mutex> lock(mutex_); if (!control_) return VisionMode::NONE;
    if (control_->isActive(SubsystemName::WORLD_EYE))  return VisionMode::CAMERA;
    if (control_->isActive(SubsystemName::SCREEN_EYE)) return VisionMode::SCREEN;
    return VisionMode::NONE;
}

bool VisionManager::isCameraActive() const { return getMode()==VisionMode::CAMERA; }
bool VisionManager::isScreenActive() const { return getMode()==VisionMode::SCREEN; }

bool VisionManager::isCameraHardwarePresent() const {
    bool found = false;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ICreateDevEnum* pDevEnum = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum,NULL,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&pDevEnum)))) {
        IEnumMoniker* pEnum = NULL;
        if (SUCCEEDED(pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&pEnum,0))&&pEnum) {
            IMoniker* pMon=NULL; if (pEnum->Next(1,&pMon,NULL)==S_OK){found=true;pMon->Release();}
            pEnum->Release();
        }
        pDevEnum->Release();
    }
    CoUninitialize(); return found;
}

VisionResult VisionManager::getLatestResult() {
    std::lock_guard<std::mutex> lock(mutex_);
    VisionResult res; res.mode=VisionMode::NONE;
    if (control_) {
        if (control_->isActive(SubsystemName::WORLD_EYE))  res.mode=VisionMode::CAMERA;
        else if (control_->isActive(SubsystemName::SCREEN_EYE)) res.mode=VisionMode::SCREEN;
    }
    std::time_t t=std::time(nullptr); std::tm tm; localtime_s(&tm,&t);
    std::ostringstream oss; oss<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S"); res.timestamp=oss.str();
    if (res.mode==VisionMode::CAMERA&&camera_) {
        CameraFrameSnapshot frame=camera_->getLatestFrame();
        res.frameWidth=frame.width; res.frameHeight=frame.height; res.pixelHash=0;
        res.status=frame.analysis.hardwarePresent?"CAMERA_CONNECTED":"NO_HARDWARE"; res.details=frame.details;
    } else if (res.mode==VisionMode::SCREEN&&screen_) {
        ScreenFrameSnapshot frame=screen_->getLatestFrame();
        res.frameWidth=frame.width; res.frameHeight=frame.height; res.pixelHash=0;
        res.status="SUCCESS"; res.details=frame.details;
    } else {
        res.mode=VisionMode::NONE; res.status="IDLE";
        res.details="Perception channel stands ready. Use 'camera on' or 'screen on'.";
        res.frameWidth=res.frameHeight=res.pixelHash=0;
    }
    return res;
}

void VisionManager::tick()                {}
void VisionManager::captureScreenExplicit()  {}
void VisionManager::captureCameraExplicit()  {}
