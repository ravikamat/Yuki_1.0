#pragma once
// GroundedGenerationBackend — LLM-free generation using LatentTokenProjector
// Generates from latent space with uncertainty quantification and clarification routing

#include "GenerationBackend.h"
#include "LatentTokenProjector.h"
#include <memory>
#include <functional>

namespace yuki {
namespace brain {
namespace language {

class GroundedGenerationBackend : public IGenerationBackend {
public:
    explicit GroundedGenerationBackend(std::shared_ptr<LatentTokenProjector> projector);
    ~GroundedGenerationBackend() override = default;

    // IGenerationBackend interface
    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override { return projector_ != nullptr; }
    BackendKind kind() const override { return BackendKind::GROUNDED; }
    std::string name() const override { return "Grounded (Latent→Token)"; }
    float estimateCost(const GenerationRequest& request) const override { return 0.0f; }

    // Set latent states from cognitive field
    void setLatentStates(const std::array<yuki::brain::predictive::RSSMState, 5>& states);
    
    // Clarification callback for high-uncertainty cases
    using ClarificationCallback = std::function<std::string(const std::string& reason)>;
    void setClarificationCallback(ClarificationCallback cb) { clarification_callback_ = std::move(cb); }

    // Stats
    struct Stats {
        std::atomic<uint64_t> total_generations{0};
        std::atomic<uint64_t> clarifications_triggered{0};
        std::atomic<uint64_t> verifications_passed{0};
        std::atomic<uint64_t> verifications_failed{0};
    };
    const Stats& getStats() const { return stats_; }

private:
    std::shared_ptr<LatentTokenProjector> projector_;
    std::array<yuki::brain::predictive::RSSMState, 5> current_latent_states_;
    bool has_latent_states_ = false;
    
    ClarificationCallback clarification_callback_;
    
    mutable Stats stats_;
};

} // namespace language
} // namespace brain
} // namespace yuki