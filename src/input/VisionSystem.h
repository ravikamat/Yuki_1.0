#pragma once
// VisionSystem.h — Screen perception + vision management (merged from ScreenEye + VisionManager)
#include "SubsystemControl.h"
#include <string>
#include <mutex>

class CameraRuntime;
class ScreenRuntime;

// ── §ScreenEye ─────────────────────────────────────────────────────────────────
struct ScreenSnapshot {
    bool allowed             = false;
    bool subsystem_available = false;
    bool subsystem_active    = false;
    bool foreground_window_present = false;
    std::string foreground_window_title;
    std::string foreground_window_class;
    std::string foreground_process_name;
    int  screen_width  = 0;
    int  screen_height = 0;
    std::string summary;
};

class ScreenEyeReader {
public:
    ScreenSnapshot capture(const SubsystemControl& control) const;
};

// ── §VisionManager ─────────────────────────────────────────────────────────────
enum class VisionMode { NONE, CAMERA, SCREEN };

struct VisionResult {
    VisionMode  mode        = VisionMode::NONE;
    std::string timestamp;
    std::string status;
    std::string details;
    int         frameWidth  = 0;
    int         frameHeight = 0;
    unsigned int pixelHash  = 0;
};

class VisionManager {
public:
    static VisionManager& instance();
    VisionManager();
    ~VisionManager();
    void       initialize(SubsystemControl* control, CameraRuntime* camera, ScreenRuntime* screen);
    void       setMode(VisionMode mode);
    VisionMode getMode() const;
    bool       isCameraActive() const;
    bool       isScreenActive() const;
    bool       isCameraHardwarePresent() const;
    VisionResult getLatestResult();
    void tick();
    void captureScreenExplicit();
    void captureCameraExplicit();
private:
    SubsystemControl* control_ = nullptr;
    CameraRuntime*    camera_  = nullptr;
    ScreenRuntime*    screen_  = nullptr;
    mutable std::mutex mutex_;
    std::string lastCameraHash_;
    std::string lastScreenHash_;
};

inline VisionManager& vision() { return VisionManager::instance(); }
