#pragma once
// WernickeComprehender — Language-to-Belief translation
// Converts natural language input into VSE belief updates
// Uses LocalTransformer embeddings + structured parsing

#include <string>
#include <vector>
#include <array>
#include <optional>
#include "LocalTransformer.h"
#include "VariationalStateEstimator.h"
#include "../../input/encoding/TextEncoder.h"

namespace yuki {
namespace brain {
namespace language {

struct WernickeConfig {
    float temperature = 0.1f;  // Low temp for deterministic parsing
    int max_tokens = 128;
    bool use_embeddings = true;
    float confidence_threshold = 0.6f;
};

struct WernickeInput {
    std::string user_text;
    std::string context;  // Conversation history
    // Current belief state (for incremental update)
    std::array<float, 8> q_intent;
    std::array<float, 3> q_engagement;
    std::array<float, 2> q_urgency;
    float safety_mass = 0.0f;
};

struct WernickeOutput {
    // Belief update (delta from current)
    std::array<float, 8> intent_delta{};
    std::array<float, 3> engagement_delta{};
    std::array<float, 2> urgency_delta{};
    float safety_delta = 0.0f;
    
    // Parsed features
    float question_score = 0.0f;
    float command_score = 0.0f;
    float emotional_score = 0.0f;
    float technical_score = 0.0f;
    float greeting_score = 0.0f;
    float urgency_score = 0.0f;
    float action_score = 0.0f;
    float polarity_score = 0.0f;
    float phatic_score = 0.0f;
    
    // Cognitive intent (14-category)
    int cognitive_intent = 0;
    float cognitive_confidence = 0.0f;
    
    // Entities and references
    std::vector<std::string> entities;
    std::vector<std::string> coreferences;
    
    // Quality metrics
    float overall_confidence = 0.0f;
    bool success = false;
    std::string failure_reason;
    float latency_ms = 0.0f;
};

class WernickeComprehender {
public:
    explicit WernickeComprehender(LocalTransformer* lt = nullptr, perception::TextEncoder* te = nullptr);
    ~WernickeComprehender() = default;
    
    void setLocalTransformer(LocalTransformer* lt) noexcept { local_transformer_ = lt; }
    void setTextEncoder(perception::TextEncoder* te) noexcept { text_encoder_ = te; }
    void setConfig(const WernickeConfig& cfg) { config_ = cfg; }
    
    // Primary comprehension: language → belief update
    WernickeOutput comprehend(const WernickeInput& input) const;
    
    // Quick classification without full belief update
    WernickeOutput classifyOnly(const std::string& text) const;
    
    // Extract entities and coreferences
    void extractEntities(const std::string& text, std::vector<std::string>& entities, std::vector<std::string>& corefs) const;
    
    // Compute text embedding for HDC/VSE
    std::vector<float> embedText(const std::string& text) const;

private:
    LocalTransformer* local_transformer_ = nullptr;
    perception::TextEncoder* text_encoder_ = nullptr;
    WernickeConfig config_;
    
    // Build parsing prompt
    std::string buildParsePrompt(const WernickeInput& input) const;
    
    // Parse LocalTransformer output into structured features
    bool parseLLMOutput(const std::string& llm_output, WernickeOutput& output) const;
    
    // Fallback to TextEncoder for feature extraction
    void fallbackToTextEncoder(const std::string& text, WernickeOutput& output) const;
    
    // Merge LLM and TextEncoder features
    void mergeFeatures(const WernickeOutput& llm_features, const WernickeOutput& te_features, WernickeOutput& merged) const;
};

} // namespace language
} // namespace brain
} // namespace yuki