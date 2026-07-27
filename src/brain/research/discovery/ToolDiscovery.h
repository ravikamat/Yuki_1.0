#ifndef YUKI_TOOL_DISCOVERY_H
#define YUKI_TOOL_DISCOVERY_H

#include "brain/research/core/ToolInterface.h"
#include "brain/research/core/ToolRegistry.h"
#include <vector>
#include <string>

namespace yuki {
namespace research {

class ToolDiscovery {
public:
    void scanPathEnvironment();
    void scanPluginDirectories();
    void scanPackageManagers();
    void scanKnownIDEs();
    void scanCloudServices();
    void scanLocalServices();

    // NEW — Platform App Discovery
    void scanMacOSApplications();      // /Applications, ~/Applications
    void scanWindowsProgramFiles();    // Registry + Program Files
    void scanLinuxDesktopEntries();    // /usr/share/applications
    void scanMobileURLSchemes();       // iOS: canOpenURL

    void registerDiscoveredTools(ToolRegistry* registry);
    void registerAll(ToolRegistry* registry);

    size_t getDiscoveredCount() const { return discovered_.size(); }
    void clear() { discovered_.clear(); }

private:
    std::vector<ToolMetadata> discovered_;

    ToolMetadata inferFromPath(const std::string& path);
    bool isKnownExecutable(const std::string& name);

    bool isMacOSAppInstalled(const std::string& bundleId);
    bool isWindowsAppInstalled(const std::string& appName);
    bool isLinuxAppInstalled(const std::string& desktopFile);
};

} // namespace research
} // namespace yuki

#endif
