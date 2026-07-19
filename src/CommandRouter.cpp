#include "CommandRouter.h"
#include "input/VisionSystem.h"
#include "input/PerceptionLayer.h"
#include "brain/core/ResponseResolver.h"
#include <algorithm>
#include <sstream>

CommandRouter::CommandRouter(SubsystemControl& subsystems)
    : subsystems_(subsystems), uiCallback_(nullptr) {}

void CommandRouter::setUIActionCallback(UIActionCallback cb) {
    uiCallback_ = cb;
}

void CommandRouter::setMobileUrlProvider(MobileUrlProvider fn) {
    mobileUrlProvider_ = fn;
}

// Returns true if the normalised input contains any mobile/phone keyword
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

    // Shell / UI commands
    if (norm == "status" || norm == "subsystem status" || norm == "get status" || norm == "system status") {
        res.handled = true;
        res.success = true;
        res.responseText = getStatusSummary();
        return res;
    }
    if (norm == "vision status" || norm == "vision") {
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_VISION_STATUS");
        return res;
    }
    if (norm == "open chat window" || norm == "chat floating window" || norm == "open floating window" || 
        norm == "show chat" || norm == "show chat window" || norm == "show floating window") {
        res.handled = true;
        res.success = true;
        if (uiCallback_) uiCallback_("OPEN_CHAT");
        res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_CHAT");
        return res;
    }
    if (norm == "open detail view" || norm == "open expanded view" || norm == "expand view" ||
        norm == "show detail" || norm == "show detail view") {
        res.handled = true;
        res.success = true;
        if (uiCallback_) uiCallback_("OPEN_DETAIL");
        res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_DETAIL");
        return res;
    }
    if (norm == "open avatar" || norm == "show avatar") {
        res.handled = true;
        res.success = true;
        if (uiCallback_) uiCallback_("OPEN_AVATAR");
        res.responseText = ResponseResolver::instance().resolve("CMD_OPEN_AVATAR");
        return res;
    }
    if (norm == "hide avatar" || norm == "close avatar") {
        res.handled = true;
        res.success = true;
        if (uiCallback_) uiCallback_("CLOSE_AVATAR");
        res.responseText = ResponseResolver::instance().resolve("CMD_CLOSE_AVATAR");
        return res;
    }

    // Subsystem commands
    if (norm == "mic on" || norm == "turn mic on" || norm == "enable mic" || 
        norm == "microphone on" || norm == "turn microphone on" || norm == "enable microphone") {
        subsystems_.setMicEnabled(true);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_MIC_ON");
        return res;
    }
    if (norm == "mic off" || norm == "turn mic off" || norm == "disable mic" || 
        norm == "microphone off" || norm == "turn microphone off" || norm == "disable microphone") {
        subsystems_.setMicEnabled(false);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_MIC_OFF");
        return res;
    }
    if (norm == "toggle mic" || norm == "toggle microphone") {
        subsystems_.toggleMic();
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_MIC_TOGGLE");
        return res;
    }

    if (norm == "speaker on" || norm == "turn speaker on" || norm == "enable speaker" ||
        norm == "sound on" || norm == "audio on") {
        subsystems_.setSpeakerEnabled(true);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SPEAKER_ON");
        return res;
    }
    if (norm == "speaker off" || norm == "turn speaker off" || norm == "disable speaker" ||
        norm == "sound off" || norm == "audio off") {
        subsystems_.setSpeakerEnabled(false);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SPEAKER_OFF");
        return res;
    }
    if (norm == "toggle speaker" || norm == "toggle sound" || norm == "toggle audio") {
        subsystems_.toggleSpeaker();
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SPEAKER_TOGGLE");
        return res;
    }

    if (norm == "camera on" || norm == "turn camera on" || norm == "enable camera" || 
        norm == "camera vision on" || norm == "start camera" || norm == "start camera vision") {
        subsystems_.setCameraEnabled(true);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_CAMERA_ON");
        return res;
    }
    if (norm == "camera off" || norm == "turn camera off" || norm == "disable camera" || 
        norm == "camera vision off" || norm == "stop camera" || norm == "stop camera vision") {
        subsystems_.setCameraEnabled(false);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_CAMERA_OFF");
        return res;
    }
    if (norm == "toggle camera") {
        subsystems_.toggleCamera();
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_CAMERA_TOGGLE");
        return res;
    }

    if (norm == "screen on" || norm == "turn screen on" || norm == "enable screen" || 
        norm == "screen vision on" || norm == "look screen" || norm == "look at screen" || 
        norm == "start screen" || norm == "start screen vision") {
        subsystems_.setScreenEnabled(true);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SCREEN_ON");
        return res;
    }
    if (norm == "screen off" || norm == "turn screen off" || norm == "disable screen" || 
        norm == "screen vision off" || norm == "stop screen" || norm == "stop screen vision") {
        subsystems_.setScreenEnabled(false);
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SCREEN_OFF");
        return res;
    }
    if (norm == "toggle screen") {
        subsystems_.toggleScreen();
        res.handled = true;
        res.success = true;
        res.responseText = ResponseResolver::instance().resolve("CMD_SCREEN_TOGGLE");
        return res;
    }

    // Out-of-band mobile URL handling
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

bool CommandRouter::isCommand(const std::string& input) const {
    std::string norm = normalize(input);
    if (norm.empty()) return false;
    return (norm == "mic on" || norm == "turn mic on" || norm == "enable mic" || 
            norm == "microphone on" || norm == "turn microphone on" || norm == "enable microphone" ||
            norm == "mic off" || norm == "turn mic off" || norm == "disable mic" || 
            norm == "microphone off" || norm == "turn microphone off" || norm == "disable microphone" ||
            norm == "toggle mic" || norm == "toggle microphone" ||
            norm == "speaker on" || norm == "turn speaker on" || norm == "enable speaker" ||
            norm == "sound on" || norm == "audio on" ||
            norm == "speaker off" || norm == "turn speaker off" || norm == "disable speaker" ||
            norm == "sound off" || norm == "audio off" ||
            norm == "toggle speaker" || norm == "toggle sound" || norm == "toggle audio" ||
            norm == "camera on" || norm == "turn camera on" || norm == "enable camera" || 
            norm == "camera vision on" || norm == "start camera" || norm == "start camera vision" ||
            norm == "camera off" || norm == "turn camera off" || norm == "disable camera" || 
            norm == "camera vision off" || norm == "stop camera" || norm == "stop camera vision" ||
            norm == "toggle camera" ||
            norm == "screen on" || norm == "turn screen on" || norm == "enable screen" || 
            norm == "screen vision on" || norm == "look screen" || norm == "look at screen" || 
            norm == "start screen" || norm == "start screen vision" ||
            norm == "screen off" || norm == "turn screen off" || norm == "disable screen" || 
            norm == "screen vision off" || norm == "stop screen" || norm == "stop screen vision" ||
            norm == "toggle screen" ||
            norm == "status" || norm == "subsystem status" || norm == "get status" || norm == "system status" ||
            norm == "vision" || norm == "vision status" ||
            norm == "open chat window" || norm == "chat floating window" || norm == "open floating window" || 
            norm == "show chat" || norm == "show chat window" || norm == "show floating window" ||
            norm == "open detail view" || norm == "open expanded view" || norm == "expand view" ||
            norm == "show detail" || norm == "show detail view" ||
            norm == "open avatar" || norm == "show avatar" ||
            norm == "hide avatar" || norm == "close avatar");
}

CommandResult CommandRouter::route(const PerceptionEvent& event) {
    // A PerceptionEvent's raw_content represents the input signal
    return route(event.raw_content);
}
