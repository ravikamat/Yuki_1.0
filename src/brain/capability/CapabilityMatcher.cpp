#include "CapabilityMatcher.h"
#include <algorithm>
#include <unordered_set>

using namespace yuki::capability;

CapabilityMatcher::CapabilityMatcher(const CapabilityGraph& graph) : graph_(graph) {}

std::vector<MatchResult> CapabilityMatcher::matchGoal(const std::string&,
                                                      const std::vector<std::string>& required_outputs,
                                                      const std::vector<std::string>&,
                                                      const std::string& current_platform,
                                                      float min_competence_threshold,
                                                      size_t top_k) {
    std::vector<MatchResult> results;
    std::unordered_set<uint32_t> candidate_ids;

    for (const auto& out : required_outputs) {
        auto nodes = graph_.getNodesByOutput(out);
        for (uint32_t id : nodes) {
            candidate_ids.insert(id);
        }
    }

    for (uint32_t id : candidate_ids) {
        auto node_opt = graph_.getNode(id);
        if (!node_opt.has_value()) continue;
        const auto& node = node_opt.value();
        if (!node.is_active) continue;

        MatchResult result;
        result.node_id = id;
        result.output_overlap = computeOutputOverlap(required_outputs, node.profile.outputs);

        result.platform_compatible = node.profile.platform_tags.empty();
        if (!result.platform_compatible) {
            for (const auto& plat : node.profile.platform_tags) {
                if (plat == current_platform) {
                    result.platform_compatible = true;
                    break;
                }
            }
        }

        result.competence_available = (node.profile.required_competence >= min_competence_threshold);

        result.confidence = result.output_overlap * 0.5f +
                           (result.platform_compatible ? 0.25f : 0.0f) +
                           (result.competence_available ? 0.25f : 0.0f);

        if (result.confidence > 0.0f) {
            results.push_back(std::move(result));
        }
    }

    std::sort(results.begin(), results.end(),
              [](const MatchResult& a, const MatchResult& b) { return a.confidence > b.confidence; });

    if (results.size() > top_k) {
        results.resize(top_k);
    }
    return results;
}

float CapabilityMatcher::computeOutputOverlap(const std::vector<std::string>& required,
                                              const std::vector<std::string>& provided) {
    if (required.empty() && provided.empty()) return 1.0f;
    if (required.empty() || provided.empty()) return 0.0f;

    std::unordered_set<std::string> req_set(required.begin(), required.end());
    std::unordered_set<std::string> prov_set(provided.begin(), provided.end());

    size_t intersection = 0;
    for (const auto& r : req_set) {
        if (prov_set.find(r) != prov_set.end()) ++intersection;
    }

    size_t uni = req_set.size() + prov_set.size() - intersection;
    return uni > 0 ? static_cast<float>(intersection) / static_cast<float>(uni) : 0.0f;
}
