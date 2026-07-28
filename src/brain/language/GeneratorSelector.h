// ============================================================================
// YUKI v1.0 - GeneratorSelector Implementation
// Single authoritative arbitration gate before response assembly.
// ============================================================================
#pragma once

#include "brain/predictive/TurnState.h"
#include <string>
#include <vector>

namespace yuki {
namespace language {

enum class GeneratorType {
    TRANSFORMER_PRIMARY,
    REASONING_VERBALIZATION,
    VAE_PLUS_TRANSFORMER,
    PLANNER_AND_TOOL,
    CLARIFY_QUESTION,
    SAFE_DEFERRAL,
    PCFG_FALLBACK
};

struct GeneratorDecision {
    GeneratorType selectedType;
    std::string rationale;
    float confidenceScore;
    bool requiresApproval;
};

class GeneratorSelector {
public:
    GeneratorSelector() = default;
    ~GeneratorSelector() = default;

    GeneratorDecision select(const PredictionState& state) const {
        GeneratorDecision decision;
        decision.requiresApproval = false;

        // 1. Safety Veto Check
        if (state.precision.safety < 0.50f || state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::SECURITY_ALERT) {
            decision.selectedType = GeneratorType::SAFE_DEFERRAL;
            decision.rationale = "Safety alert or low safety precision detected; deferring for human approval.";
            decision.confidenceScore = 0.95f;
            decision.requiresApproval = true;
            return decision;
        }

        // 2. Tool Execution Action Check
        if (state.analyzed_input.inputType == yuki::input::InputType::COMMAND || state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::COMMAND) {
            decision.selectedType = GeneratorType::PLANNER_AND_TOOL;
            decision.rationale = "Command/Tool request detected; passing to action planner.";
            decision.confidenceScore = 0.90f;
            return decision;
        }

        // 3. Causal / Reasoning Intent Check
        if (state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::CAUSAL_QUERY ||
            state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::COUNTERFACTUAL ||
            state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::ANALOGY_REQUEST ||
            state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::MATHEMATICAL) {
            decision.selectedType = GeneratorType::REASONING_VERBALIZATION;
            decision.rationale = "Causal/Logic query detected; verbalizing reasoning organ outputs.";
            decision.confidenceScore = 0.88f;
            return decision;
        }

        // 4. Creative Generation Intent Check
        if (state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::CREATIVE_GENERATION ||
            state.analyzed_input.cognitiveIntent == yuki::input::CognitiveIntent::METAPHOR_QUERY) {
            decision.selectedType = GeneratorType::VAE_PLUS_TRANSFORMER;
            decision.rationale = "Creative intent detected; sampling VAE latent space + Transformer.";
            decision.confidenceScore = 0.82f;
            return decision;
        }

        // 5. Low Confidence Clarification Check
        if (state.analyzed_input.confidence < 0.40f) {
            decision.selectedType = GeneratorType::CLARIFY_QUESTION;
            decision.rationale = "Input confidence below threshold; requesting user clarification.";
            decision.confidenceScore = 0.70f;
            return decision;
        }

        // 6. Default Primary Language Cortex
        decision.selectedType = GeneratorType::TRANSFORMER_PRIMARY;
        decision.rationale = "Standard conversational turn; using primary language transformer.";
        decision.confidenceScore = 0.92f;
        return decision;
    }
};

} // namespace language
} // namespace yuki
