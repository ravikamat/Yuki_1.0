#pragma once
#include <string>
#include <functional>
#include "SubsystemControl.h"

struct PerceptionEvent;

struct CommandResult {
    bool handled = false;
    bool success = false;
    std::string responseText;
};

class CommandRouter {
public:
    CommandRouter(SubsystemControl& subsystems);
    
    // Callbacks for UI-specific actions (like restoring windows)
    using UIActionCallback = std::function<void(const std::string& action)>;
    void setUIActionCallback(UIActionCallback cb);

    // Provides the mobile server URL when the user asks to connect from phone
    using MobileUrlProvider = std::function<std::string()>;
    void setMobileUrlProvider(MobileUrlProvider fn);

    // Check if the input text matches any known command
    bool isCommand(const std::string& input) const;

    // Route raw input text
    CommandResult route(const std::string& rawInput);

    // Route unified perception event
    CommandResult route(const PerceptionEvent& event);

private:
    std::string normalize(const std::string& text) const;
    std::string getStatusSummary() const;
    bool hasMobileKeyword(const std::string& norm) const;

    SubsystemControl&  subsystems_;
    UIActionCallback   uiCallback_;
    MobileUrlProvider  mobileUrlProvider_;
};
