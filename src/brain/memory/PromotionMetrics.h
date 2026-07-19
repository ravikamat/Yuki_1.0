// PromotionMetrics.h — Yuki_1.0 Phase B: Adaptive promotion with exponential decay
#pragma once
#include <string>
#include <cstdint>
#include <cmath>

namespace yuki {
namespace memory {

struct PromotionMetrics {
    std::string id;
    uint64_t access_count        = 0;
    uint64_t reinforcement_count = 0;
    uint64_t last_access_timestamp = 0; // Unix seconds
    double   created_at_hours    = 0.0;

    // Decayed reinforcement strength. Monotonically decreasing with age.
    // decayed_strength = reinforcement_count * exp(-lambda * age_hours)
    // lambda = 0.01 by default (configurable per call-site).
    double decayedStrength(double lambda, double current_time_hours) const;

    // Adaptive T1→T2 promotion threshold.
    // Starts at 0.5, asymptotes to 0.7 as episodic count grows.
    // t1_thresh = 0.5 + 0.2 * (1 - exp(-0.1 * global_episodic_count))
    static double adaptiveT1Threshold(size_t global_episodic_count);

    // Adaptive T2→T3 promotion threshold.
    // Starts at 0.6, asymptotes to 0.8 as semantic count grows.
    // t2_thresh = 0.6 + 0.2 * (1 - exp(-0.05 * global_semantic_count))
    static double adaptiveT2Threshold(size_t global_semantic_count);
};

} // namespace memory
} // namespace yuki
