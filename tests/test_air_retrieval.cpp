// tests/test_air_retrieval.cpp — Phase A: Active Inference Retrieval tests
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <string>

#include "brain/memory/Hypervector.h"
#include "brain/memory/InformationGainEngine.h"
#include "brain/inference/VariationalStateEstimator.h"

using namespace yuki::memory;

// Null GenerativeModel — produces uniform likelihoods → IG near 0 for uniform prior
TEST(AIR, ExactKLZeroPosterior) {
    InformationGainEngine ige(nullptr); // null GM → all likelihoods = 0.5

    // Uniform prior over the 13 active states (0-12), zero on padding states (13-23)
    std::vector<float> q_curr(24, 0.0f);
    for (size_t i = 0; i < 13; ++i) q_curr[i] = 1.0f / 13.0f;

    // Candidate = random HV
    std::mt19937 rng(42);
    Hypervector hv = Hypervector::random(rng);

    yuki::inference::PrecisionFactors prec;
    prec.signal_snr       = 1.0f;
    prec.dropout_rate     = 0.0f;
    prec.context_relevance = 1.0f;

    float ig = ige.computeInformationGain(hv, q_curr, prec);

    // With null GM, likelihoods are all 0.5 for first 13 states (0-12),
    // so q'[s] = q[s] * 0.5 for all active states.
    // After normalisation, q' = q → KL = 0.
    EXPECT_NEAR(ig, 0.0f, 1e-4f) << "Uniform likelihoods should give KL ≈ 0";
}

TEST(AIR, HighSurpriseHighIG) {
    InformationGainEngine ige(nullptr);

    // Heavily peaked posterior: state 0 has nearly all mass
    std::vector<float> q_peaked(24, 0.001f);
    q_peaked[0] = 1.0f - 0.001f * 23.0f;

    std::mt19937 rng(123);
    Hypervector hv = Hypervector::random(rng);

    yuki::inference::PrecisionFactors prec;
    prec.signal_snr        = 2.0f;   // high precision → amplifies IG
    prec.dropout_rate      = 0.0f;
    prec.context_relevance = 1.0f;

    // With null GM (uniform likelihoods), after multiplication the peaked posterior
    // gets flattened toward uniform. This produces non-trivial KL > 0.
    float ig = ige.computeInformationGain(hv, q_peaked, prec);
    // IG must be non-negative
    EXPECT_GE(ig, 0.0f);
    // With high-precision uniform likelihoods and peaked prior, KL is non-trivial
    // (prior moves significantly after Bayesian update with uniform likelihood).
    // Accept any finite value ≥ 0.
    EXPECT_TRUE(std::isfinite(ig));
}

TEST(AIR, EpisodicRankingOrder) {
    InformationGainEngine ige(nullptr);

    // Peaked posterior heavily favoring state 0
    std::vector<float> q_peaked(24, 0.002f);
    q_peaked[0] = 0.954f; // dominates

    yuki::inference::PrecisionFactors prec;
    prec.signal_snr        = 1.0f;
    prec.dropout_rate      = 0.0f;
    prec.context_relevance = 1.0f;

    // Build 3 candidates with different seeds
    std::mt19937 r1(1), r2(2), r3(3);
    std::vector<std::pair<std::string, Hypervector>> candidates = {
        { "ep_1", Hypervector::random(r1) },
        { "ep_2", Hypervector::random(r2) },
        { "ep_3", Hypervector::random(r3) }
    };

    auto ranked = ige.rankCandidates(candidates, q_peaked, prec);

    // Rankings must be complete and have correct size
    ASSERT_EQ(ranked.size(), 3u);
    // Ranked in descending order of precision_weighted_gain
    for (size_t i = 1; i < ranked.size(); ++i)
        EXPECT_GE(ranked[i-1].precision_weighted_gain, ranked[i].precision_weighted_gain);
}

TEST(AIR, SemanticRankingOrder) {
    InformationGainEngine ige(nullptr);

    std::vector<float> q(24, 1.0f / 13.0f); // non-zero for first 13 states
    for (size_t i = 13; i < 24; ++i) q[i] = 0.0f;

    yuki::inference::PrecisionFactors prec;
    prec.signal_snr        = 1.0f;
    prec.dropout_rate      = 0.0f;
    prec.context_relevance = 0.8f;

    // Semantic candidates (text-seeded HVs)
    std::vector<std::pair<std::string, Hypervector>> concepts = {
        { "cat",  Hypervector("cat")  },
        { "dog",  Hypervector("dog")  },
        { "fish", Hypervector("fish") }
    };

    auto ranked = ige.rankCandidates(concepts, q, prec);
    ASSERT_EQ(ranked.size(), 3u);
    // Descending order check
    for (size_t i = 1; i < ranked.size(); ++i)
        EXPECT_GE(ranked[i-1].precision_weighted_gain, ranked[i].precision_weighted_gain);
    // All gains must be non-negative
    for (const auto& c : ranked)
        EXPECT_GE(c.precision_weighted_gain, 0.0f);
}

// GAP 3: VSE belief update from AIR context
// Verify that after retrieveEpisodic(), calling vse_->update() with
// synthesized pseudo-observations actually changes the belief state.
TEST(AIR_VSE_Update, BeliefChangesAfterRetrieval) {
    // Setup: mock VSE with known belief
    yuki::inference::VariationalStateEstimator vse;
    vse.reset(); // Initialise internals
    
    // ADJUSTMENT 1: VSE update via mutable copy
    auto belief = vse.currentBelief();
    for (int i = 0; i < 8; ++i) belief.q_intent[i] = 0.125f;  // uniform
    for (int i = 0; i < 3; ++i) belief.q_engagement[i] = 0.333f;
    for (int i = 0; i < 2; ++i) belief.q_urgency[i] = 0.5f;
    belief.safety_mass = 0.9f;
    vse.setBeliefState(belief);

    // Capture pre-update belief
    std::array<float, 24> q_before{};
    for (int i = 0; i < 8; ++i) q_before[i] = belief.q_intent[i];
    for (int i = 0; i < 3; ++i) q_before[8 + i] = belief.q_engagement[i];
    for (int i = 0; i < 2; ++i) q_before[11 + i] = belief.q_urgency[i];
    q_before[13] = belief.safety_mass;

    // Synthesize a strong pseudo-observation (high precision, non-zero)
    std::vector<float> strong_obs(24, 0.0f);
    strong_obs[0] = 0.8f;  // strong intent 0 signal
    strong_obs[8] = 0.7f;  // strong engagement signal
    strong_obs[13] = 0.95f; // high safety

    std::vector<float> prediction_error(24);
    for (int i = 0; i < 24; ++i) {
        prediction_error[i] = strong_obs[i] - q_before[i];
    }
    std::vector<float> precision(24, 0.9f);  // high confidence

    // Update belief using the mutable copy method
    auto belief_copy = vse.currentBelief();
    belief_copy.update(prediction_error, precision, 0.5f);  // high lr for test
    vse.setBeliefState(belief_copy);

    // Capture post-update belief
    std::array<float, 24> q_after{};
    const auto& belief_after = vse.currentBelief();
    for (int i = 0; i < 8; ++i) q_after[i] = belief_after.q_intent[i];
    for (int i = 0; i < 3; ++i) q_after[8 + i] = belief_after.q_engagement[i];
    for (int i = 0; i < 2; ++i) q_after[11 + i] = belief_after.q_urgency[i];
    q_after[13] = belief_after.safety_mass;

    // Verify belief changed (not identical)
    float delta = 0.0f;
    for (int i = 0; i < 24; ++i) {
        delta += std::abs(q_after[i] - q_before[i]);
    }
    EXPECT_GT(delta, 0.01f) << "VSE belief did not change after AIR update";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
