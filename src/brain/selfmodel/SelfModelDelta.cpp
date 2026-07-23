#include "SelfModelDelta.h"
#include <cmath>
#include <algorithm>

namespace yuki {
namespace selfmodel {

std::vector<CompetenceGap> SelfModelDelta::computeGaps(
    const std::vector<float>& self_assessed,
    const std::vector<float>& measured) const {

    std::vector<CompetenceGap> gaps;
    size_t count = std::min({self_assessed.size(), measured.size(),
                             static_cast<size_t>(DOMAIN_COUNT)});

    for (size_t i = 0; i < count; ++i) {
        CompetenceGap g;
        g.domain = static_cast<uint32_t>(i);
        g.self_assessed_competence = self_assessed[i];
        g.measured_competence = measured[i];
        g.gap = self_assessed[i] - measured[i];

        float abs_gap = std::abs(g.gap);
        if (g.gap > 0.0f) {
            g.severity = std::min(1.0f, abs_gap * OVERCONFIDENCE_PENALTY);
        } else {
            g.severity = std::min(1.0f, abs_gap);
        }

        gaps.push_back(g);
    }
    return gaps;
}

std::vector<uint32_t> SelfModelDelta::detectDecline(
    const std::vector<std::vector<float>>& competence_history,
    float decline_threshold) const {

    std::vector<uint32_t> declining;
    if (competence_history.size() < 2) return declining;

    size_t domain_count = std::min(competence_history[0].size(),
                                    static_cast<size_t>(DOMAIN_COUNT));

    for (size_t d = 0; d < domain_count; ++d) {
        float first = competence_history.front()[d];
        float window_avg = 0.0f;
        size_t window_size = std::min(size_t{3}, competence_history.size());
        for (size_t i = competence_history.size() - window_size;
             i < competence_history.size(); ++i) {
            window_avg += competence_history[i][d];
        }
        window_avg /= static_cast<float>(window_size);

        if (first - window_avg > decline_threshold) {
            declining.push_back(static_cast<uint32_t>(d));
        }
    }
    return declining;
}

std::vector<uint32_t> SelfModelDelta::detectInstability(
    const std::vector<std::vector<float>>& competence_history,
    float variance_threshold) const {

    std::vector<uint32_t> unstable;
    if (competence_history.size() < 2) return unstable;

    size_t domain_count = std::min(competence_history[0].size(),
                                    static_cast<size_t>(DOMAIN_COUNT));

    for (size_t d = 0; d < domain_count; ++d) {
        float mean = 0.0f;
        for (const auto& h : competence_history) {
            mean += h[d];
        }
        mean /= static_cast<float>(competence_history.size());

        float variance = 0.0f;
        for (const auto& h : competence_history) {
            float diff = h[d] - mean;
            variance += diff * diff;
        }
        variance /= static_cast<float>(competence_history.size());

        if (variance > variance_threshold * variance_threshold) {
            unstable.push_back(static_cast<uint32_t>(d));
        }
    }
    return unstable;
}

} // namespace selfmodel
} // namespace yuki
