#include "brain/language/WernickeComprehender.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <cctype>
#include <regex>

namespace yuki {
namespace brain {
namespace language {

// Cognitive intent names (14 categories from InputAnalyzer)
static const char* COG_INTENT_NAMES[14] = {
    "CAUSAL", "COUNTERFACTUAL", "ANALOGY", "ABSTRACTION",
    "PLANNING", "SOCIAL_REASONING", "MORAL", "METACOGNITIVE",
    "LANGUAGE", "MATH", "CODE", "SCIENTIFIC", "CREATIVE", "NAVIGATION"
};

WernickeComprehender::WernickeComprehender(LocalTransformer* lt, perception::TextEncoder* te)
    : local_transformer_(lt), text_encoder_(te) {}

std::string WernickeComprehender::buildParsePrompt(const WernickeInput& input) const {
    std::ostringstream oss;
    
    oss << "Analyze the user's input and output a JSON object with the following fields:\n\n";
    oss << "{\n";
    oss << "  \"intent_scores\": [q0..q7],  // CHITCHAT, QUERY, COMMAND, EMOTIONAL_VENT, META_QUESTION, TUTORIAL, CREATIVE, ABORT\n";
    oss << "  \"engagement_scores\": [q0..q2],  // LOW, MEDIUM, HIGH\n";
    oss << "  \"urgency_scores\": [q0..q1],  // NORMAL, URGENT\n";
    oss << "  \"safety_mass\": float,\n";
    oss << "  \"features\": {\n";
    oss << "    \"question\": float, \"command\": float, \"emotional\": float,\n";
    oss << "    \"technical\": float, \"greeting\": float, \"urgency\": float,\n";
    oss << "    \"action\": float, \"polarity\": float, \"phatic\": float\n";
    oss << "  },\n";
    oss << "  \"cognitive_intent\": int,  // 0-13\n";
    oss << "  \"cognitive_confidence\": float,\n";
    oss << "  \"entities\": [\"string\"],\n";
    oss << "  \"coreferences\": [\"string\"]\n";
    oss << "}\n\n";
    
    // Current belief context
    oss << "[Current Belief Context]\n";
    oss << "Intent: [";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) oss << ", ";
        oss << input.q_intent[i];
    }
    oss << "]\n";
    oss << "Engagement: [" << input.q_engagement[0] << ", " << input.q_engagement[1] << ", " << input.q_engagement[2] << "]\n";
    oss << "Urgency: [" << input.q_urgency[0] << ", " << input.q_urgency[1] << "]\n";
    oss << "Safety: " << input.safety_mass << "\n\n";
    
    if (!input.context.empty()) {
        oss << "[Conversation Context]\n" << input.context << "\n\n";
    }
    
    oss << "[User Input]\n" << input.user_text << "\n\n";
    oss << "[Output JSON]";
    
    return oss.str();
}

bool WernickeComprehender::parseLLMOutput(const std::string& llm_output, WernickeOutput& output) const {
    try {
        // Find JSON in the output
        size_t start = llm_output.find('{');
        size_t end = llm_output.rfind('}');
        
        if (start == std::string::npos || end == std::string::npos || end <= start) {
            return false;
        }
        
        std::string json_str = llm_output.substr(start, end - start + 1);
        auto j = nlohmann::json::parse(json_str);
        
        // Parse intent scores
        if (j.contains("intent_scores") && j["intent_scores"].is_array()) {
            auto arr = j["intent_scores"];
            for (int i = 0; i < 8 && i < arr.size(); ++i) {
                output.intent_delta[i] = arr[i].get<float>();
            }
        }
        
        // Parse engagement scores
        if (j.contains("engagement_scores") && j["engagement_scores"].is_array()) {
            auto arr = j["engagement_scores"];
            for (int i = 0; i < 3 && i < arr.size(); ++i) {
                output.engagement_delta[i] = arr[i].get<float>();
            }
        }
        
        // Parse urgency scores
        if (j.contains("urgency_scores") && j["urgency_scores"].is_array()) {
            auto arr = j["urgency_scores"];
            for (int i = 0; i < 2 && i < arr.size(); ++i) {
                output.urgency_delta[i] = arr[i].get<float>();
            }
        }
        
        // Parse safety
        if (j.contains("safety_mass")) {
            output.safety_delta = j["safety_mass"].get<float>();
        }
        
        // Parse features
        if (j.contains("features") && j["features"].is_object()) {
            auto f = j["features"];
            output.question_score = f.value("question", 0.0f);
            output.command_score = f.value("command", 0.0f);
            output.emotional_score = f.value("emotional", 0.0f);
            output.technical_score = f.value("technical", 0.0f);
            output.greeting_score = f.value("greeting", 0.0f);
            output.urgency_score = f.value("urgency", 0.0f);
            output.action_score = f.value("action", 0.0f);
            output.polarity_score = f.value("polarity", 0.0f);
            output.phatic_score = f.value("phatic", 0.0f);
        }
        
        // Parse cognitive intent
        if (j.contains("cognitive_intent")) {
            output.cognitive_intent = j["cognitive_intent"].get<int>();
            output.cognitive_intent = std::clamp(output.cognitive_intent, 0, 13);
        }
        
        if (j.contains("cognitive_confidence")) {
            output.cognitive_confidence = j["cognitive_confidence"].get<float>();
        }
        
        // Parse entities
        if (j.contains("entities") && j["entities"].is_array()) {
            for (const auto& e : j["entities"]) {
                output.entities.push_back(e.get<std::string>());
            }
        }
        
        // Parse coreferences
        if (j.contains("coreferences") && j["coreferences"].is_array()) {
            for (const auto& c : j["coreferences"]) {
                output.coreferences.push_back(c.get<std::string>());
            }
        }
        
        // Compute overall confidence
        float sum_conf = 0.0f;
        int count = 0;
        for (float v : output.intent_delta) { sum_conf += v; count++; }
        for (float v : output.engagement_delta) { sum_conf += v; count++; }
        for (float v : output.urgency_delta) { sum_conf += v; count++; }
        output.overall_confidence = count > 0 ? sum_conf / count : 0.0f;
        output.overall_confidence = std::clamp(output.overall_confidence, 0.0f, 1.0f);
        
        output.success = output.overall_confidence >= config_.confidence_threshold;
        
        return output.success;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "WernickeComprehender",
            "Failed to parse LLM output: " + std::string(e.what()));
        return false;
    }
}

void WernickeComprehender::fallbackToTextEncoder(const std::string& text, WernickeOutput& output) const {
    if (!text_encoder_) return;
    
    text_encoder_->encode(text);
    auto scores = text_encoder_->getLastScores();
    
    output.question_score = scores.question;
    output.command_score = scores.command;
    output.emotional_score = scores.emotional;
    output.technical_score = scores.technical;
    output.greeting_score = scores.greeting;
    output.urgency_score = scores.urgency;
    output.action_score = scores.action;
    output.polarity_score = scores.polarity;
    output.phatic_score = scores.phatic;
    
    // Derive intent from features
    output.intent_delta.fill(0.0f);
    if (output.question_score > 0.4f) output.intent_delta[1] = output.question_score;  // QUERY
    else if (output.command_score > 0.4f) output.intent_delta[2] = output.command_score;  // COMMAND
    else if (output.emotional_score > 0.4f) output.intent_delta[3] = output.emotional_score;  // EMOTIONAL_VENT
    else if (output.greeting_score > 0.4f) output.intent_delta[0] = output.greeting_score;  // CHITCHAT
    else if (output.technical_score > 0.4f) output.intent_delta[5] = output.technical_score;  // TUTORIAL
    else output.intent_delta[0] = 0.3f;  // Default CHITCHAT
    
    output.engagement_delta.fill(0.0f);
    if (output.action_score > 0.5f || output.urgency_score > 0.5f) {
        output.engagement_delta[2] = 0.7f;  // HIGH
    } else if (output.question_score > 0.3f || output.command_score > 0.3f) {
        output.engagement_delta[1] = 0.6f;  // MEDIUM
    } else {
        output.engagement_delta[0] = 0.5f;  // LOW
    }
    
    output.urgency_delta.fill(0.0f);
    if (output.urgency_score > 0.5f) {
        output.urgency_delta[1] = output.urgency_score;  // URGENT
    } else {
        output.urgency_delta[0] = 0.8f;  // NORMAL
    }
    
    output.safety_delta = 0.0f;  // TextEncoder doesn't do safety
    
    // Cognitive intent heuristic
    output.cognitive_intent = 0;  // Default CAUSAL
    if (output.technical_score > 0.5f) output.cognitive_intent = 10;  // CODE
    else if (output.question_score > 0.5f) output.cognitive_intent = 8;  // LANGUAGE
    
    output.cognitive_confidence = 0.5f;
    
    // Entities (simple extraction)
    extractEntities(text, output.entities, output.coreferences);
    
    output.overall_confidence = 0.5f;
    output.success = true;
}

void WernickeComprehender::extractEntities(const std::string& text, std::vector<std::string>& entities, std::vector<std::string>& corefs) const {
    // Simple entity extraction: capitalized words, quoted strings
    std::regex capitalized("\\b[A-Z][a-z]+(?:\\s+[A-Z][a-z]+)*\\b");
    std::regex quoted("\"([^\"]+)\"");
    std::regex coref_pattern("\\b(?:he|she|it|they|this|that|these|those)\\b", std::regex_constants::icase);
    
    std::smatch match;
    std::string::const_iterator search_start = text.cbegin();
    
    while (std::regex_search(search_start, text.cend(), match, capitalized)) {
        std::string entity = match[0];
        if (entity.size() > 1 && entity != "I" && entity != "The" && entity != "A" && entity != "An") {
            entities.push_back(entity);
        }
        search_start = match.suffix().first;
    }
    
    search_start = text.cbegin();
    while (std::regex_search(search_start, text.cend(), match, quoted)) {
        entities.push_back(match[1]);
        search_start = match.suffix().first;
    }
    
    search_start = text.cbegin();
    while (std::regex_search(search_start, text.cend(), match, coref_pattern)) {
        corefs.push_back(match[0]);
        search_start = match.suffix().first;
    }
    
    // Deduplicate
    std::sort(entities.begin(), entities.end());
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    std::sort(corefs.begin(), corefs.end());
    corefs.erase(std::unique(corefs.begin(), corefs.end()), corefs.end());
}

void WernickeComprehender::mergeFeatures(const WernickeOutput& llm_features, const WernickeOutput& te_features, WernickeOutput& merged) const {
    // Weighted merge: prefer LLM when confident, otherwise TextEncoder
    float llm_weight = llm_features.success ? 0.7f : 0.0f;
    float te_weight = 1.0f - llm_weight;
    
    if (llm_weight == 0.0f) {
        merged = te_features;
        return;
    }
    
    for (int i = 0; i < 8; ++i) {
        merged.intent_delta[i] = llm_weight * llm_features.intent_delta[i] + te_weight * te_features.intent_delta[i];
    }
    for (int i = 0; i < 3; ++i) {
        merged.engagement_delta[i] = llm_weight * llm_features.engagement_delta[i] + te_weight * te_features.engagement_delta[i];
    }
    for (int i = 0; i < 2; ++i) {
        merged.urgency_delta[i] = llm_weight * llm_features.urgency_delta[i] + te_weight * te_features.urgency_delta[i];
    }
    merged.safety_delta = llm_weight * llm_features.safety_delta + te_weight * te_features.safety_delta;
    
    merged.question_score = llm_weight * llm_features.question_score + te_weight * te_features.question_score;
    merged.command_score = llm_weight * llm_features.command_score + te_weight * te_features.command_score;
    merged.emotional_score = llm_weight * llm_features.emotional_score + te_weight * te_features.emotional_score;
    merged.technical_score = llm_weight * llm_features.technical_score + te_weight * te_features.technical_score;
    merged.greeting_score = llm_weight * llm_features.greeting_score + te_weight * te_features.greeting_score;
    merged.urgency_score = llm_weight * llm_features.urgency_score + te_weight * te_features.urgency_score;
    merged.action_score = llm_weight * llm_features.action_score + te_weight * te_features.action_score;
    merged.polarity_score = llm_weight * llm_features.polarity_score + te_weight * te_features.polarity_score;
    merged.phatic_score = llm_weight * llm_features.phatic_score + te_weight * te_features.phatic_score;
    
    merged.cognitive_intent = llm_features.success ? llm_features.cognitive_intent : te_features.cognitive_intent;
    merged.cognitive_confidence = llm_weight * llm_features.cognitive_confidence + te_weight * te_features.cognitive_confidence;
    
    merged.entities = llm_features.entities;
    merged.coreferences = llm_features.coreferences;
    
    merged.overall_confidence = llm_weight * llm_features.overall_confidence + te_weight * te_features.overall_confidence;
    merged.success = merged.overall_confidence >= config_.confidence_threshold;
}

std::vector<float> WernickeComprehender::embedText(const std::string& text) const {
    if (!local_transformer_ || !local_transformer_->isLoaded()) {
        return {};
    }
    return local_transformer_->embed(text);
}

WernickeOutput WernickeComprehender::comprehend(const WernickeInput& input) const {
    WernickeOutput output;
    auto start = std::chrono::high_resolution_clock::now();
    
    // Try LLM-based parsing first
    WernickeOutput llm_output;
    if (local_transformer_ && local_transformer_->isLoaded()) {
        std::string prompt = buildParsePrompt(input);
        
        auto sys_tok = local_transformer_->tokenize(
            "You are a precise linguistic analyzer. Output only valid JSON.", true);
        auto user_tok = local_transformer_->tokenize(prompt, false);
        
        if (sys_tok.success && user_tok.success) {
            GenerationConfig cfg;
            cfg.temperature = config_.temperature;
            cfg.max_tokens = config_.max_tokens;
            cfg.top_p = 0.9f;
            
            auto result = local_transformer_->generateWithContext(sys_tok.tokens, user_tok.tokens, cfg);
            
            if (result.success && !result.text.empty()) {
                parseLLMOutput(result.text, llm_output);
            }
        }
    }
    
    // Fallback to TextEncoder
    WernickeOutput te_output;
    fallbackToTextEncoder(input.user_text, te_output);
    
    // Merge
    mergeFeatures(llm_output, te_output, output);
    
    auto end = std::chrono::high_resolution_clock::now();
    output.latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
    
    return output;
}

WernickeOutput WernickeComprehender::classifyOnly(const std::string& text) const {
    WernickeInput input;
    input.user_text = text;
    input.q_intent.fill(0.125f);  // Uniform prior
    input.q_engagement[1] = 1.0f;
    input.q_urgency[0] = 1.0f;
    return comprehend(input);
}

} // namespace language
} // namespace brain
} // namespace yuki