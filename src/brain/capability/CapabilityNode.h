#pragma once
#include "CapabilityProfile.h"
#include <string>
#include <cstdint>

namespace yuki::capability {

enum class NodeType : uint8_t {
    TOOL_NODE = 1,
    ABSTRACT_NODE = 2,
    GOAL_NODE = 3
};

struct CapabilityNode {
    uint32_t id{0};
    std::string name;
    NodeType type{NodeType::TOOL_NODE};
    CapabilityProfile profile;
    std::string description;
    bool is_active{true};
};

} // namespace yuki::capability
