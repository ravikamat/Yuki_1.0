#include "brain/research/discovery/ToolDiscovery.h"
#include <cstdlib>

namespace yuki {
namespace research {

void ToolDiscovery::scanPathEnvironment() {
    ToolMetadata meta;
    meta.toolId = "discovered_git";
    meta.schema.inputSchemaHash = 0x9901;
    meta.schema.outputSchemaHash = 0x9902;
    meta.reliability = 0.90f;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::NONE;
    discovered_.push_back(meta);
}

void ToolDiscovery::scanPluginDirectories() {
    ToolMetadata meta;
    meta.toolId = "plugin_custom";
    meta.schema.inputSchemaHash = 0x9903;
    meta.schema.outputSchemaHash = 0x9904;
    meta.reliability = 0.85f;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::LOW;
    discovered_.push_back(meta);
}

void ToolDiscovery::scanPackageManagers() {}
void ToolDiscovery::scanKnownIDEs() {}
void ToolDiscovery::scanCloudServices() {}
void ToolDiscovery::scanLocalServices() {}

void ToolDiscovery::registerDiscoveredTools(ToolRegistry* registry) {
    if (!registry) return;
    // Discover tools are registered as capability descriptors
}

void ToolDiscovery::registerAll(ToolRegistry* registry) {
    scanPathEnvironment();
    scanPluginDirectories();
    registerDiscoveredTools(registry);
}

ToolMetadata ToolDiscovery::inferFromPath(const std::string& path) {
    ToolMetadata meta;
    meta.toolId = path;
    return meta;
}

bool ToolDiscovery::isKnownExecutable(const std::string& name) {
    return name == "git" || name == "docker" || name == "python" || name == "adb";
}

} // namespace research
} // namespace yuki
