#include "CapabilityGraph.h"
#include <algorithm>
#include <cstring>

using namespace yuki::capability;

CapabilityGraph::CapabilityGraph() = default;

uint32_t CapabilityGraph::registerTool(const std::string& tool_id,
                                       const CapabilityProfile& profile,
                                       const std::string& description) {
    uint32_t id = next_id_++;
    CapabilityNode node;
    node.id = id;
    node.name = tool_id;
    node.type = NodeType::TOOL_NODE;
    node.profile = profile;
    node.description = description;
    nodes_[id] = std::move(node);
    addToIndices(id, nodes_[id]);
    return id;
}

uint32_t CapabilityGraph::registerAbstractCapability(const std::string& name,
                                                     const std::vector<std::string>& inputs,
                                                     const std::vector<std::string>& outputs,
                                                     const std::string& description) {
    uint32_t id = next_id_++;
    CapabilityNode node;
    node.id = id;
    node.name = name;
    node.type = NodeType::ABSTRACT_NODE;
    node.profile.inputs = inputs;
    node.profile.outputs = outputs;
    node.description = description;
    nodes_[id] = std::move(node);
    addToIndices(id, nodes_[id]);
    return id;
}

uint32_t CapabilityGraph::registerGoal(const std::string& goal_text,
                                       const std::vector<std::string>& required_outputs) {
    uint32_t id = next_id_++;
    CapabilityNode node;
    node.id = id;
    node.name = goal_text;
    node.type = NodeType::GOAL_NODE;
    node.profile.inputs = required_outputs;
    node.profile.outputs = required_outputs;
    nodes_[id] = std::move(node);
    addToIndices(id, nodes_[id]);
    return id;
}

bool CapabilityGraph::addEdge(uint32_t from, uint32_t to, const CapabilityEdge& edge) {
    if (nodes_.find(from) == nodes_.end() || nodes_.find(to) == nodes_.end()) {
        return false;
    }
    if (from == to) return false;
    CapabilityEdge e = edge;
    e.from_node = from;
    e.to_node = to;
    auto& edges = adjacency_[from];
    for (const auto& existing : edges) {
        if (existing.to_node == to) return false; // duplicate
    }
    edges.push_back(std::move(e));
    return true;
}

bool CapabilityGraph::removeEdge(uint32_t from, uint32_t to) {
    auto it = adjacency_.find(from);
    if (it == adjacency_.end()) return false;
    auto& edges = it->second;
    auto old_size = edges.size();
    edges.erase(std::remove_if(edges.begin(), edges.end(),
        [to](const CapabilityEdge& e) { return e.to_node == to; }), edges.end());
    return edges.size() < old_size;
}

bool CapabilityGraph::removeNode(uint32_t id) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return false;
    removeFromIndices(id, it->second);
    nodes_.erase(it);
    adjacency_.erase(id);
    for (auto& [from, edges] : adjacency_) {
        edges.erase(std::remove_if(edges.begin(), edges.end(),
            [id](const CapabilityEdge& e) { return e.to_node == id; }), edges.end());
    }
    return true;
}

void CapabilityGraph::autoBuildEdges() {
    for (const auto& [from_id, from_node] : nodes_) {
        if (!from_node.is_active) continue;
        for (const auto& out : from_node.profile.outputs) {
            auto it = input_index_.find(out);
            if (it == input_index_.end()) continue;
            for (uint32_t to_id : it->second) {
                if (to_id == from_id) continue;
                auto to_it = nodes_.find(to_id);
                if (to_it == nodes_.end() || !to_it->second.is_active) continue;

                auto& edges = adjacency_[from_id];
                bool exists = false;
                for (const auto& e : edges) {
                    if (e.to_node == to_id) { exists = true; break; }
                }
                if (exists) continue;

                CapabilityEdge edge;
                edge.from_node = from_id;
                edge.to_node = to_id;
                const auto& fp = from_node.profile;
                const auto& tp = to_it->second.profile;
                constexpr float kDurationNorm = 10000.0f;
                constexpr float kRamNorm = 2048.0f;
                edge.time_cost = std::min(1.0f, fp.avg_duration_ms / kDurationNorm);
                edge.resource_cost = std::min(1.0f, (fp.avg_ram_mb + tp.avg_ram_mb) / kRamNorm);
                edge.risk_cost = std::max(fp.base_risk, tp.base_risk);
                edge.competence_cost = 1.0f - tp.required_competence;
                edge.monetary_cost = 0.0f; // derived from EconomyEngine in v2
                edges.push_back(std::move(edge));
            }
        }
    }
}

void CapabilityGraph::updateEdgeCosts(uint32_t tool_id, float live_duration_ms,
                                      float live_ram_mb, float live_cpu_percent) {
    auto it = adjacency_.find(tool_id);
    if (it == adjacency_.end()) return;

    constexpr float kAlpha = 0.1f;
    constexpr float kDurationNorm = 10000.0f;
    constexpr float kRamNorm = 2048.0f;

    for (auto& edge : it->second) {
        edge.time_cost = std::min(1.0f, live_duration_ms / kDurationNorm);
        edge.resource_cost = std::min(1.0f, live_ram_mb / kRamNorm);
    }

    auto nit = nodes_.find(tool_id);
    if (nit != nodes_.end()) {
        auto& prof = nit->second.profile;
        prof.avg_duration_ms = (1.0f - kAlpha) * prof.avg_duration_ms + kAlpha * live_duration_ms;
        prof.avg_ram_mb = (1.0f - kAlpha) * prof.avg_ram_mb + kAlpha * live_ram_mb;
        prof.avg_cpu_percent = (1.0f - kAlpha) * prof.avg_cpu_percent + kAlpha * live_cpu_percent;
    }
}

std::optional<CapabilityNode> CapabilityGraph::getNode(uint32_t id) const {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) return it->second;
    return std::nullopt;
}

std::vector<CapabilityEdge> CapabilityGraph::getNeighbors(uint32_t node_id) const {
    auto it = adjacency_.find(node_id);
    if (it != adjacency_.end()) return it->second;
    return {};
}

std::vector<uint32_t> CapabilityGraph::getNodesByOutput(const std::string& output_type) const {
    auto it = output_index_.find(output_type);
    if (it != output_index_.end()) return it->second;
    return {};
}

std::vector<uint32_t> CapabilityGraph::getNodesByInput(const std::string& input_type) const {
    auto it = input_index_.find(input_type);
    if (it != input_index_.end()) return it->second;
    return {};
}

std::vector<uint32_t> CapabilityGraph::getNodesByPlatform(const std::string& platform) const {
    auto it = platform_index_.find(platform);
    if (it != platform_index_.end()) return it->second;
    return {};
}

size_t CapabilityGraph::nodeCount() const {
    return nodes_.size();
}

size_t CapabilityGraph::edgeCount() const {
    size_t count = 0;
    for (const auto& kv : adjacency_) {
        count += kv.second.size();
    }
    return count;
}

void CapabilityGraph::rebuildIndices() {
    output_index_.clear();
    input_index_.clear();
    platform_index_.clear();
    for (const auto& [id, node] : nodes_) {
        addToIndices(id, node);
    }
}

void CapabilityGraph::addToIndices(uint32_t id, const CapabilityNode& node) {
    for (const auto& out : node.profile.outputs) {
        output_index_[out].push_back(id);
    }
    for (const auto& in : node.profile.inputs) {
        input_index_[in].push_back(id);
    }
    for (const auto& plat : node.profile.platform_tags) {
        platform_index_[plat].push_back(id);
    }
}

void CapabilityGraph::removeFromIndices(uint32_t id, const CapabilityNode& node) {
    auto erase_id = [id](std::vector<uint32_t>& vec) {
        vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    };
    for (const auto& out : node.profile.outputs) {
        auto it = output_index_.find(out);
        if (it != output_index_.end()) erase_id(it->second);
    }
    for (const auto& in : node.profile.inputs) {
        auto it = input_index_.find(in);
        if (it != input_index_.end()) erase_id(it->second);
    }
    for (const auto& plat : node.profile.platform_tags) {
        auto it = platform_index_.find(plat);
        if (it != platform_index_.end()) erase_id(it->second);
    }
}

std::vector<uint8_t> CapabilityGraph::serialize() const {
    std::vector<uint8_t> data;
    constexpr uint32_t kMagic = 0x59434150u; // "YCAP"
    constexpr uint32_t kVersion = 1u;

    auto append_u32 = [&data](uint32_t v) {
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&v),
                    reinterpret_cast<uint8_t*>(&v) + sizeof(v));
    };

    append_u32(kMagic);
    append_u32(kVersion);
    append_u32(static_cast<uint32_t>(nodes_.size()));
    append_u32(static_cast<uint32_t>(edgeCount()));

    for (const auto& [id, node] : nodes_) {
        append_u32(id);
        append_u32(static_cast<uint32_t>(node.type));
        auto prof_data = node.profile.serialize();
        append_u32(static_cast<uint32_t>(prof_data.size()));
        data.insert(data.end(), prof_data.begin(), prof_data.end());
        append_u32(static_cast<uint32_t>(node.description.size()));
        data.insert(data.end(), node.description.begin(), node.description.end());
        append_u32(node.is_active ? 1u : 0u);
    }

    for (const auto& [from, edges] : adjacency_) {
        for (const auto& e : edges) {
            append_u32(e.from_node);
            append_u32(e.to_node);
            auto append_f = [&data](float f) {
                data.insert(data.end(), reinterpret_cast<uint8_t*>(&f),
                            reinterpret_cast<uint8_t*>(&f) + sizeof(f));
            };
            append_f(e.time_cost);
            append_f(e.resource_cost);
            append_f(e.risk_cost);
            append_f(e.competence_cost);
            append_f(e.monetary_cost);
            append_u32(e.requires_exclusive_lock ? 1u : 0u);
            append_u32(e.max_parallel_instances);
        }
    }
    return data;
}

bool CapabilityGraph::deserialize(const std::vector<uint8_t>& data) {
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    auto read_u32 = [&ptr, end]() -> uint32_t {
        if (ptr + sizeof(uint32_t) > end) return 0;
        uint32_t v = 0;
        std::memcpy(&v, ptr, sizeof(v));
        ptr += sizeof(v);
        return v;
    };

    if (read_u32() != 0x59434150u) return false;
    if (read_u32() != 1u) return false;

    uint32_t node_count = read_u32();
    uint32_t edge_count = read_u32();

    nodes_.clear();
    adjacency_.clear();
    next_id_ = 1;

    for (uint32_t i = 0; i < node_count; ++i) {
        uint32_t id = read_u32();
        uint32_t type_val = read_u32();
        uint32_t prof_size = read_u32();
        if (ptr + prof_size > end) return false;
        std::vector<uint8_t> prof_data(ptr, ptr + prof_size);
        ptr += prof_size;
        auto prof_opt = CapabilityProfile::deserialize(prof_data);
        if (!prof_opt.has_value()) return false;

        uint32_t desc_len = read_u32();
        if (ptr + desc_len > end) return false;
        std::string desc(reinterpret_cast<const char*>(ptr), desc_len);
        ptr += desc_len;

        uint32_t active = read_u32();

        CapabilityNode node;
        node.id = id;
        node.type = static_cast<NodeType>(type_val);
        node.profile = std::move(prof_opt.value());
        node.description = std::move(desc);
        node.is_active = (active != 0);
        if (type_val == 1) node.name = node.profile.tool_id;
        nodes_[id] = std::move(node);
        if (id >= next_id_) next_id_ = id + 1;
    }

    for (uint32_t i = 0; i < edge_count; ++i) {
        uint32_t from = read_u32();
        uint32_t to = read_u32();
        auto read_f = [&ptr, end]() -> float {
            if (ptr + sizeof(float) > end) return 0.0f;
            float f = 0.0f;
            std::memcpy(&f, ptr, sizeof(f));
            ptr += sizeof(f);
            return f;
        };
        CapabilityEdge e;
        e.from_node = from;
        e.to_node = to;
        e.time_cost = read_f();
        e.resource_cost = read_f();
        e.risk_cost = read_f();
        e.competence_cost = read_f();
        e.monetary_cost = read_f();
        e.requires_exclusive_lock = (read_u32() != 0);
        e.max_parallel_instances = read_u32();
        adjacency_[from].push_back(std::move(e));
    }

    rebuildIndices();
    return true;
}
