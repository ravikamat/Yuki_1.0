#include "brain/research/core/ToolRegistry.h"

namespace yuki {
namespace research {

ToolRegistry::ToolRegistry() = default;

void ToolRegistry::registerTool(ToolPtr tool) {
    if (!tool) return;
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[tool->getMetadata().toolId] = tool;
}

void ToolRegistry::unregisterTool(const std::string& toolId) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.erase(toolId);
}

ToolPtr ToolRegistry::getTool(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(toolId);
    if (it != tools_.end()) return it->second;
    return nullptr;
}

std::vector<ToolPtr> ToolRegistry::getAllTools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolPtr> result;
    result.reserve(tools_.size());
    for (const auto& pair : tools_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<ToolPtr> ToolRegistry::findBySchemaHash(uint64_t outputSchemaHash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolPtr> result;
    for (const auto& pair : tools_) {
        if (pair.second->getMetadata().schema.outputSchemaHash == outputSchemaHash) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool ToolRegistry::hasTool(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(toolId) > 0;
}

size_t ToolRegistry::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.size();
}

void ToolRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.clear();
    discoveredMetadata_.clear();
}

void ToolRegistry::registerDiscovered(const std::vector<ToolMetadata>& tools) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& meta : tools) {
        discoveredMetadata_[meta.toolId] = meta;
    }
}

bool ToolRegistry::isDiscovered(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return discoveredMetadata_.count(toolId) > 0;
}

void ToolRegistry::registerActionTool(ToolPtr tool) {
    if (!tool) return;
    std::lock_guard<std::mutex> lock(mutex_);
    actionTools_[tool->getMetadata().toolId] = tool;
}

void ToolRegistry::unregisterActionTool(const std::string& toolId) {
    std::lock_guard<std::mutex> lock(mutex_);
    actionTools_.erase(toolId);
}

ToolPtr ToolRegistry::getActionTool(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = actionTools_.find(toolId);
    if (it != actionTools_.end()) return it->second;
    return nullptr;
}

std::vector<ToolPtr> ToolRegistry::getAllActionTools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolPtr> result;
    for (const auto& pair : actionTools_) {
        result.push_back(pair.second);
    }
    return result;
}

bool ToolRegistry::hasActionTool(const std::string& toolId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return actionTools_.count(toolId) > 0;
}

void ToolRegistry::registerToolWithProfile(const std::string& tool_id,
                                           const ::yuki::capability::CapabilityProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    toolProfiles_[tool_id] = profile;
}

std::optional<::yuki::capability::CapabilityProfile> ToolRegistry::getProfile(const std::string& tool_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = toolProfiles_.find(tool_id);
    if (it != toolProfiles_.end()) return it->second;
    return std::nullopt;
}

} // namespace research
} // namespace yuki
