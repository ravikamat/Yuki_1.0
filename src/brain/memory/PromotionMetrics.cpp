// PromotionMetrics.cpp — exact decay + adaptive threshold formulas
#include "brain/memory/PromotionMetrics.h"

namespace yuki {
namespace memory {

double PromotionMetrics::decayedStrength(double lambda, double current_time_hours) const {
    double age = current_time_hours - created_at_hours;
    if (age < 0.0) age = 0.0;
    return static_cast<double>(reinforcement_count) * std::exp(-lambda * age);
}

double PromotionMetrics::adaptiveT1Threshold(size_t global_episodic_count) {
    return 0.5 + 0.2 * (1.0 - std::exp(-0.1 * static_cast<double>(global_episodic_count)));
}

double PromotionMetrics::adaptiveT2Threshold(size_t global_semantic_count) {
    return 0.6 + 0.2 * (1.0 - std::exp(-0.05 * static_cast<double>(global_semantic_count)));
}

} // namespace memory
} // namespace yuki
