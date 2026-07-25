#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <atomic>
#include <string>

namespace yuki {
namespace metacognition { class MetacognitionEngine; }
namespace organism { class MetabolismEngine; }
}

namespace yuki {
namespace memory { class CognitiveMemoryFabric; }
namespace self {

class SelfModel {
public:
    static constexpr size_t kCapabilityDims = 11; // matches MetacognitionEngine domain count
    static constexpr float kIdentityEmaAlpha = 0.05f;
    static constexpr float kSuccessEmaAlpha = 0.1f;
    static constexpr uint32_t kSerializationMagic = 0x534D4F44; // "SMOD"
    static constexpr uint16_t kSerializationVersion = 1;

    SelfModel();

    // Update from turn outcome and subsystem states.
    // drive_activations: [curiosity, competence, social, homeostasis] in [0,1]
    void update(const std::array<float, kCapabilityDims>& competence_vector,
                float metabolism_viability,
                const std::array<float, 4>& drive_activations,
                bool last_turn_success,
                float last_precision);

    // Identity stability: EMA of how much the capability vector changes per update [0,1]
    float identityStability() const;

    // Drift from last checkpoint (0 = identical, 1 = completely different)
    float identityDrift() const;

    // FNV-1a hash of capability vector for quick identity comparison
    uint64_t identityHash() const;

    // Snapshot current state as drift baseline
    void checkpoint();

    // Sleep consolidation — updates baseline and restores energy
    void consolidate();

    // Summary string
    std::string toString() const;

    // Binary serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    // Accessors
    const std::array<float, kCapabilityDims>& capabilityVector() const { return capability_vector_; }
    float energyLevel() const { return energy_level_; }
    float recentSuccessRate() const { return recent_success_rate_; }
    uint64_t turnCount() const { return turn_count_.load(std::memory_order_acquire); }

    void loadFromCMF(yuki::memory::CognitiveMemoryFabric* cmf);

private:
    std::array<float, kCapabilityDims> capability_vector_;
    std::array<float, kCapabilityDims> checkpoint_vector_;
    float energy_level_;
    float recent_success_rate_;
    std::atomic<uint64_t> turn_count_;
    float identity_stability_;

    float vectorDelta(const std::array<float, kCapabilityDims>& a,
                      const std::array<float, kCapabilityDims>& b) const;
    float clamp01(float v) const;
};

} // namespace self
} // namespace yuki
