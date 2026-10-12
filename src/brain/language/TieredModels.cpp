#include "brain/language/TieredModels.h"
#include "brain/core/Logger.h"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace yuki {
namespace language {

TieredModels::TieredModels(const TieredModelConfig& cfg) : config_(cfg) {}

bool TieredModels::initialize() {
    // In production, would initialize actual backends (llama.cpp, etc.)
    // For now, create placeholder backends
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TieredModels",
        "Initializing tiered models: router=" + config_.router_model + 
        ", generator=" + config_.generator_model);
    return true;
}

TieredResult TieredModels::generate(const std::string& prompt,
                                                  const std::string& system_prompt,
                                                  const std::vector<std::pair<std::string, std::string>>& history) {
    auto start = std::chrono::steady_clock::now();
    stats_.total_requests++;

    // Analyze complexity
    RoutingDecision decision = analyzeComplexity(prompt);

    TieredResult result;

    switch (decision.action) {
        case RoutingDecision::Action::USE_ROUTER: {
            result = generateRouterOnly(prompt, system_prompt);
            result.decision = decision;
            stats_.router_only++;
            break;
        }
        case RoutingDecision::Action::USE_GENERATOR: {
            if (checkGeneratorBudget(config_.generator_max_tokens)) {
                result = generateWithGenerator(prompt, system_prompt);
                result.decision = decision;
                result.from_generator = true;
                stats_.generator_calls++;
            } else {
                // Fallback to router
                result = generateRouterOnly(prompt, system_prompt);
                result.decision.action = RoutingDecision::Action::USE_ROUTER;
                result.decision.reasoning += " [Generator budget exceeded, fallback to router]";
            }
            break;
        }
        case RoutingDecision::Action::USE_HYBRID: {
            result = generateHybrid(prompt, system_prompt);
            result.decision = decision;
            stats_.hybrid_calls++;
            break;
        }
        case RoutingDecision::Action::CLARIFY: {
            result.text = "Could you clarify what you mean?";
            result.decision = decision;
            stats_.clarifications++;
            break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (result.from_generator) {
        uint64_t gen_calls = stats_.generator_calls.load();
        stats_.avg_generator_latency = 
            (stats_.avg_generator_latency * (gen_calls - 1) + latency) 
            / gen_calls;
    } else {
        uint64_t router_calls = stats_.router_only.load() + stats_.hybrid_calls.load();
        stats_.avg_router_latency = 
            (stats_.avg_router_latency * (router_calls - 1) + latency) 
            / router_calls;
    }

    return result;
}

RoutingDecision TieredModels::analyzeComplexity(const std::string& prompt) const {
    RoutingDecision decision;
    
    // Compute complexity features
    float complexity = computeComplexityScore(prompt);
    float uncertainty = computeUncertainty(prompt);
    
    // Quick router confidence estimate (would run router in production)
    float router_conf = 0.9f;  // Placeholder
    
    decision.complexity_score = complexity;
    decision.uncertainty = uncertainty;
    decision.router_confidence = router_conf;

    // Decision logic
    if (complexity < config_.complexity_threshold && 
        uncertainty < config_.uncertainty_threshold &&
        router_conf > config_.router_confidence_threshold) {
        decision.action = RoutingDecision::Action::USE_ROUTER;
        decision.reasoning = "Low complexity, high router confidence";
    } else if (complexity > config_.complexity_threshold || 
               uncertainty > config_.uncertainty_threshold) {
        decision.action = RoutingDecision::Action::USE_GENERATOR;
        decision.reasoning = "High complexity or uncertainty";
    } else {
        decision.action = RoutingDecision::Action::USE_HYBRID;
        decision.reasoning = "Moderate complexity, hybrid approach";
    }

    return decision;
}

TieredResult TieredModels::generateRouterOnly(const std::string& prompt, 
                                                            const std::string& system_prompt) {
    TieredResult result;
    result.from_generator = false;

    // Build prompt with system
    std::string full_prompt = system_prompt.empty() ? prompt : system_prompt + "\n\n" + prompt;
    
    auto start = std::chrono::steady_clock::now();
    // In production: call router backend
    // result.text = router_backend_->generate(full_prompt, config_.router_temperature, config_.router_max_tokens);
    result.text = "[Router] " + prompt;  // Placeholder
    auto end = std::chrono::steady_clock::now();
    
    result.decision.router_latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.tokens_used = config_.router_max_tokens;
    stats_.router_tokens_used += config_.router_max_tokens;
    result.quality_score = 0.7f;
    
    return result;
}

TieredResult TieredModels::generateWithGenerator(const std::string& prompt,
                                                                const std::string& system_prompt) {
    TieredResult result;
    result.from_generator = true;

    std::string full_prompt = system_prompt.empty() ? prompt : system_prompt + "\n\n" + prompt;
    
    auto start = std::chrono::steady_clock::now();
    // In production: call generator backend
    // result.text = generator_backend_->generate(full_prompt, config_.generator_temperature, config_.generator_max_tokens);
    result.text = "[Generator] " + prompt;  // Placeholder
    auto end = std::chrono::steady_clock::now();
    
    result.decision.generator_latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.tokens_used = config_.generator_max_tokens;
    consumeGeneratorTokens(config_.generator_max_tokens);
    stats_.generator_tokens_used += config_.generator_max_tokens;
    result.quality_score = 0.95f;
    
    return result;
}

TieredResult TieredModels::generateHybrid(const std::string& prompt,
                                                         const std::string& system_prompt) {
    TieredResult result;
    
    // Router generates structure/plan
    TieredResult router_result = generateRouterOnly(
        "Generate a structured outline for: " + prompt, system_prompt);
    
    // Generator fills in content
    TieredResult gen_result = generateWithGenerator(
        "Expand this outline into a full response: " + router_result.text, system_prompt);
    
    result.text = gen_result.text;
    result.decision = router_result.decision;
    result.decision.action = RoutingDecision::Action::USE_HYBRID;
    result.from_generator = true;
    result.tokens_used = router_result.tokens_used + gen_result.tokens_used;
    consumeGeneratorTokens(gen_result.tokens_used);
    result.quality_score = 0.9f;
    
    return result;
}

TieredModels::StreamState TieredModels::beginStream(const std::string& prompt, 
                                                     const std::string& system_prompt) {
    StreamState state;
    RoutingDecision decision = analyzeComplexity(prompt);
    state.decision = decision;
    state.is_generator = (decision.action == RoutingDecision::Action::USE_GENERATOR ||
                          decision.action == RoutingDecision::Action::USE_HYBRID);
    
    // Start appropriate stream
    // In production: initialize backend stream
    state.accumulated_text = "";
    state.done = false;
    
    return state;
}

bool TieredModels::stepStream(StreamState& state) {
    // In production: get next token from backend
    // For now, simulate completion
    if (state.accumulated_text.empty()) {
        state.accumulated_text = state.is_generator ? "[Generator stream] " : "[Router stream] ";
        state.done = true;
    }
    return state.done;
}

float TieredModels::computeComplexityScore(const std::string& prompt) const {
    float score = 0.0f;
    
    // Length factor
    score += std::min(1.0f, prompt.size() / 500.0f) * 0.3f;
    
    // Question complexity
    size_t question_marks = std::count(prompt.begin(), prompt.end(), '?');
    score += std::min(1.0f, question_marks / 3.0f) * 0.2f;
    
    // Technical terms (heuristic)
    const std::vector<std::string> tech_terms = {
        "algorithm", "architecture", "implementation", "optimization",
        "quantum", "neural", "distributed", "concurrent", "recursive"
    };
    int tech_count = 0;
    for (const auto& term : tech_terms) {
        if (prompt.find(term) != std::string::npos) tech_count++;
    }
    score += std::min(1.0f, tech_count / 5.0f) * 0.3f;
    
    // Reasoning keywords
    const std::vector<std::string> reasoning_terms = {
        "why", "how", "explain", "analyze", "compare", "evaluate",
        "prove", "derive", "synthesize"
    };
    int reasoning_count = 0;
    for (const auto& term : reasoning_terms) {
        if (prompt.find(term) != std::string::npos) reasoning_count++;
    }
    score += std::min(1.0f, reasoning_count / 3.0f) * 0.2f;
    
    return std::clamp(score, 0.0f, 1.0f);
}

float TieredModels::computeUncertainty(const std::string& prompt) const {
    float uncertainty = 0.0f;
    
    // Ambiguity markers
    const std::vector<std::string> ambiguous = {
        "maybe", "perhaps", "might", "could", "possibly", "unsure",
        "unclear", "vague", "ambiguous"
    };
    int amb_count = 0;
    for (const auto& term : ambiguous) {
        if (prompt.find(term) != std::string::npos) amb_count++;
    }
    uncertainty += std::min(1.0f, amb_count / 3.0f) * 0.5f;
    
    // Pronoun density (high = more context-dependent)
    int pronouns = 0;
    const std::vector<std::string> prons = {"it", "this", "that", "these", "those", "he", "she", "they"};
    for (const auto& p : prons) {
        size_t pos = 0;
        while ((pos = prompt.find(p, pos)) != std::string::npos) {
            pronouns++;
            pos += p.size();
        }
    }
    uncertainty += std::min(1.0f, pronouns / 10.0f) * 0.5f;
    
    return std::clamp(uncertainty, 0.0f, 1.0f);
}

float TieredModels::computeRouterConfidence(const std::string& prompt, const std::string& router_output) const {
    // Simplified: in production would analyze router output quality
    if (router_output.empty()) return 0.5f;
    
    float conf = 0.7f;  // Base confidence
    
    // Length check
    if (router_output.size() > 10) conf += 0.1f;
    if (router_output.size() > 50) conf += 0.1f;
    
    // Structure check
    if (router_output.find('.') != std::string::npos) conf += 0.05f;
    if (router_output.find(',') != std::string::npos) conf += 0.05f;
    
    return std::clamp(conf, 0.0f, 1.0f);
}

bool TieredModels::checkGeneratorBudget(size_t estimated_tokens) const {
    size_t remaining = (daily_generator_tokens_ < config_.daily_generator_token_budget) 
        ? config_.daily_generator_token_budget - daily_generator_tokens_ 
        : 0;
    return remaining >= estimated_tokens;
}

void TieredModels::consumeGeneratorTokens(size_t tokens) {
    daily_generator_tokens_ += tokens;
}

size_t TieredModels::getRemainingGeneratorBudget() const {
    if (daily_generator_tokens_ >= config_.daily_generator_token_budget) return 0;
    return config_.daily_generator_token_budget - daily_generator_tokens_;
}

void TieredModels::resetDailyBudget() {
    daily_generator_tokens_ = 0;
}

std::string TieredModels::httpRequest(const std::string& host, int port,
                                       const std::string& model, const std::string& prompt,
                                       float temperature, int max_tokens) const {
    // Placeholder for HTTP request to Ollama/llama.cpp server
    return "{}";
}

} // namespace language
} // namespace yuki