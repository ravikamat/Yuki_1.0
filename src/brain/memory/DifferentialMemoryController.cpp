#include "DifferentialMemoryController.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace yuki::memory {

DifferentialMemoryController::DifferentialMemoryController() = default;
DifferentialMemoryController::~DifferentialMemoryController() = default;

bool DifferentialMemoryController::init(ProceduralStore* store,
                                         const std::string& weight_key) {
    store_ = store;
    weight_key_ = weight_key;

    if (loadWeights()) {
        std::cout << "[DMC] Loaded weights: " << weight_key_ << "\n";
        return true;
    }

    std::cout << "[DMC] Cold start — seeding MLP.\n";
    uint64_t seed = TinyMLP::DMC_MLP_SEED_v1;
    for (char c : weight_key) seed = seed * 31 + static_cast<uint64_t>(c);
    mlp_ = TinyMLP(seed);

    if (!saveWeights()) {
        std::cerr << "[DMC] Failed to save initial weights.\n";
    }
    return mlp_.isInitialized();
}

std::pair<DMCDecision, DecisionToken> DifferentialMemoryController::evaluate(
    const std::array<float, 24>& vse_posterior,
    const std::array<float, 24>& context_features)
{
    std::shared_lock<std::shared_mutex> lock(weight_mutex_);

    if (!mlp_.isInitialized()) {
        return {conservativeFallback(), DecisionToken{0, 0}};
    }

    // Assemble 48-dim input
    std::array<float, TinyMLP::INPUT_DIM> input;
    for (int i = 0; i < 24; ++i) input[i] = vse_posterior[i];
    for (int i = 0; i < 24; ++i) input[24 + i] = context_features[i];

    // Forward
    auto logits = mlp_.forward(input);
    auto decision = extractDecision(logits);
    decision = applySafetyEnvelope(decision, vse_posterior);

    // Store in ring buffer
    size_t idx = head_.fetch_add(1, std::memory_order_relaxed) % RING_SIZE;
    outcomes_[idx].input = input;
    outcomes_[idx].logits = logits;
    outcomes_[idx].decision = decision;
    outcomes_[idx].retrieval_success = false;
    outcomes_[idx].retrieval_precision = 0.0f;
    outcomes_[idx].timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    outcomes_[idx].recorded = false;

    total_outcomes_.fetch_add(1, std::memory_order_relaxed);

    return {decision, DecisionToken{idx, outcomes_[idx].timestamp}};
}

void DifferentialMemoryController::recordOutcome(const DecisionToken& token,
                                                  bool success, float precision)
{
    if (token.ring_index >= RING_SIZE) {
        std::cerr << "[DMC] Invalid token index.\n";
        return;
    }

    // Verify timestamp matches (detect stale/overwrite)
    if (outcomes_[token.ring_index].timestamp != token.timestamp) {
        std::cerr << "[DMC] Token stale — outcome overwritten.\n";
        return;
    }

    std::lock_guard<std::mutex> lock(outcome_mu_);
    outcomes_[token.ring_index].retrieval_success = success;
    outcomes_[token.ring_index].retrieval_precision = precision;
    outcomes_[token.ring_index].recorded = true;
}

bool DifferentialMemoryController::consolidate() {
    std::unique_lock<std::shared_mutex> lock(weight_mutex_);

    // Drain completed outcomes
    std::vector<TurnOutcome> batch;
    {
        std::lock_guard<std::mutex> olock(outcome_mu_);
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);

        // Limit the drain to not loop infinitely if head advances.
        // Also ring buffers can loop back.
        size_t count = 0;
        while (t != h && count < RING_SIZE) {
            size_t idx = t % RING_SIZE;
            if (outcomes_[idx].recorded) {
                batch.push_back(outcomes_[idx]);
            }
            t = (t + 1) % RING_SIZE;
            count++;
        }
        tail_.store(t, std::memory_order_release);
    }

    if (batch.size() < static_cast<size_t>(safety_cfg_.min_outcomes_before_learning)) {
        std::cout << "[DMC] Consolidate: " << batch.size() << " outcomes, need "
                  << safety_cfg_.min_outcomes_before_learning << ".\n";
        return false;
    }

    return performLearning(batch);
}

bool DifferentialMemoryController::performLearning(const std::vector<TurnOutcome>& batch) {
    std::vector<TinyMLP::LearningSample> samples;
    samples.reserve(batch.size());

    for (const auto& o : batch) {
        TinyMLP::LearningSample s;
        s.input = o.input;
        s.logits = o.logits;
        s.sdm_action = o.decision.sdm_top3_indices[0];
        s.lsh_action = o.decision.lsh_top2_indices[0];
        s.action_action = static_cast<int>(o.decision.action);
        s.reward = o.retrieval_success ? +1.0f : -1.0f;
        samples.push_back(s);
    }

    mlp_.learn(samples);

    if (!saveWeights()) {
        std::cerr << "[DMC] Weight save failed after consolidation.\n";
    }

    std::cout << "[DMC] Consolidated " << batch.size() << " outcomes. Updates: "
              << mlp_.getUpdateCount() << "\n";
    return true;
}

bool DifferentialMemoryController::saveWeights() {
    if (!store_) return false;
    auto blob = mlp_.serialize();
    return store_->store(weight_key_, ProceduralStore::BlobType::DMC_WEIGHTS, blob);
}

bool DifferentialMemoryController::loadWeights() {
    if (!store_) return false;
    auto blob = store_->retrieve(weight_key_);
    if (!blob) return false;
    return mlp_.deserialize(*blob);
}

std::array<float, 24> DifferentialMemoryController::assembleContextFeatures(
    const std::array<float, 24>& vse_posterior,
    const std::array<float, 24>& raw_context)
{
    (void)vse_posterior;
    // Context features are already 24-dim from caller
    // Future: compute derived features (entropy, recency, etc.)
    return raw_context;
}

DMCDecision DifferentialMemoryController::extractDecision(
    const std::array<float, TinyMLP::OUTPUT_DIM>& logits)
{
    DMCDecision d;

    // SDM group: 0-7 — argmax top-3
    {
        std::array<std::pair<float, int>, 8> scores;
        for (int i = 0; i < 8; ++i) scores[i] = {logits[i], i};
        std::partial_sort(scores.begin(), scores.begin() + 3, scores.end(),
                          std::greater<>());
        for (int i = 0; i < 3; ++i) {
            d.sdm_top3_indices[i] = scores[i].second;
            d.sdm_top3_weights[i] = scores[i].first;
        }
    }

    // LSH group: 8-15 — argmax top-2
    {
        std::array<std::pair<float, int>, 8> scores;
        for (int i = 0; i < 8; ++i) scores[i] = {logits[8 + i], i};
        std::partial_sort(scores.begin(), scores.begin() + 2, scores.end(),
                          std::greater<>());
        for (int i = 0; i < 2; ++i) {
            d.lsh_top2_indices[i] = scores[i].second;
            d.lsh_top2_weights[i] = scores[i].first;
        }
    }

    // Action group: 16-20 — argmax
    {
        int best = 16;
        float best_score = logits[16];
        for (int i = 17; i < 21; ++i) {
            if (logits[i] > best_score) {
                best_score = logits[i];
                best = i;
            }
        }
        d.action = static_cast<DMCDecision::Action>(best - 16);
        d.action_confidence = best_score;
    }

    return d;
}

DMCDecision DifferentialMemoryController::applySafetyEnvelope(
    const DMCDecision& proposed,
    const std::array<float, 24>& vse_posterior)
{
    DMCDecision safe = proposed;

    // C1: Never Commit Early — if confidence too low, force NOP
    if (safe.action_confidence < safety_cfg_.min_confidence_threshold) {
        safe.action = DMCDecision::NOP;
        safe.safety_override = true;
        safe.safety_reason = "C1: confidence below threshold";
        return safe;
    }

    // C3: Know Thy Ignorance — high entropy in VSE → don't write/promote
    float entropy = 0.0f;
    for (int i = 0; i < 24; ++i) {
        if (vse_posterior[i] > 1e-6f) {
            entropy -= vse_posterior[i] * std::log(vse_posterior[i]);
        }
    }
    if (entropy > safety_cfg_.max_entropy_for_write &&
        (safe.action == DMCDecision::WRITE || safe.action == DMCDecision::PROMOTE)) {
        safe.action = DMCDecision::READ;
        safe.safety_override = true;
        safe.safety_reason = "C3: high entropy, forcing READ";
    }

    // C7: Thou Shalt Not Deceive Thyself — FORGET is dangerous, require high confidence
    if (safe.action == DMCDecision::FORGET && safe.action_confidence < 0.8f) {
        safe.action = DMCDecision::NOP;
        safe.safety_override = true;
        safe.safety_reason = "C7: FORGET requires confidence >= 0.8";
    }

    return safe;
}

DMCDecision DifferentialMemoryController::conservativeFallback() const {
    DMCDecision d;
    d.sdm_top3_indices = {0, 1, 2};
    d.sdm_top3_weights = {0.0f, 0.0f, 0.0f};
    d.lsh_top2_indices = {0, 1};
    d.lsh_top2_weights = {0.0f, 0.0f};
    d.action = DMCDecision::NOP;
    d.action_confidence = 0.0f;
    d.safety_override = true;
    d.safety_reason = "MLP not initialized — conservative fallback";
    return d;
}

void DifferentialMemoryController::setSafetyConfig(const SafetyConfig& cfg) {
    safety_cfg_ = cfg;
}

} // namespace yuki::memory
