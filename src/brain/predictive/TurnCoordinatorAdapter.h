#pragma once
// TurnCoordinatorAdapter — Unified turn coordinator using CognitiveField
// Replaces both legacy TurnCoordinator and old adapter with single unified pipeline
// Features: CognitiveField integration, ActiveInferenceRetrieval, AIR, SelfModel,
// SentenceBuilder, PresenceShell, OrganismController, LocalLLM, PerTurnLearningHook

#include <memory>
#include <string>
#include <chrono>
#include "brain/field/CognitiveField.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/predictive/TurnState.h"
#include "brain/memory/ActiveInferenceRetrieval.h"
#include "brain/language/SentenceBuilder.h"
#include "brain/self/RichSelfModel.h"
#include "brain/language/LocalLLM.h"
#include "brain/selfimprove/PerTurnLearningHook.h"
#include "brain/selfimprove/WeightCalibratorRegistry.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/perception/TextEncoder.h"
#include "brain/organism/OrganismController.h"
#include "PresenceShell.h"

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
    
    // Core dependencies
    void setCognitiveField(yuki::brain::field::CognitiveField* cf) { cognitive_field_ = cf; }
    void setBrocaTranslator(yuki::brain::language::BrocaTranslator* broca) { broca_ = broca; }
    void setWernickeComprehender(yuki::brain::language::WernickeComprehender* wernicke) { wernicke_ = wernicke; }
    
    // Extended dependencies (from legacy TurnCoordinator)
    void setActiveInferenceRetrieval(yuki::memory::ActiveInferenceRetrieval* air) { air_ = air; }
    void setSelfModel(yuki::brain::self::RichSelfModel* sm) { self_model_ = sm; }
    void setSentenceBuilder(yuki::language::SentenceBuilder* sb) { sentence_builder_ = sb; }
    void setPresenceShell(PresenceShell* shell) { presence_shell_ = shell; }
    void setOrganismController(yuki::organism::OrganismController* oc) { organism_controller_ = oc; }
    void setLocalLLM(yuki::brain::language::LocalLLM* llm) { local_llm_ = llm; }
    void setPerTurnLearningHook(yuki::selfimprove::PerTurnLearningHook* hook) { per_turn_hook_ = hook; }
    void setWeightCalibratorRegistry(yuki::selfimprove::WeightCalibratorRegistry* reg) { calibrator_registry_ = reg; }
    void setMemoryFabric(yuki::brain::memory::CognitiveMemoryFabric* cmf) { cmf_ = cmf; }
    void setTextEncoder(yuki::perception::TextEncoder* te) { text_encoder_ = te; }
    
    // Main entry: process user input → response
    AdapterTurnResult processTurn(const std::string& user_input, 
                                   std::chrono::milliseconds deadline = std::chrono::seconds(5));
    
    // Get cognitive field for direct access
    yuki::brain::field::CognitiveField* field() { return cognitive_field_; }
    yuki::brain::self::RichSelfModel* getSelfModel() const { return self_model_; }

private:
    yuki::brain::field::CognitiveField* cognitive_field_ = nullptr;
    yuki::brain::language::BrocaTranslator* broca_ = nullptr;
    yuki::brain::language::WernickeComprehender* wernicke_ = nullptr;
    
    // Extended dependencies
    yuki::memory::ActiveInferenceRetrieval* air_ = nullptr;
    yuki::brain::self::RichSelfModel* self_model_ = nullptr;
    yuki::language::SentenceBuilder* sentence_builder_ = nullptr;
    PresenceShell* presence_shell_ = nullptr;
    yuki::organism::OrganismController* organism_controller_ = nullptr;
    yuki::brain::language::LocalLLM* local_llm_ = nullptr;
    yuki::selfimprove::PerTurnLearningHook* per_turn_hook_ = nullptr;
    yuki::selfimprove::WeightCalibratorRegistry* calibrator_registry_ = nullptr;
    yuki::brain::memory::CognitiveMemoryFabric* cmf_ = nullptr;
    yuki::perception::TextEncoder* text_encoder_ = nullptr;
    
    // Helpers
    AdapterTurnResult buildResultFromThought(const std::string& thought, const yuki::brain::field::CognitiveSnapshot& snap);
    AdapterTurnResult buildClarificationResult(const yuki::brain::field::CognitiveSnapshot& snap, const std::string& dimension);
    AdapterTurnResult buildSafetyVeto(const std::string& reason);
    AdapterTurnResult buildErrorResult(const std::string& error);
};

} // namespace predictive
} // namespace yuki