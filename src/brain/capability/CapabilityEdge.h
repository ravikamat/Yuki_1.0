#pragma once
#include <cstdint>
#include <cstddef>

namespace yuki::capability {

struct CapabilityEdge {
    uint32_t from_node{0};
    uint32_t to_node{0};
    float time_cost{0.0f};
    float resource_cost{0.0f};
    float risk_cost{0.0f};
    float competence_cost{0.0f};
    float monetary_cost{0.0f};
    bool requires_exclusive_lock{false};
    uint32_t max_parallel_instances{0xFFFFFFFFu};

    float scalarCost(float w_time, float w_resource, float w_risk,
                     float w_competence, float w_monetary) const {
        return w_time * time_cost + w_resource * resource_cost +
               w_risk * risk_cost + w_competence * competence_cost +
               w_monetary * monetary_cost;
    }
};

} // namespace yuki::capability
