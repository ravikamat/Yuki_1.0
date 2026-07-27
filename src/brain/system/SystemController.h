#pragma once
#include <string>
#include <memory>
#include <mutex>

namespace yuki::security {
class SecuritySandbox;
class ApprovalGate;
}

namespace yuki::system {
class ResourceMonitor;

class SystemController {
public:
    explicit SystemController(yuki::security::SecuritySandbox* sandbox = nullptr,
                              yuki::security::ApprovalGate* gate = nullptr,
                              yuki::system::ResourceMonitor* monitor = nullptr);
    ~SystemController();

    bool screenshot(const std::string& path, std::string& error);
    bool setVolume(float level); // [0.0, 1.0]
    bool mute();
    bool unmute();
    bool setClipboardText(const std::string& text);
    bool getClipboardText(std::string& out);
    bool openUrl(const std::string& url, std::string& error);
    bool openApplication(const std::string& app_name, std::string& error);

    struct MetricsSnapshot {
        float cpu_percent = 0.0f;
        float ram_percent = 0.0f;
        float disk_percent = 0.0f;
        float volume_level = 1.0f;
    };
    MetricsSnapshot getMetricsSnapshot();

    void setSandbox(yuki::security::SecuritySandbox* sandbox);
    void setApprovalGate(yuki::security::ApprovalGate* gate);
    void setResourceMonitor(yuki::system::ResourceMonitor* monitor);

private:
    yuki::security::SecuritySandbox* sandbox_;
    yuki::security::ApprovalGate* gate_;
    yuki::system::ResourceMonitor* monitor_;
    float volume_level_{1.0f};
    bool muted_{false};
    std::string clipboard_data_;
    mutable std::mutex mutex_;

    bool validatePath(const std::string& path, std::string& error);
    bool validateUrl(const std::string& url, std::string& error);
    float clamp01(float v) const;
};

} // namespace yuki::system
