#pragma once
#include "TinyMLP.h"
#include "ProceduralStore.h"
#include <array>
#include <vector>
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <cstdint>
#include <string>

namespace yuki::memory {

struct DMCDecision {
    // SDM write targets (top-3 of 8 hard locations)
    std::array<int, 3> sdm_top3_indices;
    std::array<float, 3> sdm_top3_weights;

    // LSH probe targets (top-2 of 8 tables)
    std::array<int, 2> lsh_top2_indices;
    std::array<float, 2> lsh_top2_weights;

    // Action
    enum Action { READ=0, WRITE=1, FORGET=2, PROMOTE=3, NOP=4 };
    Action action;
    float action_confidence;

    // Safety envelope override
    bool safety_override = false;
    std::string safety_reason;
};

struct DecisionToken {
    size_t ring_index;
    int64_t timestamp;
};

struct TurnOutcome {
    std::array<float, TinyMLP::INPUT_DIM> input;
    std::array<float, TinyMLP::OUTPUT_DIM> logits;
    DMCDecision decision;
    bool retrieval_success = false;
    float retrieval_precision = 0.0f;
    int64_t timestamp = 0;
    bool recorded = false;
};

class DifferentialMemoryController {
public:
    DifferentialMemoryController();
    ~DifferentialMemoryController();

    bool init(ProceduralStore* store, const std::string& weight_key);

    // Evaluate: thread-safe (shared lock on weights). Returns decision + token.
    std::pair<DMCDecision, DecisionToken> evaluate(
        const std::array<float, 24>& vse_posterior,
        const std::array<float, 24>& context_features);

    // Record outcome: thread-safe (brief lock on ring buffer).
    void recordOutcome(const DecisionToken& token, bool success, float precision);

    // Consolidate: called by SleepThread. Unique lock on weights.
    bool consolidate();

    // Persistence
    bool saveWeights();
    bool loadWeights();

    // Stats
    int getOutcomeCount() const { return total_outcomes_.load(); }
    int getUpdateCount() const { return mlp_.getUpdateCount(); }

    // Safety configuration
    struct SafetyConfig {
        float min_confidence_threshold = 0.5f;
        float max_entropy_for_write = 2.0f;
        int min_outcomes_before_learning = 10;
    };
    void setSafetyConfig(const SafetyConfig& cfg);

private:
    TinyMLP mlp_;
    ProceduralStore* store_ = nullptr;
    std::string weight_key_;

    static constexpr size_t RING_SIZE = 256;
    std::array<TurnOutcome, RING_SIZE> outcomes_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    std::mutex outcome_mu_;

    mutable std::shared_mutex weight_mutex_;
    SafetyConfig safety_cfg_;
    std::atomic<int> total_outcomes_{0};

    std::array<float, 24> assembleContextFeatures(
        const std::array<float, 24>& vse_posterior,
        const std::array<float, 24>& raw_context);

    DMCDecision extractDecision(const std::array<float, TinyMLP::OUTPUT_DIM>& logits);
    DMCDecision applySafetyEnvelope(const DMCDecision& proposed,
                                    const std::array<float, 24>& vse_posterior);
    DMCDecision conservativeFallback() const;
    bool performLearning(const std::vector<TurnOutcome>& batch);
};

} // namespace yuki::memory
