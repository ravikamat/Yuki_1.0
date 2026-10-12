#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <atomic>
#include <memory>

namespace yuki {
namespace memory { class EpisodicStore; }
namespace brain { namespace memory { class IdentityHeuristicAccess; } }
namespace swarm { class IIdentityStore; }
}

namespace yuki::self {

class TheoryOfMind {
public:
    static constexpr size_t kKnowledgeDims = 11; // matches SelfModel capability dims
    static constexpr size_t kGoalDims = 4;       // info, action, social, meta
    // First-run seed default
    static constexpr float kDefaultEmaAlpha = 0.1f;
    static constexpr uint32_t kSerializationMagic = 0x544F4D44; // "TOMD"
    static constexpr uint16_t kSerializationVersion = 1;

    TheoryOfMind();
    ~TheoryOfMind();

    /// Bind identity store for dynamic EMA alpha.
    void configure(yuki::swarm::IIdentityStore& store);
    /// Reinforce trust/patience EMA based on turn outcome.
    void onTrustOutcome(bool userSatisfied);

    // Observe a turn interaction. yuki_response may be empty for pre-response observation.
    void observeTurn(const std::string& user_input,
                     const std::string& yuki_response,
                     bool user_satisfied,
                     const std::array<float, kKnowledgeDims>& yuki_competence);

    // Per-domain knowledge gap: positive = user knows more, negative = YUKI knows more
    std::array<float, kKnowledgeDims> inferKnowledgeGap(
        const std::array<float, kKnowledgeDims>& yuki_competence) const;

    // Overload accepting SelfModel reference directly
    std::array<float, kKnowledgeDims> inferKnowledgeGap(const class SelfModel& self_model) const;

    // Normalized goal distribution [info, action, social, meta]
    std::array<float, kGoalDims> predictGoalDistribution() const;

    // Accessors
    float userTrust() const { return user_trust_.load(std::memory_order_acquire); }
    float userPatience() const { return user_patience_; }
    uint64_t interactionCount() const { return interaction_count_.load(std::memory_order_acquire); }

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    void reset();

private:
    std::array<float, kKnowledgeDims> user_knowledge_vector_;
    std::array<float, kGoalDims> goal_history_ema_;
    std::atomic<float> user_trust_;
    float user_patience_;
    std::atomic<uint64_t> interaction_count_;

    // Dynamic EMA alpha — loaded from IIdentityStore, self-adjusts per turn
    float emaAlpha_{kDefaultEmaAlpha};
    std::unique_ptr<yuki::brain::memory::IdentityHeuristicAccess> heuristic_;

    void updateKnowledgeInference(const std::string& input, bool satisfied);
    std::array<float, kKnowledgeDims> extractInputFeatures(const std::string& input) const;
    float inputLengthFactor(const std::string& input) const;
    float clamp01(float v) const;
};

} // namespace yuki::self
