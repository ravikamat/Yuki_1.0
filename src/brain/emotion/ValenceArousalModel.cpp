#include "ValenceArousalModel.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace yuki::emotion {

ValenceArousalModel::ValenceArousalModel()
    : valence_(0.0f),
      arousal_(0.0f),
      valence_ema_(0.0f),
      arousal_ema_(0.0f) {
    reset();
}

float ValenceArousalModel::clamp(float v, float lo, float hi) const {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void ValenceArousalModel::reset() {
    valence_ema_ = 0.0f;
    arousal_ema_ = 0.0f;
    valence_.store(0.0f, std::memory_order_release);
    arousal_.store(0.0f, std::memory_order_release);
}

void ValenceArousalModel::update(float outcome_reward,
                                 float user_feedback_signal,
                                 float task_difficulty,
                                 float surprise) {
    float raw_valence = kEmaAlpha * (outcome_reward * 0.4f + user_feedback_signal * 0.4f + (0.5f - task_difficulty) * 0.2f) + (1.0f - kEmaAlpha) * valence_ema_;
    float raw_arousal = kEmaAlpha * (surprise * 0.5f + task_difficulty * 0.3f + std::abs(outcome_reward) * 0.2f) + (1.0f - kEmaAlpha) * arousal_ema_;

    valence_ema_ = clamp(raw_valence, kValenceMin, kValenceMax);
    arousal_ema_ = clamp(raw_arousal, kArousalMin, kArousalMax);

    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);
}

float ValenceArousalModel::modulateThreshold(float base_threshold) const {
    float arousal_factor = 1.0f + 0.15f * (arousal() - 0.5f);
    float valence_factor = 1.0f - 0.1f * valence();
    return base_threshold * arousal_factor * valence_factor;
}

void ValenceArousalModel::decay() {
    valence_ema_ *= kDecayRate;
    arousal_ema_ *= kDecayRate;
    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);
}

std::vector<uint8_t> ValenceArousalModel::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(14);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));
    append(&valence_ema_, sizeof(valence_ema_));
    append(&arousal_ema_, sizeof(arousal_ema_));

    return out;
}

bool ValenceArousalModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 14) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    std::memcpy(&valence_ema_, data.data() + offset, sizeof(valence_ema_));
    offset += sizeof(valence_ema_);

    std::memcpy(&arousal_ema_, data.data() + offset, sizeof(arousal_ema_));

    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);

    return true;
}

} // namespace yuki::emotion
