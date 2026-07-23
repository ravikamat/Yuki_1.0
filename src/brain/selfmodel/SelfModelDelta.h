#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace selfmodel {

struct CompetenceGap {
    uint32_t domain = 0;
    float self_assessed_competence = 0.0f;
    float measured_competence = 0.0f;
    float gap = 0.0f;
    uint32_t persistence_turns = 0;
    float severity = 0.0f;
};

class SelfModelDelta {
public:
    SelfModelDelta() = default;

    std::vector<CompetenceGap> computeGaps(
        const std::vector<float>& self_assessed,
        const std::vector<float>& measured) const;

    std::vector<uint32_t> detectDecline(
        const std::vector<std::vector<float>>& competence_history,
        float decline_threshold = 0.05f) const;

    std::vector<uint32_t> detectInstability(
        const std::vector<std::vector<float>>& competence_history,
        float variance_threshold = 0.1f) const;

private:
    static constexpr uint32_t DOMAIN_COUNT = 11;
    static constexpr float OVERCONFIDENCE_PENALTY = 2.0f;
};

} // namespace selfmodel
} // namespace yuki
