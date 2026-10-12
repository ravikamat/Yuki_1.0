#pragma once
// Shared causality types - included by both causality and planning namespaces

#include <vector>
#include <string>

namespace yuki {
namespace brain {
namespace causality {

struct Intervention {
    uint32_t node_id;
    bool value;
};

struct CausalGraph;

struct InterventionPolicy {
    yuki::brain::inference::Policy policy;
    std::string target_variable;
    std::string intervention_variable;
    float expected_effect;
};

} // namespace causality
} // namespace brain
} // namespace yuki