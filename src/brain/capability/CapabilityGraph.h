#pragma once
#include "CapabilityNode.h"
#include "CapabilityEdge.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>

namespace yuki::capability {

class CapabilityGraph {
public:
    CapabilityGraph();

    uint32_t registerTool(const std::string& tool_id, const CapabilityProfile& profile,
                          const std::string& description);
    uint32_t registerAbstractCapability(const std::string& name,
                                        const std::vector<std::string>& inputs,
                                        const std::vector<std::string>& outputs,
                                        const std::string& description);
    uint32_t registerGoal(const std::string& goal_text,
                          const std::vector<std::string>& required_outputs);

    bool addEdge(uint32_t from, uint32_t to, const CapabilityEdge& edge);
    bool removeEdge(uint32_t from, uint32_t to);
    bool removeNode(uint32_t id);

    void autoBuildEdges();
    void updateEdgeCosts(uint32_t tool_id, float live_duration_ms, float live_ram_mb,
                         float live_cpu_percent);

    std::optional<CapabilityNode> getNode(uint32_t id) const;
    std::vector<CapabilityEdge> getNeighbors(uint32_t node_id) const;
    std::vector<uint32_t> getNodesByOutput(const std::string& output_type) const;
    std::vector<uint32_t> getNodesByInput(const std::string& input_type) const;
    std::vector<uint32_t> getNodesByPlatform(const std::string& platform) const;

    size_t nodeCount() const;
    size_t edgeCount() const;

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    uint32_t next_id_{1};
    std::unordered_map<uint32_t, CapabilityNode> nodes_;
    std::unordered_map<uint32_t, std::vector<CapabilityEdge>> adjacency_;
    std::unordered_map<std::string, std::vector<uint32_t>> output_index_;
    std::unordered_map<std::string, std::vector<uint32_t>> input_index_;
    std::unordered_map<std::string, std::vector<uint32_t>> platform_index_;

    void rebuildIndices();
    void addToIndices(uint32_t id, const CapabilityNode& node);
    void removeFromIndices(uint32_t id, const CapabilityNode& node);
};

} // namespace yuki::capability
