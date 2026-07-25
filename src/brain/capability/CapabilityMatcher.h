#pragma once
#include "CapabilityGraph.h"
#include <string>
#include <vector>

namespace yuki::capability {

struct MatchResult {
    uint32_t node_id{0};
    float confidence{0.0f};
    float output_overlap{0.0f};
    bool platform_compatible{false};
    bool competence_available{false};
};

class CapabilityMatcher {
public:
    explicit CapabilityMatcher(const CapabilityGraph& graph);

    std::vector<MatchResult> matchGoal(const std::string& goal_text,
                                       const std::vector<std::string>& required_outputs,
                                       const std::vector<std::string>& available_inputs,
                                       const std::string& current_platform,
                                       float min_competence_threshold,
                                       size_t top_k);

    float computeOutputOverlap(const std::vector<std::string>& required,
                               const std::vector<std::string>& provided);

private:
    const CapabilityGraph& graph_;
};

} // namespace yuki::capability
