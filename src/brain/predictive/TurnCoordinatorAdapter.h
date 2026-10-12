#pragma once
// TurnCoordinatorAdapter — Thin adapter from TurnCoordinator to CognitiveField
// Replaces complex turn pipeline with: injectSensory → waitForCommit → getThought → Broca

#include <memory>
#include <string>
#include <chrono>
#include "brain/field/CognitiveField.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/predictive/TurnState.h"

namespace yuki {
namespace predictive {

// Simplified turn result for adapter
struct AdapterTurnResult {
    bool turn_committed = false;
    bool can_act = true;
    bool requires_clarification = false;
    std::string response_text;
    std::string response_tone = "neutral";
    float confidence = 0.0f;
    std::vector<std::string> clarify_dimensions;
    bool veto = false;
    std::string template_family;
    std::string template_slot;
};

class TurnCoordinatorAdapter {
public:
    TurnCoordinatorAdapter() = default;
    ~TurnCoordinatorAdapter() = default;
    
    void setCognitiveField(yuki::brain::field::CognitiveField* cf) { cognitive_field_ = cf; }
    void setBrocaTranslator(yuki::brain::language::BrocaTranslator* broca) { broca_ = broca; }
    void setWernickeComprehender(yuki::brain::language::WernickeComprehender* wernicke) { wernicke_ = wernicke; }
    
    // Main entry: process user input → response
    AdapterTurnResult processTurn(const std::string& user_input, 
                                   std::chrono::milliseconds deadline = std::chrono::seconds(5));
    
    // Get cognitive field for direct access
    yuki::brain::field::CognitiveField* field() { return cognitive_field_; }

private:
    yuki::brain::field::CognitiveField* cognitive_field_ = nullptr;
    yuki::brain::language::BrocaTranslator* broca_ = nullptr;
    yuki::brain::language::WernickeComprehender* wernicke_ = nullptr;
    
    // Helpers
    AdapterTurnResult buildResultFromThought(const std::string& thought, const yuki::brain::field::CognitiveSnapshot& snap);
    AdapterTurnResult buildClarificationResult(const yuki::brain::field::CognitiveSnapshot& snap, const std::string& dimension);
    AdapterTurnResult buildSafetyVeto(const std::string& reason);
    AdapterTurnResult buildErrorResult(const std::string& error);
};

} // namespace predictive
} // namespace yuki