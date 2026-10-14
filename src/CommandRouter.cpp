#include "CommandRouter.h"
#include "input/VisionSystem.h"
#include "input/PerceptionLayer.h"
#include "brain/core/ResponseResolver.h"
#include <algorithm>
#include <sstream>

CommandRouter::CommandRouter(SubsystemControl& subsystems)
    : subsystems_(subsystems), uiCallback_(nullptr) {
    // Register built-in commands
    registerBuiltinCommands();
}

void CommandRouter::setUIActionCallback(UIActionCallback cb) {
    uiCallback_ = cb;
}

void CommandRouter::setMobileUrlProvider(MobileUrlProvider fn) {
    mobileUrlProvider_ = fn;
}

bool CommandRouter::registerCommand(const CommandInfo& cmd) {
    return CommandRegistry::instance().registerCommand(cmd);
}

bool CommandRouter::unregisterCommand(const std::string& name) {
    return CommandRegistry::instance().unregisterCommand(name);
}

void CommandRouter::registerBuiltinCommands() {
    auto& registry = CommandRegistry::instance();
    
    // UI commands
    registry.registerCommand({
        "open_chat", {"open chat window", "chat floating window", "open floating window",
                      "show chat", "show chat window", "show floating window"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            if (uiCallback_) uiCallback_("OPEN_CHAT");
            res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_CHAT");
            return res;
        },
        "Open the chat window",
        "ui", 10
    });
    
    registry.registerCommand({
        "open_detail", {"open detail view", "open expanded view", "expand view",
                        "show detail", "show detail view"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            if (uiCallback_) uiCallback_("OPEN_DETAIL");
            res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_DETAIL");
            return res;
        },
        "Open the detail view",
        "ui", 10
    });
    
    registry.registerCommand({
        "open_avatar", {"open avatar", "show avatar"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            if (uiCallback_) uiCallback_("OPEN_AVATAR");
            res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_AVATAR");
            return res;
        },
        "Show the avatar",
        "ui", 10
    });
    
    registry.registerCommand({
        "close_avatar", {"hide avatar", "close avatar"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            if (uiCallback_) uiCallback_("CLOSE_AVATAR");
            res.responseText = ResponseResolver::instance().resolve("CMD_CLOSE_AVATAR");
            return res;
        },
        "Hide the avatar",
        "ui", 10
    });
    
    // Status commands
    registry.registerCommand({
        "status", {"status", "subsystem status", "get status", "system status"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            res.responseText = getStatusSummary();
            return res;
        },
        "Show subsystem status",
        "system", 10
    });
    
    registry.registerCommand({
        "vision_status", {"vision status", "vision"},
        [this](const std::string&) {
            CommandResult res;
            res.handled = true;
            res.success = true;
            res.responseText = ResponseResolver::instance().resolve("CMD_VISION_STATUS");
            return res;
        },
        "Show vision system status",
        "system", 10
    });
    
    // Mic commands
    auto makeMicCmd = [this](const std::string& name, const std::vector<std::string>& triggers,
                              std::function<void()> action, const std::string& response_key, const std::string& desc) {
        return CommandInfo{
            name, triggers,
            [this, action, response_key](const std::string&) {
                CommandResult res;
                action();
                res.handled = true;
                res.success = true;
                res.responseText = ResponseResolver::instance().resolve(response_key);
                return res;
            },
            desc, "subsystem", 10
        };
    };
    
    registry.registerCommand(makeMicCmd("mic_on", {"mic on", "turn mic on", "enable mic",
                                                  "microphone on", "turn microphone on", "enable microphone"},
                                       [this](){ subsystems_.setMicEnabled(true); },
                                       "CMD_MIC_ON", "Turn microphone on"));
    
    registry.registerCommand(makeMicCmd("mic_off", {"mic off", "turn mic off", "disable mic",
                                                   "microphone off", "turn microphone off", "disable microphone"},
                                       [this](){ subsystems_.setMicEnabled(false); },
                                       "CMD_MIC_OFF", "Turn microphone off"));
    
    registry.registerCommand(makeMicCmd("mic_toggle", {"toggle mic", "toggle microphone"},
                                       [this](){ subsystems_.toggleMic(); },
                                       "CMD_MIC_TOGGLE", "Toggle microphone"));
    
    // Speaker commands
    registry.registerCommand(makeMicCmd("speaker_on", {"speaker on", "turn speaker on", "enable speaker",
                                                      "sound on", "audio on"},
                                       [this](){ subsystems_.setSpeakerEnabled(true); },
                                       "CMD_SPEAKER_ON", "Turn speaker on"));
    
    registry.registerCommand(makeMicCmd("speaker_off", {"speaker off", "turn speaker off", "disable speaker",
                                                       "sound off", "audio off"},
                                       [this](){ subsystems_.setSpeakerEnabled(false); },
                                       "CMD_SPEAKER_OFF", "Turn speaker off"));
    
    registry.registerCommand(makeMicCmd("speaker_toggle", {"toggle speaker", "toggle sound", "toggle audio"},
                                       [this](){ subsystems_.toggleSpeaker(); },
                                       "CMD_SPEAKER_TOGGLE", "Toggle speaker"));
    
    // Camera commands
    registry.registerCommand(makeMicCmd("camera_on", {"camera on", "turn camera on", "enable camera",
                                                     "camera vision on", "start camera", "start camera vision"},
                                       [this](){ subsystems_.setCameraEnabled(true); },
                                       "CMD_CAMERA_ON", "Turn camera on"));
    
    registry.registerCommand(makeMicCmd("camera_off", {"camera off", "turn camera off", "disable camera",
                                                      "camera vision off", "stop camera", "stop camera vision"},
                                       [this](){ subsystems_.setCameraEnabled(false); },
                                       "CMD_CAMERA_OFF", "Turn camera off"));
    
    registry.registerCommand(makeMicCmd("camera_toggle", {"toggle camera"},
                                       [this](){ subsystems_.toggleCamera(); },
                                       "CMD_CAMERA_TOGGLE", "Toggle camera"));
    
    // Screen commands
    registry.registerCommand(makeMicCmd("screen_on", {"screen on", "turn screen on", "enable screen",
                                                     "screen vision on", "look screen", "look at screen",
                                                     "start screen", "start screen vision"},
                                       [this](){ subsystems_.setScreenEnabled(true); },
                                       "CMD_SCREEN_ON", "Turn screen on"));
    
    registry.registerCommand(makeMicCmd("screen_off", {"screen off", "turn screen off", "disable screen",
                                                      "screen vision off", "stop screen", "stop screen vision"},
                                       [this](){ subsystems_.setScreenEnabled(false); },
                                       "CMD_SCREEN_OFF", "Turn screen off"));
    
    registry.registerCommand(makeMicCmd("screen_toggle", {"toggle screen"},
                                       [this](){ subsystems_.toggleScreen(); },
                                       "CMD_SCREEN_TOGGLE", "Toggle screen"));
}

std::string CommandRouter::normalize(const std::string& text) const {
    std::string norm = text;
    // Lowercase conversion
    std::transform(norm.begin(), norm.end(), norm.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    // Trim leading/trailing spaces
    size_t first = norm.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = norm.find_last_not_of(" \t\r\n");
    norm = norm.substr(first, (last - first + 1));

    // Remove double/extra internal spaces
    std::string clean;
    bool inSpace = false;
    for (char c : norm) {
        if (std::isspace(c)) {
            if (!inSpace) {
                clean += ' ';
                inSpace = true;
            }
        } else {
            clean += c;
            inSpace = false;
        }
    }
    return clean;
}

std::string CommandRouter::getStatusSummary() const {
    std::stringstream ss;
    ss << "Perception & Subsystem Status Summary:\n";
    
    auto getStatusLine = [&](SubsystemName name, const std::string& label) {
        auto s = subsystems_.getStatus(name);
        std::stringstream line;
        
        // Desired state
        std::string desired = (s.mode == SubsystemMode::FORCED_OFF) ? "OFF" : "ON";
        
        // Runtime state
        std::string runtime = toString(s.runtimeState);
        
        line << "  [" << label << "] Desired: " << desired 
             << " | Runtime: " << runtime;
        
        if (s.runtimeState == SubsystemRuntimeState::FAILED || 
            s.runtimeState == SubsystemRuntimeState::UNAVAILABLE) {
            line << " (Unavailable/Failed: " << s.lastError << ")";
        } else {
            line << " (" << s.reason << ")";
        }
        line << "\n";
        return line.str();
    };
    
    ss << getStatusLine(SubsystemName::EAR, "MIC");
    ss << getStatusLine(SubsystemName::MOUTH, "SPEAKER");
    ss << getStatusLine(SubsystemName::WORLD_EYE, "CAMERA");
    ss << getStatusLine(SubsystemName::SCREEN_EYE, "SCREEN");
    ss << getStatusLine(SubsystemName::BODY_STATE, "BODY");
    
    return ss.str();
}

CommandResult CommandRouter::route(const std::string& rawInput) {
    std::string norm = normalize(rawInput);
    CommandResult res;
    
    if (norm.empty()) {
        return res;
    }
    
    // First check registry for exact/prefix matches
    auto [reg_result, cmd_info] = CommandRegistry::instance().findCommand(norm);
    if (reg_result.handled) {
        return reg_result;
    }
    
    // Mobile URL handling (keyword-based, not exact match)
    if (hasMobileKeyword(norm)) {
        res.handled = true;
        res.success = true;
        if (mobileUrlProvider_) {
            std::string url = mobileUrlProvider_();
            res.responseText = ResponseResolver::instance().resolve(
                "CMD_MOBILE_URL", {{"url", url}});
        } else {
            res.responseText = ResponseResolver::instance().resolve("CMD_MOBILE_UNAVAILABLE");
        }
        return res;
    }
    
    // Everything else falls through to MotherCore
    return res;
}

bool CommandRouter::hasMobileKeyword(const std::string& norm) const {
    const char* keys[] = {
        "mobile", "phone", "connect me", "connect on", "go live",
        "access from", "open on", "link me", "give me the link",
        "give link", "mobile access", "phone access", "connect yuki",
        "connect to mobile", "connect to phone", "access you from",
        "how to connect", "how do i connect", "mobile url",
        "phone url", "wifi", "mobile server", "browser link"
    };
    for (const char* k : keys) {
        if (norm.find(k) != std::string::npos) return true;
    }
    return false;
}

bool CommandRouter::isCommand(const std::string& input) const {
    std::string norm = normalize(input);
    if (norm.empty()) return false;
    
    // Check registry
    auto [reg_result, cmd_info] = CommandRegistry::instance().findCommand(norm);
    if (reg_result.handled) {
        return true;
    }
    
    // Check mobile keywords
    if (hasMobileKeyword(norm)) {
        return true;
    }
    
    return false;
}

CommandResult CommandRouter::route(const PerceptionEvent& event) {
    // A PerceptionEvent's raw_content represents the input signal
    return route(event.raw_content);
}