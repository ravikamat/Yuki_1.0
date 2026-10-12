#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cmath>
#include <random>

namespace yuki::selfimprove {

// ─────────────────────────────────────────────────────────────────────────────
// Bayesian Knowledge Tracing (BKT) — tracks P(know | evidence) per item
// ─────────────────────────────────────────────────────────────────────────────
struct BKTParams {
    float p_learn = 0.1f;      // P(learn | not know, attempt)
    float p_forget = 0.0f;     // P(forget | know) — usually 0 for skills
    float p_guess = 0.2f;      // P(correct | not know) — random success
    float p_slip = 0.1f;       // P(incorrect | know) — careless error
    float p_know_0 = 0.5f;     // Prior P(know) before any evidence
};

struct BKTState {
    float p_know = 0.5f;       // Current posterior P(know)
    uint32_t observations = 0; // Total evidence count
    uint32_t correct = 0;      // Correct observations
    uint64_t last_update = 0;  // Timestamp
};

class BKTCalibrator {
public:
    explicit BKTCalibrator(const BKTParams& params = BKTParams{}) noexcept
        : params_(params) {}

    // Update posterior given observed outcome (true = correct, false = incorrect)
    float update(bool correct) noexcept {
        float p_know = state_.p_know;
        float p_correct_if_know = 1.0f - params_.p_slip;
        float p_correct_if_not = params_.p_guess;

        // P(obs | know) and P(obs | not know)
        float p_obs_given_know = correct ? p_correct_if_know : params_.p_slip;
        float p_obs_given_not = correct ? p_correct_if_not : (1.0f - params_.p_guess);

        // Bayes: P(know | obs) = P(obs | know) * P(know) / P(obs)
        float numerator = p_obs_given_know * p_know;
        float denominator = numerator + p_obs_given_not * (1.0f - p_know);
        float post = (denominator > 0.0f) ? numerator / denominator : p_know;

        // Apply learning transition: P(know_next) = post + (1 - post) * p_learn
        p_know = post + (1.0f - post) * params_.p_learn;

        // Apply forgetting (usually 0 for persistent skills)
        p_know = p_know * (1.0f - params_.p_forget);

        state_.p_know = std::clamp(p_know, 0.0f, 1.0f);
        state_.observations++;
        if (correct) state_.correct++;

        return state_.p_know;
    }

    // Get current mastery probability
    float mastery() const noexcept { return state_.p_know; }

    // Get confidence interval (Wilson score interval)
    std::pair<float, float> confidenceInterval(float z = 1.96f) const noexcept {
        uint32_t n = state_.observations;
        if (n == 0) return {0.0f, 1.0f};
        float p = static_cast<float>(state_.correct) / n;
        float denom = 1.0f + z * z / n;
        float centre = (p + z * z / (2 * n)) / denom;
        float half = (z * std::sqrt(p * (1 - p) / n + z * z / (4 * n * n))) / denom;
        return {(std::max)(0.0f, centre - half), (std::min)(1.0f, centre + half)};
    }

    // Reset to prior
    void reset(float prior = -1.0f) noexcept {
        state_.p_know = (prior >= 0.0f) ? prior : params_.p_know_0;
        state_.observations = 0;
        state_.correct = 0;
    }

    // Serialize for persistence
    struct Serialized {
        float p_know;
        uint32_t observations;
        uint32_t correct;
        BKTParams params;
    };
    Serialized serialize() const noexcept {
        return {state_.p_know, state_.observations, state_.correct, params_};
    }
    void deserialize(const Serialized& s) noexcept {
        state_.p_know = s.p_know;
        state_.observations = s.observations;
        state_.correct = s.correct;
        params_ = s.params;
    }

private:
    BKTParams params_;
    BKTState state_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ELO-style Rating — tracks relative strength per item vs context
// ─────────────────────────────────────────────────────────────────────────────
struct ELOParams {
    float k_factor = 32.0f;        // Learning rate (16-32 typical)
    float initial_rating = 1200.0f; // Starting ELO
    float min_rating = 100.0f;      // Floor
    float max_rating = 3000.0f;     // Ceiling
    float context_rating = 1200.0f; // Opponent/context base rating
};

struct ELOState {
    float rating = 1200.0f;
    uint32_t games = 0;
    uint32_t wins = 0;
    uint32_t losses = 0;
    uint32_t draws = 0;
};

class ELOCalibrator {
public:
    explicit ELOCalibrator(const ELOParams& params = ELOParams{}) noexcept
        : params_(params), state_{} {
        state_.rating = params_.initial_rating;
    }

    // Record match outcome: result = 1.0 (win), 0.5 (draw), 0.0 (loss)
    float update(float result, float opponent_rating = -1.0f) noexcept {
        float opp = (opponent_rating >= 0.0f) ? opponent_rating : params_.context_rating;

        // Expected score: 1 / (1 + 10^((opp - rating)/400))
        float expected = 1.0f / (1.0f + std::pow(10.0f, (opp - state_.rating) / 400.0f));

        // Update rating
        state_.rating += params_.k_factor * (result - expected);
        state_.rating = std::clamp(state_.rating, params_.min_rating, params_.max_rating);

        state_.games++;
        if (result > 0.75f) state_.wins++;
        else if (result < 0.25f) state_.losses++;
        else state_.draws++;

        return state_.rating;
    }

    // Update with binary outcome (true = win, false = loss)
    float update(bool win, float opponent_rating = -1.0f) noexcept {
        return update(win ? 1.0f : 0.0f, opponent_rating);
    }

    // Get current rating
    float rating() const noexcept { return state_.rating; }
    const ELOParams& params() const noexcept { return params_; }

    // Win probability against given opponent
    float winProbability(float opponent_rating) const noexcept {
        return 1.0f / (1.0f + std::pow(10.0f, (opponent_rating - state_.rating) / 400.0f));
    }

    // Win probability against default context
    float winProbability() const noexcept {
        return winProbability(params_.context_rating);
    }

    // Convert rating to [0,1] weight (sigmoid centered at context_rating)
    float toWeight(float scale = 400.0f) const noexcept {
        return 1.0f / (1.0f + std::exp(-(state_.rating - params_.context_rating) / scale));
    }

    void reset(float initial = -1.0f) noexcept {
        state_.rating = (initial >= 0.0f) ? initial : params_.initial_rating;
        state_.games = state_.wins = state_.losses = state_.draws = 0;
    }

    struct Serialized {
        float rating;
        uint32_t games, wins, losses, draws;
        ELOParams params;
    };
    Serialized serialize() const noexcept {
        return {state_.rating, state_.games, state_.wins, state_.losses, state_.draws, params_};
    }
    void deserialize(const Serialized& s) noexcept {
        state_.rating = s.rating;
        state_.games = s.games;
        state_.wins = s.wins;
        state_.losses = s.losses;
        state_.draws = s.draws;
        params_ = s.params;
    }

private:
    ELOParams params_;
    ELOState state_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Multi-armed Bandit Calibrator — Thompson Sampling for weight selection
// ─────────────────────────────────────────────────────────────────────────────
class ThompsonSampler {
public:
    struct Arm {
        float alpha = 1.0f;  // Successes + 1
        float beta = 1.0f;   // Failures + 1
        uint32_t pulls = 0;
    };

    explicit ThompsonSampler(size_t num_arms = 1) : arms_(num_arms, Arm{}) {}

    // Sample from Beta(alpha, beta) for each arm, return index of max
    size_t select() const noexcept {
        static thread_local std::mt19937 rng{
            static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
        float best = -1.0f;
        size_t best_idx = 0;
        for (size_t i = 0; i < arms_.size(); ++i) {
            std::gamma_distribution<float> d1(arms_[i].alpha, 1.0f);
            std::gamma_distribution<float> d2(arms_[i].beta, 1.0f);
            float sample = d1(rng) / (d1(rng) + d2(rng));
            if (sample > best) {
                best = sample;
                best_idx = i;
            }
        }
        return best_idx;
    }

    void update(size_t arm_idx, bool reward) noexcept {
        if (arm_idx >= arms_.size()) return;
        if (reward) arms_[arm_idx].alpha += 1.0f;
        else arms_[arm_idx].beta += 1.0f;
        arms_[arm_idx].pulls++;
    }

    // Expected value of each arm
    std::vector<float> expectedValues() const noexcept {
        std::vector<float> ev(arms_.size());
        for (size_t i = 0; i < arms_.size(); ++i) {
            ev[i] = arms_[i].alpha / (arms_[i].alpha + arms_[i].beta);
        }
        return ev;
    }

    void reset() noexcept {
        for (auto& arm : arms_) {
            arm.alpha = 1.0f;
            arm.beta = 1.0f;
            arm.pulls = 0;
        }
    }

private:
    std::vector<Arm> arms_;
};

// ─────────────────────────────────────────────────────────────────────────────
// SelfWeightCalibrator — unified interface combining BKT, ELO, Thompson
// ─────────────────────────────────────────────────────────────────────────────
enum class CalibrationMode {
    BKT,      // Bayesian Knowledge Tracing — best for skill/mastery tracking
    ELO,      // ELO rating — best for competitive/relative strength
    THOMPSON, // Thompson Sampling — best for exploration/exploitation
    HYBRID    // Weighted combination of BKT + ELO
};

struct CalibratorConfig {
    CalibrationMode mode = CalibrationMode::HYBRID;
    float bkt_weight = 0.6f;     // Weight for BKT in hybrid
    float elo_weight = 0.4f;     // Weight for ELO in hybrid
    BKTParams bkt_params{};
    ELOParams elo_params{};
    size_t thompson_arms = 1;    // For multi-threshold bandit
};

class SelfWeightCalibrator {
public:
    explicit SelfWeightCalibrator(const CalibratorConfig& cfg = CalibratorConfig{}) noexcept
        : config_(cfg),
          bkt_(cfg.bkt_params),
          elo_(cfg.elo_params),
          thompson_(cfg.thompson_arms) {}

    // Record outcome and get calibrated weight in [0,1]
    float record(bool success, float context_strength = 0.5f) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);

        float weight = 0.5f;

        switch (config_.mode) {
            case CalibrationMode::BKT:
                weight = bkt_.update(success);
                break;

            case CalibrationMode::ELO:
                elo_.update(success, lerp(elo_.params().context_rating, 800.0f, context_strength));
                weight = elo_.toWeight();
                break;

            case CalibrationMode::THOMPSON: {
                size_t arm = thompson_.select();
                thompson_.update(arm, success);
                auto ev = thompson_.expectedValues();
                weight = ev[arm];
                break;
            }

            case CalibrationMode::HYBRID: {
                float bkt_w = bkt_.update(success);
                elo_.update(success, lerp(elo_.params().context_rating, 800.0f, context_strength));
                float elo_w = elo_.toWeight();
                weight = config_.bkt_weight * bkt_w + config_.elo_weight * elo_w;
                break;
            }
        }

        last_weight_ = std::clamp(weight, 0.0f, 1.0f);
        return last_weight_;
    }

    // Record with explicit result in [0,1] (1=success, 0=failure, 0.5=partial)
    float record(float result, float context_strength = 0.5f) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        float weight = 0.5f;

        switch (config_.mode) {
            case CalibrationMode::BKT:
                // BKT expects binary; treat >0.5 as success
                weight = bkt_.update(result > 0.5f);
                break;

            case CalibrationMode::ELO:
                elo_.update(result, lerp(elo_.params().context_rating, 800.0f, context_strength));
                weight = elo_.toWeight();
                break;

            case CalibrationMode::THOMPSON: {
                size_t arm = thompson_.select();
                thompson_.update(arm, result > 0.5f);
                auto ev = thompson_.expectedValues();
                weight = ev[arm];
                break;
            }

            case CalibrationMode::HYBRID: {
                float bkt_w = bkt_.update(result > 0.5f);
                elo_.update(result, lerp(elo_.params().context_rating, 800.0f, context_strength));
                float elo_w = elo_.toWeight();
                weight = config_.bkt_weight * bkt_w + config_.elo_weight * elo_w;
                break;
            }
        }

        last_weight_ = std::clamp(weight, 0.0f, 1.0f);
        return last_weight_;
    }

    // Get current calibrated weight without updating
    float weight() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_weight_;
    }

    // Get diagnostics
    struct Diagnostics {
        float weight = 0.5f;
        float bkt_mastery = 0.5f;
        float elo_rating = 1200.0f;
        float elo_winprob = 0.5f;
        uint32_t bkt_observations = 0;
        uint32_t elo_games = 0;
        std::pair<float, float> bkt_ci = {0.0f, 1.0f};
    };
    Diagnostics diagnostics() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        Diagnostics d;
        d.weight = last_weight_;
        d.bkt_mastery = bkt_.mastery();
        d.bkt_observations = bkt_.serialize().observations;
        d.bkt_ci = bkt_.confidenceInterval();
        d.elo_rating = elo_.rating();
        d.elo_winprob = elo_.winProbability();
        d.elo_games = elo_.serialize().games;
        return d;
    }

    // Reset all calibrators
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        bkt_.reset();
        elo_.reset();
        thompson_.reset();
        last_weight_ = 0.5f;
    }

    // Set mode at runtime
    void setMode(CalibrationMode mode) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
    }

    // Serialize for persistence
    struct Serialized {
        CalibratorConfig config;
        BKTCalibrator::Serialized bkt;
        ELOCalibrator::Serialized elo;
        float last_weight;
    };
    Serialized serialize() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return {config_, bkt_.serialize(), elo_.serialize(), last_weight_};
    }
    void deserialize(const Serialized& s) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = s.config;
        bkt_.deserialize(s.bkt);
        elo_.deserialize(s.elo);
        last_weight_ = s.last_weight;
    }

    // Move support (required for unordered_map emplace)
    SelfWeightCalibrator(SelfWeightCalibrator&& other) noexcept
        : config_(std::move(other.config_)),
          bkt_(std::move(other.bkt_)),
          elo_(std::move(other.elo_)),
          thompson_(std::move(other.thompson_)),
          last_weight_(other.last_weight_) {}

    SelfWeightCalibrator& operator=(SelfWeightCalibrator&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::lock_guard<std::mutex> other_lock(other.mutex_);
            config_ = std::move(other.config_);
            bkt_ = std::move(other.bkt_);
            elo_ = std::move(other.elo_);
            thompson_ = std::move(other.thompson_);
            last_weight_ = other.last_weight_;
        }
        return *this;
    }

    SelfWeightCalibrator(const SelfWeightCalibrator&) = delete;
    SelfWeightCalibrator& operator=(const SelfWeightCalibrator&) = delete;

private:
    static float lerp(float a, float b, float t) noexcept {
        return a + t * (b - a);
    }

    CalibratorConfig config_;
    mutable std::mutex mutex_;
    BKTCalibrator bkt_;
    ELOCalibrator elo_;
    ThompsonSampler thompson_;
    float last_weight_ = 0.5f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-key calibrator registry — manages many SelfWeightCalibrator instances
// ─────────────────────────────────────────────────────────────────────────────
class WeightCalibratorRegistry {
public:
    using Key = std::string;

    // Get or create calibrator for a key
    SelfWeightCalibrator& get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = calibrators_.find(key);
        if (it == calibrators_.end()) {
            it = calibrators_.emplace(key, SelfWeightCalibrator{}).first;
        }
        return it->second;
    }

    // Get calibrator (const) — returns nullptr if not found
    const SelfWeightCalibrator* get(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = calibrators_.find(key);
        return (it != calibrators_.end()) ? &it->second : nullptr;
    }

    // Record outcome for a key
    float record(const Key& key, bool success, float context_strength = 0.5f) {
        return get(key).record(success, context_strength);
    }
    float record(const Key& key, float result, float context_strength = 0.5f) {
        return get(key).record(result, context_strength);
    }

    // Get weight for a key
    float weight(const Key& key) const {
        const SelfWeightCalibrator* cal = get(key);
        return cal ? cal->weight() : 0.5f;
    }

    // Reset a specific key
    void reset(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = calibrators_.find(key);
        if (it != calibrators_.end()) it->second.reset();
    }

    // Reset all
    void resetAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [k, v] : calibrators_) v.reset();
    }

    // Get all diagnostics
    std::unordered_map<Key, SelfWeightCalibrator::Diagnostics> allDiagnostics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<Key, SelfWeightCalibrator::Diagnostics> result;
        result.reserve(calibrators_.size());
        for (const auto& [k, v] : calibrators_) {
            result[k] = v.diagnostics();
        }
        return result;
    }

    // Serialize all
    struct SerializedEntry {
        Key key;
        SelfWeightCalibrator::Serialized data;
    };
    std::vector<SerializedEntry> serializeAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<SerializedEntry> out;
        out.reserve(calibrators_.size());
        for (const auto& [k, v] : calibrators_) {
            out.push_back({k, v.serialize()});
        }
        return out;
    }
    void deserializeAll(const std::vector<SerializedEntry>& entries) {
        std::lock_guard<std::mutex> lock(mutex_);
        calibrators_.clear();
        for (const auto& e : entries) {
            calibrators_[e.key].deserialize(e.data);
        }
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<Key, SelfWeightCalibrator> calibrators_;
};

} // namespace yuki::selfimprove