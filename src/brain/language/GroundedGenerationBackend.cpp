#include "brain/language/GroundedGenerationBackend.h"
#include "brain/language/LatentTokenProjector.h"
#include "brain/core/Logger.h"

namespace yuki {
namespace brain {
namespace language {

GroundedGenerationBackend::GroundedGenerationBackend(std::shared_ptr<LatentTokenProjector> projector)
    : projector_(std::move(projector)) {}

void GroundedGenerationBackend::setLatentStates(
    const std::array<yuki::brain::predictive::RSSMState, 5>& states) {
    current_latent_states_ = states;
    has_latent_states_ = true;
}

GenerationResult GroundedGenerationBackend::generate(const GenerationRequest& request) {
    if (!projector_ || !has_latent_states_) {
        GenerationResult result;
        result.success = false;
        result.text = "";
        result.error = "GroundedGenerationBackend: no projector or latent states";
        return result;
    }

    stats_.total_generations++;

    // Generate from latent states
    auto proj_result = projector_->generateFromLatent(
        current_latent_states_,
        {}, // No prompt tokens for now
        request.maxTokens,
        request.temperature,
        40,
        request.topP
    );

    // Check if clarification is needed
    if (proj_result.needs_clarification && clarification_callback_) {
        stats_.clarifications_triggered++;
        std::string clarification = clarification_callback_(proj_result.clarification_reason);
        GenerationResult result;
        result.success = true;
        result.text = clarification;
        result.backend = BackendKind::GROUNDED;
        result.backendKind = BackendKind::GROUNDED;
        result.clarificationRequested = true;
        result.clarificationReason = proj_result.clarification_reason;
        return result;
    }

    // Verify against LWM if enabled
    if (projector_->verifyAgainstLWM(proj_result.tokens, current_latent_states_).passed) {
        stats_.verifications_passed++;
    } else {
        stats_.verifications_failed++;
    }

    GenerationResult result;
    result.success = true;
    result.text = ""; // Tokens need to be detokenized
    result.tokens = proj_result.tokens;
    result.backend = BackendKind::GROUNDED;
    result.backendKind = BackendKind::GROUNDED;
    result.confidence = proj_result.sequence_confidence;
    result.surprise = proj_result.surprise;
    result.entropy = proj_result.entropy;
    return result;
}

} // namespace language
} // namespace brain
} // namespace yuki