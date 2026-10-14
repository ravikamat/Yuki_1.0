#include <chrono>
#include <mutex>
#include <algorithm>
#include "brain/predictive/TurnCoordinatorAdapter.h"
#include "brain/field/CognitiveField.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/core/Logger.h"
#include "brain/memory/ActiveInferenceRetrieval.h"
#include "brain/self/RichSelfModel.h"
#include "brain/language/SentenceBuilder.h"
#include "brain/language/LocalLLM.h"
#include "brain/selfimprove/PerTurnLearningHook.h"
#include "brain/selfimprove/WeightCalibratorRegistry.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/perception/TextEncoder.h"
#include "brain/organism/OrganismController.h"
#include "PresenceShell.h"

namespace yuki {
namespace predictive {

AdapterTurnResult TurnCoordinatorAdapter::processTurn(const std::string& user_input,
                                                        std::chrono::milliseconds deadline) {
    
    AdapterTurnResult result;
    
    if (!cognitive_field_) {
        return buildErrorResult("CognitiveField not wired");
    }
    
    if (!cognitive_field_->isRunning()) {
        return buildErrorResult("CognitiveField not running");
    }
    
    // 1. Inject sensory input (non-blocking)
    if (!cognitive_field_->injectText(user_input)) {
        return buildErrorResult("Failed to inject sensory input");
    }
    
    // 2. Wait for cognitive commitment (belief update + policy selection).
    //    This blocks the caller for up to `deadline`; a timeout is expected
    //    under load, so surface it instead of silently degrading (audit §2).
    if (!cognitive_field_->waitForThought(deadline)) {
        yuki::core::Logger::instance().log(
            yuki::core::LogLevel::WARN, "TurnCoordinatorAdapter",
            "waitForThought timed out after " +
                std::to_string(deadline.count()) + "ms; returning current state");
        // Timeout - return current state
        auto snap = cognitive_field_->getSnapshot();
        return buildResultFromThought("I'm still thinking about that...", snap);
    }
    
    // 3. Get the thought (language-ready response)
    std::string thought = cognitive_field_->getThought();
    auto snap = cognitive_field_->getSnapshot();
    
    // 4. Build result
    return buildResultFromThought(thought, snap);
}

AdapterTurnResult TurnCoordinatorAdapter::buildResultFromThought(const std::string& thought,
                                                                    const yuki::brain::field::CognitiveSnapshot& snap) {
    AdapterTurnResult result;
    result.turn_committed = true;
    result.response_text = thought;
    result.confidence = snap.vse.entropy < 0.5f ? 0.8f : 0.5f;  // Low entropy = high confidence
    result.response_tone = "neutral";
    result.can_act = true;
    
    // Determine if clarification needed based on precision
    if (snap.precision.intent < 0.4f) {
        result.requires_clarification = true;
        result.clarify_dimensions.push_back("intent");
        result.template_family = "fallback";
        result.template_slot = "not_sure";
    } else if (snap.precision.entity < 0.4f) {
        result.requires_clarification = true;
        result.clarify_dimensions.push_back("entity");
        result.template_family = "dimension_clarify";
        result.template_slot = "entity";
    } else if (snap.precision.tone < 0.4f) {
        result.requires_clarification = true;
        result.clarify_dimensions.push_back("tone");
        result.template_family = "dimension_clarify";
        result.template_slot = "tone";
    }
    
    // Safety veto
    if (snap.vse.safety_mass > 0.05f) {
        result.veto = true;
        result.requires_clarification = true;
        result.template_family = "safety";
        result.template_slot = "veto";
        result.response_text = "I can't do that — it may not be safe. Are you sure that's what you want?";
    }
    
    // Set dominant intent as template family
    static const char* INTENT_NAMES[8] = {"chitchat", "query", "command", "emotional_vent", "meta_question", "tutorial", "creative", "abort"};
    int dominant_idx = 0;
    float max_prob = 0.0f;
    for (int i = 0; i < 8; ++i) {
        if (snap.vse.q_intent[i] > max_prob) {
            max_prob = snap.vse.q_intent[i];
            dominant_idx = i;
        }
    }
    if (max_prob > 0.0f && result.template_family.empty()) {
        result.template_family = INTENT_NAMES[dominant_idx];
        result.template_slot = "default";
    }
    
    return result;
}

AdapterTurnResult TurnCoordinatorAdapter::buildClarificationResult(const yuki::brain::field::CognitiveSnapshot& snap,
                                                                      const std::string& dimension) {
    AdapterTurnResult result;
    result.turn_committed = true;
    result.requires_clarification = true;
    result.clarify_dimensions.push_back(dimension);
    result.can_act = false;
    result.confidence = 0.3f;
    
    if (dimension == "intent") {
        result.template_family = "fallback";
        result.template_slot = "not_sure";
        result.response_text = "I'm not sure what you'd like me to do. Could you clarify?";
    } else if (dimension == "entity") {
        result.template_family = "dimension_clarify";
        result.template_slot = "entity";
        result.response_text = "Could you be more specific about what you're referring to?";
    } else if (dimension == "tone") {
        result.template_family = "dimension_clarify";
        result.template_slot = "tone";
        result.response_text = "I want to make sure I respond appropriately. How are you feeling about this?";
    } else {
        result.template_family = "dimension_clarify";
        result.template_slot = dimension;
        result.response_text = "Could you clarify " + dimension + "?";
    }
    
    return result;
}

AdapterTurnResult TurnCoordinatorAdapter::buildSafetyVeto(const std::string& reason) {
    AdapterTurnResult result;
    result.turn_committed = true;
    result.veto = true;
    result.requires_clarification = true;
    result.can_act = false;
    result.template_family = "safety";
    result.template_slot = "veto";
    result.response_text = "I can't do that — it may not be safe. " + reason;
    result.confidence = 0.9f;
    return result;
}

AdapterTurnResult TurnCoordinatorAdapter::buildErrorResult(const std::string& error) {
    AdapterTurnResult result;
    result.turn_committed = true;
    result.response_text = "I'm having trouble processing that right now: " + error;
    result.confidence = 0.1f;
    result.response_tone = "neutral";
    result.can_act = true;
    return result;
}

} // namespace predictive
} // namespace yuki