#ifndef YUKI_METRIC_CALCULATOR_H
#define YUKI_METRIC_CALCULATOR_H

#include <vector>

namespace yuki {
namespace testing {

struct ExecutionMetrics {
    float mean = 0.0f;
    float stdDev = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
};

class MetricCalculator {
public:
    ExecutionMetrics compute(const std::vector<float>& samples);
};

} // namespace testing
} // namespace yuki

#endif
