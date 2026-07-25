#ifndef YUKI_TOOL_REGISTRY_H
#define YUKI_TOOL_REGISTRY_H

#include "brain/research/core/ToolInterface.h"
#include "brain/capability/CapabilityProfile.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>

namespace yuki {
namespace research {

class ToolRegistry {
public:
    ToolRegistry();

    void registerTool(ToolPtr tool);
    void unregisterTool(const std::string& toolId);
    ToolPtr getTool(const std::string& toolId) const;
    std::vector<ToolPtr> getAllTools() const;
    std::vector<ToolPtr> findBySchemaHash(uint64_t outputSchemaHash) const;
    bool hasTool(const std::string& toolId) const;
    size_t size() const;

    void clear();

    void registerDiscovered(const std::vector<ToolMetadata>& tools);
    bool isDiscovered(const std::string& toolId) const;

    // M4: Separate action tool registration
    void registerActionTool(ToolPtr tool);
    void unregisterActionTool(const std::string& toolId);
    ToolPtr getActionTool(const std::string& toolId) const;
    std::vector<ToolPtr> getAllActionTools() const;
    bool hasActionTool(const std::string& toolId) const;

    // M5: CapabilityProfile integration
    void registerToolWithProfile(const std::string& tool_id, const ::yuki::capability::CapabilityProfile& profile);
    std::optional<::yuki::capability::CapabilityProfile> getProfile(const std::string& tool_id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolPtr> tools_;
    std::unordered_map<std::string, ToolPtr> actionTools_;
    std::unordered_map<std::string, ToolMetadata> discoveredMetadata_;
    std::unordered_map<std::string, ::yuki::capability::CapabilityProfile> toolProfiles_;
};

} // namespace research
} // namespace yuki

#endif
