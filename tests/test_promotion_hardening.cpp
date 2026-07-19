// tests/test_promotion_hardening.cpp — Phase B: PromotionMetrics adaptive promotion tests
#include <gtest/gtest.h>
#include <cmath>
#include "brain/memory/PromotionMetrics.h"

using namespace yuki::memory;

// ── TEST 1: decayedStrength is monotonically decreasing with age ──────────────
TEST(Promotion, DecayedStrengthMonotonic) {
    PromotionMetrics pm;
    pm.reinforcement_count = 10;
    pm.created_at_hours    = 0.0;

    constexpr double lambda = 0.01;

    double prev = pm.decayedStrength(lambda, 0.0);  // age = 0 → strength = 10.0
    EXPECT_NEAR(prev, 10.0, 1e-9);

    for (double age_h : { 10.0, 50.0, 100.0, 500.0, 1000.0 }) {
        double curr = pm.decayedStrength(lambda, age_h);
        EXPECT_LT(curr, prev)
            << "decayedStrength should decrease: age " << age_h << " -> " << curr
            << " not < prev " << prev;
        prev = curr;
    }

    // Verify formula at age=100h: 10 * exp(-0.01 * 100) = 10 * exp(-1)
    double expected_100h = 10.0 * std::exp(-1.0);
    EXPECT_NEAR(pm.decayedStrength(lambda, 100.0), expected_100h, 1e-9);
}

// ── TEST 2: Adaptive thresholds asymptote to 0.7 and 0.8 ─────────────────────
TEST(Promotion, AdaptiveThresholdAsymptote) {
    // T1→T2: starts at 0.5, asymptotes to 0.7
    EXPECT_NEAR(PromotionMetrics::adaptiveT1Threshold(0), 0.5, 1e-9);

    // At large count it should be very close to 0.7
    double large_t1 = PromotionMetrics::adaptiveT1Threshold(1000);
    EXPECT_GT(large_t1, 0.69);
    EXPECT_LT(large_t1, 0.70001);

    // Must be strictly monotonically increasing
    double prev_t1 = PromotionMetrics::adaptiveT1Threshold(0);
    for (size_t n : { 1u, 5u, 20u, 100u, 500u }) {
        double next_t1 = PromotionMetrics::adaptiveT1Threshold(n);
        EXPECT_GT(next_t1, prev_t1);
        prev_t1 = next_t1;
    }

    // T2→T3: starts at 0.6, asymptotes to 0.8
    EXPECT_NEAR(PromotionMetrics::adaptiveT2Threshold(0), 0.6, 1e-9);

    double large_t2 = PromotionMetrics::adaptiveT2Threshold(1000);
    EXPECT_GT(large_t2, 0.79);
    EXPECT_LT(large_t2, 0.80001);

    double prev_t2 = PromotionMetrics::adaptiveT2Threshold(0);
    for (size_t n : { 1u, 5u, 20u, 100u, 500u }) {
        double next_t2 = PromotionMetrics::adaptiveT2Threshold(n);
        EXPECT_GT(next_t2, prev_t2);
        prev_t2 = next_t2;
    }
}

// ── TEST 3: Promotion decision logic ─────────────────────────────────────────
// Verify that (raw_score * decayedStrength) crosses adaptiveThreshold as expected.
TEST(Promotion, EndToEndPromotion) {
    // Simulate a memory item:
    PromotionMetrics pm;
    pm.reinforcement_count = 5;
    pm.created_at_hours    = 0.0; // freshly created

    const double now_hours  = 10.0;   // 10 hours later
    const double lambda     = 0.01;

    double strength = pm.decayedStrength(lambda, now_hours);
    // strength = 5 * exp(-0.01 * 10) ≈ 5 * 0.9048 ≈ 4.524
    EXPECT_NEAR(strength, 5.0 * std::exp(-0.1), 1e-6);

    // Threshold at 10 episodic memories: 0.5 + 0.2*(1-exp(-0.1*10)) ≈ 0.5 + 0.2*0.632 ≈ 0.6264
    double t1_thresh = PromotionMetrics::adaptiveT1Threshold(10);

    // If raw_score = 0.15:
    //   adjusted = 0.15 * 4.524 = 0.6786 > 0.6264 → should promote
    double raw_score_promote = 0.15;
    EXPECT_GT(raw_score_promote * strength, t1_thresh)
        << "Should promote: adjusted_score=" << raw_score_promote * strength
        << " vs threshold=" << t1_thresh;

    // If raw_score = 0.01:
    //   adjusted = 0.01 * 4.524 = 0.0452 < 0.6264 → should NOT promote
    double raw_score_no_promote = 0.01;
    EXPECT_LT(raw_score_no_promote * strength, t1_thresh)
        << "Should NOT promote: adjusted_score=" << raw_score_no_promote * strength
        << " vs threshold=" << t1_thresh;

    // Verify T2→T3 threshold at 5 semantic concepts: 0.6 + 0.2*(1-exp(-0.05*5)) ≈ 0.643
    double t2_thresh = PromotionMetrics::adaptiveT2Threshold(5);
    EXPECT_GT(t2_thresh, 0.6);
    EXPECT_LT(t2_thresh, 0.8);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
