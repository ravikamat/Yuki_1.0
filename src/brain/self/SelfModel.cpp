#include "SelfModel.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace yuki { namespace memory { class CognitiveMemoryFabric; } }

namespace yuki::self {

SelfModel::SelfModel()
    : capability_vector_{},
      checkpoint_vector_{},
      energy_level_(1.0f),
      recent_success_rate_(1.0f),
      turn_count_(0),
      identity_stability_(0.0f) {
    capability_vector_.fill(0.0f);
    checkpoint_vector_.fill(0.0f);
    checkpoint();
}

float SelfModel::clamp01(float v) const {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

float SelfModel::vectorDelta(const std::array<float, kCapabilityDims>& a,
                            const std::array<float, kCapabilityDims>& b) const {
    float diff = 0.0f;
    for (size_t i = 0; i < kCapabilityDims; ++i) {
        diff += std::abs(a[i] - b[i]);
    }
    return diff / static_cast<float>(kCapabilityDims);
}

void SelfModel::update(const std::array<float, kCapabilityDims>& competence_vector,
                       float metabolism_viability,
                       const std::array<float, 4>& /*drive_activations*/,
                       bool last_turn_success,
                       float /*last_precision*/) {
    capability_vector_ = competence_vector;
    energy_level_ = clamp01(metabolism_viability);
    recent_success_rate_ = kSuccessEmaAlpha * (last_turn_success ? 1.0f : 0.0f) +
                           (1.0f - kSuccessEmaAlpha) * recent_success_rate_;
    turn_count_.fetch_add(1, std::memory_order_acq_rel);

    float delta = vectorDelta(capability_vector_, checkpoint_vector_);
    identity_stability_ = kIdentityEmaAlpha * delta + (1.0f - kIdentityEmaAlpha) * identity_stability_;
}

float SelfModel::identityStability() const {
    return identity_stability_;
}

float SelfModel::identityDrift() const {
    return vectorDelta(capability_vector_, checkpoint_vector_);
}

uint64_t SelfModel::identityHash() const {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(capability_vector_.data());
    size_t size = kCapabilityDims * sizeof(float);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

void SelfModel::checkpoint() {
    checkpoint_vector_ = capability_vector_;
}

void SelfModel::consolidate() {
    checkpoint();
    energy_level_ = std::min(1.0f, energy_level_ + 0.1f);
}

std::string SelfModel::toString() const {
    std::ostringstream oss;
    oss << "SelfModel [turns=" << turn_count_.load()
        << ", energy=" << energy_level_
        << ", success_rate=" << recent_success_rate_
        << ", stability=" << identity_stability_ << "]";
    return oss.str();
}

std::vector<uint8_t> SelfModel::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(70);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;
    uint64_t turns = turn_count_.load(std::memory_order_acquire);

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));
    append(capability_vector_.data(), sizeof(float) * kCapabilityDims);
    append(&energy_level_, sizeof(energy_level_));
    append(&recent_success_rate_, sizeof(recent_success_rate_));
    append(&turns, sizeof(turns));
    append(&identity_stability_, sizeof(identity_stability_));

    return out;
}

bool SelfModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 70) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    std::memcpy(capability_vector_.data(), data.data() + offset, sizeof(float) * kCapabilityDims);
    offset += sizeof(float) * kCapabilityDims;

    std::memcpy(&energy_level_, data.data() + offset, sizeof(energy_level_));
    offset += sizeof(energy_level_);

    std::memcpy(&recent_success_rate_, data.data() + offset, sizeof(recent_success_rate_));
    offset += sizeof(recent_success_rate_);

    uint64_t turns = 0;
    std::memcpy(&turns, data.data() + offset, sizeof(turns));
    turn_count_.store(turns, std::memory_order_release);
    offset += sizeof(turns);

    std::memcpy(&identity_stability_, data.data() + offset, sizeof(identity_stability_));

    return true;
}

void SelfModel::loadFromCMF(yuki::memory::CognitiveMemoryFabric* /*cmf*/) {
    // Advisory hook — CMF integration ready for M10 persistence
}

} // namespace yuki::self
