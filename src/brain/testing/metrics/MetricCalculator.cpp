#include "brain/testing/metrics/MetricCalculator.h"
#include <numeric>
#include <cmath>
#include <algorithm>

namespace yuki {
namespace testing {

ExecutionMetrics MetricCalculator::compute(const std::vector<float>& samples) {
    ExecutionMetrics m;
    if (samples.empty()) return m;

    m.mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / samples.size();
    m.min = *std::min_element(samples.begin(), samples.end());
    m.max = *std::max_element(samples.begin(), samples.end());

    float variance = 0.0f;
    for (float s : samples) {
        float diff = s - m.mean;
        variance += diff * diff;
    }
    m.stdDev = std::sqrt(variance / samples.size());

    return m;
}

} // namespace testing
} // namespace yuki
