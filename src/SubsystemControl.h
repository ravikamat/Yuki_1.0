#pragma once

// SubsystemControl.h
// Stage 3 — Yuki_1.0
//
// Central switchboard for controlling Yuki's major functional units.
// Allows manual overrides (FORCED_ON/OFF) or automatic management (AUTO).
// Evolved to support thread-safety and dynamic state synchronization.

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>

// Major functional subsystems planned for Yuki_1.0
enum class SubsystemName {
    EAR,
    SCREEN_EYE,
    WORLD_EYE,
    BODY_STATE,
    MOUTH
};

enum class SubsystemMode {
    AUTO,
    FORCED_ON,
    FORCED_OFF
};

enum class SubsystemRuntimeState {
    STOPPED,
    STARTING,
    RUNNING,
    FAILED,
    UNAVAILABLE
};

enum class SttState {
    STOPPED,
    STARTING,
    LOADING_MODEL,
    READY,
    LISTENING,
    CAPTURING_UTTERANCE,
    DECODING,
    FAILED,
    STOPPING
};

// Current status snapshot of a subsystem
struct SubsystemStatus {
    SubsystemName name;
    SubsystemMode mode = SubsystemMode::AUTO;
    bool available     = false; // Hardware/Software sensor/actuator present
    bool active        = false; // Is the subsystem actually running?
    std::string reason;         // Descriptive reason for the active state
    SubsystemRuntimeState runtimeState = SubsystemRuntimeState::STOPPED;
    std::string lastError;      // Failure details if relevant
};

// Readable string converters
std::string toString(SubsystemName name);
std::string toString(SubsystemMode mode);
std::string toString(SubsystemRuntimeState state);
std::string toString(SttState state);

class SubsystemControl {
public:
    SubsystemControl();

    // Change Notification Callback
    using ChangeCallback = std::function<void()>;
    void setChangeCallback(ChangeCallback cb);

    using RuntimeStateQuery = std::function<SubsystemRuntimeState(SubsystemName)>;
    void setRuntimeStateQuery(RuntimeStateQuery query);

    // Mode management
    void setMode(SubsystemName name, SubsystemMode mode);
    SubsystemMode getMode(SubsystemName name) const;

    // Availability management (external sensors/modules report this)
    void setAvailable(SubsystemName name, bool available);
    bool isAvailable(SubsystemName name) const;

    // Status queries
    bool isActive(SubsystemName name) const;
    SubsystemStatus getStatus(SubsystemName name) const;
    std::vector<SubsystemStatus> getAllStatuses() const;

    // Recalculates 'active' and 'reason' for all subsystems based on mode/availability
    void refresh();

    // Subsystem Specific Mutators & Accessors (Helper Methods)
    void setMicEnabled(bool enabled);
    void toggleMic();
    bool isMicEnabled() const;

    void setSpeakerEnabled(bool enabled);
    void toggleSpeaker();
    bool isSpeakerEnabled() const;

    void setCameraEnabled(bool enabled);
    void toggleCamera();
    bool isCameraEnabled() const;

    void setScreenEnabled(bool enabled);
    void toggleScreen();
    bool isScreenEnabled() const;

    // Speech-to-Text State
    void setSttState(SttState state);
    SttState getSttState() const;

    // Debugging Helpers
    std::string getCompactStatusString() const;

private:
    void notifyListeners();

    mutable std::mutex mutex_;
    std::map<SubsystemName, SubsystemStatus> subsystems_;
    ChangeCallback changeCallback_;
    RuntimeStateQuery runtimeStateQuery_;
    SttState sttState_ = SttState::STOPPED;
};
