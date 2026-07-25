#include "TheoryOfMind.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace yuki::self {

TheoryOfMind::TheoryOfMind()
    : user_knowledge_vector_{},
      goal_history_ema_{},
      user_trust_(0.5f),
      user_patience_(1.0f),
      interaction_count_(0) {
    reset();
}

float TheoryOfMind::clamp01(float v) const {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

void TheoryOfMind::reset() {
    user_knowledge_vector_.fill(0.0f);
    goal_history_ema_.fill(0.25f);
    user_trust_.store(0.5f, std::memory_order_release);
    user_patience_ = 1.0f;
    interaction_count_.store(0, std::memory_order_release);
}

std::array<float, TheoryOfMind::kKnowledgeDims> TheoryOfMind::extractInputFeatures(const std::string& input) const {
    std::array<float, kKnowledgeDims> features{};
    uint64_t seed = 14695981039346656037ULL;
    for (char c : input) {
        seed ^= static_cast<uint8_t>(c);
        seed *= 1099511628211ULL;
    }
    for (size_t i = 0; i < kKnowledgeDims; ++i) {
        float hash_f = std::sin(static_cast<float>(seed) * static_cast<float>(i + 1) * 12.9898f + 78.233f) * 43758.5453f;
        hash_f = hash_f - std::floor(hash_f);
        features[i] = hash_f;
    }
    return features;
}

float TheoryOfMind::inputLengthFactor(const std::string& input) const {
    return clamp01(static_cast<float>(input.size()) / 100.0f);
}

void TheoryOfMind::updateKnowledgeInference(const std::string& input, bool satisfied) {
    auto features = extractInputFeatures(input);
    for (size_t i = 0; i < kKnowledgeDims; ++i) {
        if (satisfied) {
            user_knowledge_vector_[i] = clamp01(user_knowledge_vector_[i] + kEmaAlpha * features[i]);
        } else {
            user_knowledge_vector_[i] = clamp01(user_knowledge_vector_[i] - kEmaAlpha * features[i] * 0.5f);
        }
    }
}

void TheoryOfMind::observeTurn(const std::string& user_input,
                                const std::string& /*yuki_response*/,
                                bool user_satisfied,
                                const std::array<float, kKnowledgeDims>& /*yuki_competence*/) {
    interaction_count_.fetch_add(1, std::memory_order_acq_rel);

    float current_trust = user_trust_.load(std::memory_order_acquire);
    float trust_update = kEmaAlpha * (user_satisfied ? 1.0f : 0.0f) + (1.0f - kEmaAlpha) * current_trust;
    user_trust_.store(trust_update, std::memory_order_release);

    updateKnowledgeInference(user_input, user_satisfied);

    user_patience_ = kEmaAlpha * (1.0f / (1.0f + inputLengthFactor(user_input))) + (1.0f - kEmaAlpha) * user_patience_;

    auto features = extractInputFeatures(user_input);
    for (size_t j = 0; j < kGoalDims; ++j) {
        goal_history_ema_[j] = kEmaAlpha * features[j] + (1.0f - kEmaAlpha) * goal_history_ema_[j];
    }
}

std::array<float, TheoryOfMind::kKnowledgeDims> TheoryOfMind::inferKnowledgeGap(
    const std::array<float, kKnowledgeDims>& yuki_competence) const {
    std::array<float, kKnowledgeDims> gap{};
    for (size_t i = 0; i < kKnowledgeDims; ++i) {
        gap[i] = clamp01(user_knowledge_vector_[i]) - yuki_competence[i];
    }
    return gap;
}

std::array<float, TheoryOfMind::kGoalDims> TheoryOfMind::predictGoalDistribution() const {
    std::array<float, kGoalDims> dist{};
    float sum = 0.0f;
    for (size_t j = 0; j < kGoalDims; ++j) {
        sum += goal_history_ema_[j];
    }
    if (sum < 1e-6f) {
        return {0.25f, 0.25f, 0.25f, 0.25f};
    }
    for (size_t j = 0; j < kGoalDims; ++j) {
        dist[j] = goal_history_ema_[j] / sum;
    }
    return dist;
}

std::vector<uint8_t> TheoryOfMind::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(82);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;
    float trust = user_trust_.load(std::memory_order_acquire);
    uint64_t count = interaction_count_.load(std::memory_order_acquire);

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));
    append(user_knowledge_vector_.data(), sizeof(float) * kKnowledgeDims);
    append(goal_history_ema_.data(), sizeof(float) * kGoalDims);
    append(&trust, sizeof(trust));
    append(&user_patience_, sizeof(user_patience_));
    append(&count, sizeof(count));

    return out;
}

bool TheoryOfMind::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 82) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    std::memcpy(user_knowledge_vector_.data(), data.data() + offset, sizeof(float) * kKnowledgeDims);
    offset += sizeof(float) * kKnowledgeDims;

    std::memcpy(goal_history_ema_.data(), data.data() + offset, sizeof(float) * kGoalDims);
    offset += sizeof(float) * kGoalDims;

    float trust = 0.5f;
    std::memcpy(&trust, data.data() + offset, sizeof(trust));
    user_trust_.store(trust, std::memory_order_release);
    offset += sizeof(trust);

    std::memcpy(&user_patience_, data.data() + offset, sizeof(user_patience_));
    offset += sizeof(user_patience_);

    uint64_t count = 0;
    std::memcpy(&count, data.data() + offset, sizeof(count));
    interaction_count_.store(count, std::memory_order_release);

    return true;
}

} // namespace yuki::self
