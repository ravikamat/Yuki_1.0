#ifndef YUKI_TOOL_REGISTRY_H
#define YUKI_TOOL_REGISTRY_H

#include "brain/research/core/ToolInterface.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

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

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolPtr> tools_;
    std::unordered_map<std::string, ToolMetadata> discoveredMetadata_;
};

} // namespace research
} // namespace yuki

#endif
