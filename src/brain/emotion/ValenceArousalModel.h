#pragma once
#include <cstdint>
#include <vector>
#include <atomic>

namespace yuki::emotion {

class ValenceArousalModel {
public:
    static constexpr float kDecayRate = 0.95f;
    static constexpr float kEmaAlpha = 0.1f;
    static constexpr float kValenceMin = -1.0f;
    static constexpr float kValenceMax = 1.0f;
    static constexpr float kArousalMin = 0.0f;
    static constexpr float kArousalMax = 1.0f;
    static constexpr uint32_t kSerializationMagic = 0x56414D4F; // "VAMO"
    static constexpr uint16_t kSerializationVersion = 1;

    ValenceArousalModel();

    // Update from turn outcome signals.
    // outcome_reward: +1 success, -1 failure, 0 neutral
    // user_feedback_signal: +1 approval, -1 rejection, 0 none
    // task_difficulty: [0,1]
    // surprise: [0,1] from FreeEnergy / VSE
    void update(float outcome_reward,
                float user_feedback_signal,
                float task_difficulty,
                float surprise);

    // Modulate a decision threshold.
    // High arousal → higher threshold (more cautious).
    // Negative valence → higher threshold (more defensive).
    // Positive valence → lower threshold (more optimistic).
    float modulateThreshold(float base_threshold) const;

    // Accessors
    float valence() const { return valence_.load(std::memory_order_acquire); }
    float arousal() const { return arousal_.load(std::memory_order_acquire); }
    float valenceEma() const { return valence_ema_; }
    float arousalEma() const { return arousal_ema_; }

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    void reset();
    void decay(); // apply kDecayRate to both dimensions

private:
    std::atomic<float> valence_;
    std::atomic<float> arousal_;
    float valence_ema_;
    float arousal_ema_;

    float clamp(float v, float lo, float hi) const;
};

} // namespace yuki::emotion
