#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include <algorithm>
#include <limits>
#include <iostream>

namespace yuki::memory {

// Production MLP: 48 input → 128 hidden (ReLU) → 24 output
// Deterministic forward. Policy-gradient learning (REINFORCE).
// Thread-safe for forward (read-only weights). Caller must lock for learn().
class TinyMLP {
public:
    static constexpr int INPUT_DIM = 48;
    static constexpr int HIDDEN_DIM = 128;
    static constexpr int OUTPUT_DIM = 24;
    static constexpr float LEARNING_RATE = 0.01f;
    static constexpr float GRADIENT_CLIP = 1.0f;
    static constexpr float WEIGHT_DECAY = 0.0001f;
    static constexpr float TEMPERATURE = 0.5f; // policy softmax temperature
    static constexpr uint64_t DMC_MLP_SEED_v1 = 0xD3C1175EEDULL;

    TinyMLP();
    explicit TinyMLP(uint64_t seed);

    // Forward: deterministic, read-only on weights. Thread-safe.
    std::array<float, OUTPUT_DIM> forward(const std::array<float, INPUT_DIM>& input) const;

    // REINFORCE policy gradient. NOT thread-safe — caller must hold unique_lock.
    struct LearningSample {
        std::array<float, INPUT_DIM> input;
        std::array<float, OUTPUT_DIM> logits; // stored at decision time
        int sdm_action;    // 0-7 (primary selected)
        int lsh_action;    // 0-7 (primary selected)
        int action_action; // 0-4 (primary selected)
        float reward;      // +1.0 success, -1.0 failure
    };
    void learn(const std::vector<LearningSample>& batch);

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& blob);

    // Integrity
    uint64_t computeWeightHash() const;

    // State
    bool isInitialized() const { return initialized_; }
    int getUpdateCount() const { return update_count_; }
    size_t paramCount() const { return INPUT_DIM * HIDDEN_DIM + HIDDEN_DIM + HIDDEN_DIM * OUTPUT_DIM + OUTPUT_DIM; }

private:
    // Flat arrays for cache efficiency
    std::array<float, INPUT_DIM * HIDDEN_DIM> W1_;
    std::array<float, HIDDEN_DIM> b1_;
    std::array<float, HIDDEN_DIM * OUTPUT_DIM> W2_;
    std::array<float, OUTPUT_DIM> b2_;

    bool initialized_ = false;
    int update_count_ = 0;

    void initWeights(uint64_t seed);
    static float relu(float x) { return x > 0.0f ? x : 0.0f; }
    static float reluDeriv(float x) { return x > 0.0f ? 1.0f : 0.0f; }

    // Group softmax: SDM[0-7], LSH[8-15], Action[16-20], Reserved[21-23]
    static void groupSoftmax(const std::array<float, OUTPUT_DIM>& logits,
                             float temperature,
                             std::array<float, OUTPUT_DIM>& probs);

    static void clipGradient(float& g);
    static bool isFinite(float x);
};

} // namespace yuki::memory
