// SubsystemControl.cpp
// Stage 3 — Yuki_1.0

#include "SubsystemControl.h"
#include "input/VisionSystem.h"
#include "input/PerceptionLayer.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <sstream>

// -------------------------------------------------
// String Helpers
// -------------------------------------------------

std::string toString(SubsystemName name) {
    switch (name) {
        case SubsystemName::EAR:        return "EAR";
        case SubsystemName::SCREEN_EYE: return "SCREEN_EYE";
        case SubsystemName::WORLD_EYE:  return "WORLD_EYE";
        case SubsystemName::BODY_STATE: return "BODY_STATE";
        case SubsystemName::MOUTH:      return "MOUTH";
    }
    return "UNKNOWN";
}

std::string toString(SubsystemMode mode) {
    switch (mode) {
        case SubsystemMode::AUTO:       return "AUTO";
        case SubsystemMode::FORCED_ON:  return "FORCED_ON";
        case SubsystemMode::FORCED_OFF: return "FORCED_OFF";
    }
    return "UNKNOWN";
}

std::string toString(SubsystemRuntimeState state) {
    switch (state) {
        case SubsystemRuntimeState::STOPPED:     return "STOPPED";
        case SubsystemRuntimeState::STARTING:    return "STARTING";
        case SubsystemRuntimeState::RUNNING:     return "RUNNING";
        case SubsystemRuntimeState::FAILED:      return "FAILED";
        case SubsystemRuntimeState::UNAVAILABLE: return "UNAVAILABLE";
    }
    return "UNKNOWN";
}

std::string toString(SttState state) {
    switch (state) {
        case SttState::STOPPED:             return "STOPPED";
        case SttState::STARTING:            return "STARTING";
        case SttState::LOADING_MODEL:       return "LOADING_MODEL";
        case SttState::READY:               return "READY";
        case SttState::LISTENING:           return "LISTENING";
        case SttState::CAPTURING_UTTERANCE: return "CAPTURING_UTTERANCE";
        case SttState::DECODING:            return "DECODING";
        case SttState::FAILED:              return "FAILED";
        case SttState::STOPPING:            return "STOPPING";
        default: return "UNKNOWN";
    }
}

// -------------------------------------------------
// SubsystemControl Implementation
// -------------------------------------------------

SubsystemControl::SubsystemControl() : changeCallback_(nullptr) {
    const SubsystemName all[] = {
        SubsystemName::EAR, SubsystemName::SCREEN_EYE, SubsystemName::WORLD_EYE,
        SubsystemName::BODY_STATE, SubsystemName::MOUTH
    };
    for (auto name : all) {
        SubsystemStatus s;
        s.name      = name;
        s.available = true;
        // All sensors start FORCED_OFF — user toggles them on via the shell buttons.
        // The MobileServer starts independently and is never controlled here.
        s.mode      = SubsystemMode::FORCED_OFF;
        subsystems_[name] = s;
    }
    refresh();
}

void SubsystemControl::setChangeCallback(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = cb;
}

void SubsystemControl::setRuntimeStateQuery(RuntimeStateQuery query) {
    std::lock_guard<std::mutex> lock(mutex_);
    runtimeStateQuery_ = query;
}

void SubsystemControl::setMode(SubsystemName name, SubsystemMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subsystems_[name].mode = mode;
    }
    refresh();
}

SubsystemMode SubsystemControl::getMode(SubsystemName name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_.at(name).mode;
}

void SubsystemControl::setAvailable(SubsystemName name, bool available) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subsystems_[name].available = available;
    }
    refresh();
}

bool SubsystemControl::isAvailable(SubsystemName name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_.at(name).available;
}

bool SubsystemControl::isActive(SubsystemName name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_.at(name).active;
}

SubsystemStatus SubsystemControl::getStatus(SubsystemName name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subsystems_.at(name);
}

std::vector<SubsystemStatus> SubsystemControl::getAllStatuses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SubsystemStatus> result;
    const SubsystemName order[] = {
        SubsystemName::EAR, SubsystemName::SCREEN_EYE, SubsystemName::WORLD_EYE,
        SubsystemName::BODY_STATE, SubsystemName::MOUTH
    };
    for (auto name : order) {
        result.push_back(subsystems_.at(name));
    }
    return result;
}

void SubsystemControl::refresh() {
    bool stateChanged = false;
    std::vector<std::pair<std::string, std::string>> pendingEvents;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 1. Refresh EAR (Mic)
        {
            auto& s = subsystems_[SubsystemName::EAR];
            bool oldActive = s.active;
            // NOTE: hardware detection happens once in AutoSensor.cpp at launch.
            // refresh() respects whatever available was explicitly set.
            // Only re-check if no mic was ever found (numDevs==0 = definite absence).
            if (!s.available) {
                UINT numDevs = waveInGetNumDevs();
                if (numDevs > 0) s.available = true;  // hardware appeared
            }
            if (s.mode == SubsystemMode::FORCED_OFF) {
                s.active = false;
                s.runtimeState = SubsystemRuntimeState::STOPPED;
                s.reason = "Mic blocked (desired state OFF).";
                s.lastError = "";
            } else if (s.mode == SubsystemMode::FORCED_ON) {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Forced on and available.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "Forced on but unavailable.";
                    s.lastError = "waveInGetNumDevs() returned 0 devices.";
                }
            } else {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Mic capture pipeline is active and streaming.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "No mic hardware detected.";
                    s.lastError = "waveInGetNumDevs() returned 0 devices.";
                }
            }
            if (s.active != oldActive) {
                stateChanged = true;
                pendingEvents.push_back({"MIC_STATE_CHANGED", s.active ? "ACTIVE" : "INACTIVE"});
            }
        }

        // 2. Refresh MOUTH (Speaker)
        {
            auto& s = subsystems_[SubsystemName::MOUTH];
            bool oldActive = s.active;
            
            // Query waveOut device count
            UINT numDevs = waveOutGetNumDevs();
            if (numDevs == 0) {
                s.available = false;
            }
            
            if (s.mode == SubsystemMode::FORCED_OFF) {
                s.active = false;
                s.runtimeState = SubsystemRuntimeState::STOPPED;
                s.reason = "Speaker muted (desired state OFF).";
                s.lastError = "";
            } else if (s.mode == SubsystemMode::FORCED_ON) {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Forced on and available.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "Forced on but unavailable.";
                    s.lastError = "waveOutGetNumDevs() returned 0 devices.";
                }
            } else {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Speaker playback pipeline is active.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "No playback hardware detected.";
                    s.lastError = "waveOutGetNumDevs() returned 0 devices.";
                }
            }
            if (s.active != oldActive) {
                stateChanged = true;
                pendingEvents.push_back({"SPEAKER_STATE_CHANGED", s.active ? "ACTIVE" : "INACTIVE"});
            }
        }

        // 3. Refresh WORLD_EYE (Camera)
        {
            auto& s = subsystems_[SubsystemName::WORLD_EYE];
            bool oldActive = s.active;
            // NOTE: isCameraHardwarePresent() calls CoInitializeEx which is
            // expensive and threading-sensitive.  Detection is done ONCE in
            // AutoSensor.cpp.  Here we just respect what was set.
            // (If someone toggles the camera off and back on, available stays
            //  as AutoSensor left it.)
            if (s.mode == SubsystemMode::FORCED_OFF) {
                s.active = false;
                s.runtimeState = SubsystemRuntimeState::STOPPED;
                s.reason = "Camera disabled (desired state OFF).";
                s.lastError = "";
            } else if (s.mode == SubsystemMode::FORCED_ON) {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Forced on and available.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "Forced on but unavailable.";
                    s.lastError = "DirectShow CreateClassEnumerator returned no video capture devices.";
                }
            } else {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Camera is actively streaming video frames.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::UNAVAILABLE;
                    s.reason = "Camera hardware is not present.";
                    s.lastError = "DirectShow CreateClassEnumerator returned no video capture devices.";
                }
            }
            if (s.active != oldActive) {
                stateChanged = true;
                pendingEvents.push_back({"CAMERA_STATE_CHANGED", s.active ? "ACTIVE" : "INACTIVE"});
            }
        }

        // 4. Refresh SCREEN_EYE (Screen)
        {
            auto& s = subsystems_[SubsystemName::SCREEN_EYE];
            bool oldActive = s.active;
            
            HWND hwndDesktop = GetDesktopWindow();
            if (hwndDesktop == NULL) {
                s.available = false;
            }
            
            if (s.mode == SubsystemMode::FORCED_OFF) {
                s.active = false;
                s.runtimeState = SubsystemRuntimeState::STOPPED;
                s.reason = "Screen perception disabled (desired state OFF).";
                s.lastError = "";
            } else if (s.mode == SubsystemMode::FORCED_ON) {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Forced on and available.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::FAILED;
                    s.reason = "Forced on but unavailable.";
                    s.lastError = "GetDesktopWindow() failed.";
                }
            } else {
                if (s.available) {
                    s.active = true;
                    s.runtimeState = SubsystemRuntimeState::RUNNING;
                    s.reason = "Desktop BitBlt capture is running.";
                    s.lastError = "";
                } else {
                    s.active = false;
                    s.runtimeState = SubsystemRuntimeState::FAILED;
                    s.reason = "Desktop window handle is NULL.";
                    s.lastError = "GetDesktopWindow() failed.";
                }
            }
            if (s.active != oldActive) {
                stateChanged = true;
                pendingEvents.push_back({"SCREEN_STATE_CHANGED", s.active ? "ACTIVE" : "INACTIVE"});
            }
        }

        // 5. Refresh BODY_STATE (Telemetry)
        {
            auto& s = subsystems_[SubsystemName::BODY_STATE];
            bool oldActive = s.active;
            s.available = true;
            if (s.mode == SubsystemMode::FORCED_OFF) {
                s.active = false;
                s.runtimeState = SubsystemRuntimeState::STOPPED;
                s.reason = "Telemetry engine disabled (desired state OFF).";
            } else if (s.mode == SubsystemMode::FORCED_ON) {
                s.active = true;
                s.runtimeState = SubsystemRuntimeState::RUNNING;
                s.reason = "Forced on and available.";
            } else {
                s.active = true;
                s.runtimeState = SubsystemRuntimeState::RUNNING;
                s.reason = "Telemetry engine is active.";
            }
            if (s.active != oldActive) {
                stateChanged = true;
                pendingEvents.push_back({"TELEMETRY_STATE_CHANGED", s.active ? "ACTIVE" : "INACTIVE"});
            }
        }

        // Query live runtime states if callback is registered
        if (runtimeStateQuery_) {
            for (auto& pair : subsystems_) {
                auto& s = pair.second;
                if (s.active) {
                    s.runtimeState = runtimeStateQuery_(s.name);
                }
            }
        }
    }

    // Submit pending events to Perception Unified Layer outside the lock
    for (const auto& ev : pendingEvents) {
        UnifiedPerceptionLayer::instance().submitEvent(
            UnifiedPerceptionLayer::fromInternal(ev.first, ev.second, "")
        );
    }

    // Notify listeners outside the lock to prevent deadlock
    notifyListeners();
}

void SubsystemControl::notifyListeners() {
    ChangeCallback cb = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = changeCallback_;
    }
    if (cb) {
        cb();
    }
}

// -------------------------------------------------
// Subsystem Specific Mutators & Accessors (Helper Methods)
// -------------------------------------------------

void SubsystemControl::setMicEnabled(bool enabled) {
    setMode(SubsystemName::EAR, enabled ? SubsystemMode::AUTO : SubsystemMode::FORCED_OFF);
    refresh();
}

void SubsystemControl::toggleMic() {
    setMicEnabled(!isMicEnabled());
}

bool SubsystemControl::isMicEnabled() const {
    return getMode(SubsystemName::EAR) != SubsystemMode::FORCED_OFF;
}

void SubsystemControl::setSpeakerEnabled(bool enabled) {
    setMode(SubsystemName::MOUTH, enabled ? SubsystemMode::AUTO : SubsystemMode::FORCED_OFF);
    refresh();
}

void SubsystemControl::toggleSpeaker() {
    setSpeakerEnabled(!isSpeakerEnabled());
}

bool SubsystemControl::isSpeakerEnabled() const {
    return getMode(SubsystemName::MOUTH) != SubsystemMode::FORCED_OFF;
}

void SubsystemControl::setCameraEnabled(bool enabled) {
    if (enabled) {
        setMode(SubsystemName::WORLD_EYE, SubsystemMode::AUTO);
        setAvailable(SubsystemName::WORLD_EYE, true);
    } else {
        setMode(SubsystemName::WORLD_EYE, SubsystemMode::FORCED_OFF);
    }
    refresh();
}

void SubsystemControl::toggleCamera() {
    setCameraEnabled(!isCameraEnabled());
}

bool SubsystemControl::isCameraEnabled() const {
    return getMode(SubsystemName::WORLD_EYE) != SubsystemMode::FORCED_OFF;
}

void SubsystemControl::setScreenEnabled(bool enabled) {
    if (enabled) {
        setMode(SubsystemName::SCREEN_EYE, SubsystemMode::AUTO);
        setAvailable(SubsystemName::SCREEN_EYE, true);
    } else {
        setMode(SubsystemName::SCREEN_EYE, SubsystemMode::FORCED_OFF);
    }
    refresh();
}

void SubsystemControl::toggleScreen() {
    setScreenEnabled(!isScreenEnabled());
}

bool SubsystemControl::isScreenEnabled() const {
    return getMode(SubsystemName::SCREEN_EYE) != SubsystemMode::FORCED_OFF;
}

void SubsystemControl::setSttState(SttState state) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sttState_ = state;
    }
    notifyListeners();
}

SttState SubsystemControl::getSttState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sttState_;
}

std::string SubsystemControl::getCompactStatusString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << "[MIC: " << (subsystems_.at(SubsystemName::EAR).active ? "ON" : "OFF")
       << " | SPK: " << (subsystems_.at(SubsystemName::MOUTH).active ? "ON" : "OFF")
       << " | CAM: " << (subsystems_.at(SubsystemName::WORLD_EYE).active ? "ON" : "OFF")
       << " | SCR: " << (subsystems_.at(SubsystemName::SCREEN_EYE).active ? "ON" : "OFF")
       << "]";
    return ss.str();
}
