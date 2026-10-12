#pragma once
// BrocaTranslator — Belief-to-Language translation
// Converts VSE belief state into natural language via LocalTransformer
// Uses structured prompts with intent, memory, and context

#include <string>
#include <vector>
#include <optional>
#include "LocalTransformer.h"
#include "VariationalStateEstimator.h"
#include "TurnState.h"
#include "ContextManager.h"
#include "KnowledgeDaemon.h"
#include "NativeSentenceSynthesizer.h"
#include "GrammarEngine.h"
#include "WernickeComprehender.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace language {

struct BrocaConfig {
    float temperature = 0.7f;
    float top_p = 0.95f;
    int max_tokens = 512;
    bool use_kv_cache = true;
    int max_context_tokens = 3500;
};

struct BrocaInput {
    // VSE belief state
    std::array<float, 8> q_intent;
    std::array<float, 3> q_engagement;
    std::array<float, 2> q_urgency;
    float safety_mass = 0.0f;
    float surprise_budget = 0.0f;
    float entropy = 0.0f;
    
    // Current input
    std::string user_input;
    
    // Context
    std::string retrieved_context;
    std::string memory_context;
    std::string user_name;
    int cognitive_intent = 0;  // InputAnalyzer 14-category
    
    // Precision state
    struct PrecisionState {
        float intent = 0.5f;
        float entity = 0.5f;
        float tone = 0.5f;
        float safety = 0.5f;
        float source = 0.5f;
    } precision;
    
    // HPC state
    std::array<float, 24> hpc_prediction_errors{};
    float hpc_free_energy = 0.0f;
    
    // Drive states for motivational modulation
    struct DriveState {
        float curiosity = 0.5f;
        float social = 0.5f;
        float competence = 0.5f;
        float energy = 1.0f;
        float safety_need = 0.1f;
    } drive_states;
};

struct BrocaOutput {
    std::string response_text;
    bool success = false;
    float confidence = 0.0f;
    float latency_ms = 0.0f;
    int tokens_generated = 0;
    std::string failure_reason;
    // For debugging
    std::string prompt_used;
    int map_intent = -1;
};

class BrocaTranslator {
public:
    explicit BrocaTranslator(LocalTransformer* lt = nullptr);
    ~BrocaTranslator() = default;
    
    void setLocalTransformer(LocalTransformer* lt) noexcept { local_transformer_ = lt; }
    void setNativeSentenceSynthesizer(::yuki::language::NativeSentenceSynthesizer* synth) noexcept { native_synthesizer_ = synth; }
    void setGrammarEngine(::yuki::language::GrammarEngine* grammar) noexcept { grammar_engine_ = grammar; }
    void setWernickeComprehender(::yuki::brain::language::WernickeComprehender* w) noexcept { wernicke_ = w; }
    void setHdcGraph(::yuki::memory::HdcSemanticGraph* hdc) noexcept { hdc_graph_ = hdc; }
    void setConfig(const BrocaConfig& cfg) { config_ = cfg; }
    
    // Primary translation: belief → language
    BrocaOutput translate(const BrocaInput& input) const;
    
    // Specialized translations for different response types
    BrocaOutput translateClarification(const BrocaInput& input, const std::string& clarify_dimension) const;
    BrocaOutput translateActionAck(const BrocaInput& input, const std::string& action_result) const;
    BrocaOutput translateSafetyVeto(const BrocaInput& input, const std::string& reason) const;
    BrocaOutput translateFailure(const BrocaInput& input, const std::string& error) const;
    
    // Inner speech (phonological loop) - generates without motor output
    BrocaOutput innerSpeech(const BrocaInput& input) const;
    
    // Deliberation: generate multiple candidates and select best
    BrocaOutput deliberate(const BrocaInput& input, int n_candidates = 3) const;

private:
    LocalTransformer* local_transformer_ = nullptr;
    ::yuki::language::NativeSentenceSynthesizer* native_synthesizer_ = nullptr;
    ::yuki::language::GrammarEngine* grammar_engine_ = nullptr;
    ::yuki::brain::language::WernickeComprehender* wernicke_ = nullptr;
    ::yuki::memory::HdcSemanticGraph* hdc_graph_ = nullptr;
    BrocaConfig config_;
    
    // Build system prompt from belief state
    std::string buildSystemPrompt(const BrocaInput& input) const;
    std::string buildUserPrompt(const BrocaInput& input) const;
    
    // Extract MAP intent from q_intent
    int getMapIntent(const std::array<float, 8>& q_intent) const;
    
    // Format belief summary for prompt
    std::string formatBeliefSummary(const BrocaInput& input) const;
    
    // Select best candidate from multiple generations
    BrocaOutput selectBest(const std::vector<BrocaOutput>& candidates) const;
    
    // Native LLM-free generation fallback
    BrocaOutput generateNative(const BrocaInput& input) const;
    
    // Helpers for native generation
    std::string determineIntentFromVSE(const BrocaInput& input) const;
    std::string determineToneFromVSE(const BrocaInput& input) const;
};

} // namespace language
} // namespace brain
} // namespace yuki