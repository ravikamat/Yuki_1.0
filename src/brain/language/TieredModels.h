#pragma once
// TieredModels — Tiered LLM inference: small router → large generator
// Routes 90% of turns to 1.7B model, only calls 7B for complex generation

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <optional>
#include "brain/language/GenerationBackend.h"
#include "brain/language/GenerationRouter.h"

namespace yuki {
namespace language {

using yuki::brain::language::IGenerationBackend;
using yuki::brain::language::GenerationRouter;

struct TieredModelConfig {
    // Router model (small, fast)
    std::string router_model = "qwen3:1.7b";
    std::string router_host = "127.0.0.1";
    int router_port = 11434;
    int router_max_tokens = 64;
    float router_temperature = 0.1f;

    // Generator model (large, high quality)
    std::string generator_model = "qwen3:7b";
    std::string generator_host = "127.0.0.1";
    int generator_port = 11434;
    int generator_max_tokens = 512;
    float generator_temperature = 0.7f;

    // Routing thresholds
    float router_confidence_threshold = 0.8f;   // Above: use router output directly
    float complexity_threshold = 0.6f;          // Above: escalate to generator
    float uncertainty_threshold = 0.7f;         // Above: escalate to generator

    // Token budgets
    int max_router_tokens_per_turn = 64;
    int max_generator_tokens_per_turn = 512;
    int daily_generator_token_budget = 100000;

    // Fallback
    bool allow_generator_fallback = true;
    int max_retries = 2;
};

struct RoutingDecision {
    enum class Action : uint8_t {
        USE_ROUTER = 0,      // Router output sufficient
        USE_GENERATOR = 1,   // Escalate to generator
        USE_HYBRID = 2,      // Router for structure, generator for content
        CLARIFY = 3          // Ask for clarification
    };

    Action action = Action::USE_ROUTER;
    float router_confidence = 0.0f;
    float complexity_score = 0.0f;
    float uncertainty = 0.0f;
    std::string reasoning;
    std::chrono::microseconds router_latency;
    std::chrono::microseconds generator_latency;
};

struct TieredResult {
    std::string text;
    RoutingDecision decision;
    bool from_generator = false;
    size_t tokens_used = 0;
    float quality_score = 0.0f;
};

class TieredModels {
public:
    explicit TieredModels(const TieredModelConfig& cfg = TieredModelConfig{});
    ~TieredModels() = default;

    void setConfig(const TieredModelConfig& cfg) { config_ = cfg; }
    const TieredModelConfig& getConfig() const { return config_; }

    // Initialize models
    bool initialize();

    // Main inference: route and generate
    TieredResult generate(const std::string& prompt, 
                          const std::string& system_prompt = "",
                          const std::vector<std::pair<std::string, std::string>>& history = {});

    // Streaming generation
    struct StreamState {
        std::string accumulated_text;
        RoutingDecision decision;
        bool is_generator = false;
        bool done = false;
    };
    StreamState beginStream(const std::string& prompt, const std::string& system_prompt = "");
    bool stepStream(StreamState& state);

    // Analyze prompt complexity without generating
    RoutingDecision analyzeComplexity(const std::string& prompt) const;

    // Token budget management
    bool checkGeneratorBudget(size_t estimated_tokens) const;
    void consumeGeneratorTokens(size_t tokens);
    size_t getRemainingGeneratorBudget() const;
    void resetDailyBudget();

    // Statistics
    struct Stats {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> router_only{0};
        std::atomic<uint64_t> generator_calls{0};
        std::atomic<uint64_t> hybrid_calls{0};
        std::atomic<uint64_t> clarifications{0};
        std::atomic<uint64_t> generator_tokens_used{0};
        std::atomic<uint64_t> router_tokens_used{0};
        std::chrono::microseconds avg_router_latency{0};
        std::chrono::microseconds avg_generator_latency{0};
    };
    const Stats& getStats() const { return stats_; }

    // Backend access for GenerationRouter integration
    std::shared_ptr<IGenerationBackend> getRouterBackend() { return router_backend_; }
    std::shared_ptr<IGenerationBackend> getGeneratorBackend() { return generator_backend_; }

private:
    TieredModelConfig config_;
    std::shared_ptr<IGenerationBackend> router_backend_;
    std::shared_ptr<IGenerationBackend> generator_backend_;
    Stats stats_;
    std::atomic<size_t> daily_generator_tokens_{0};

    // Complexity analysis
    float computeComplexityScore(const std::string& prompt) const;
    float computeUncertainty(const std::string& prompt) const;
    float computeRouterConfidence(const std::string& prompt, const std::string& router_output) const;

    // Generation paths
    TieredResult generateRouterOnly(const std::string& prompt, const std::string& system_prompt);
    TieredResult generateWithGenerator(const std::string& prompt, const std::string& system_prompt);
    TieredResult generateHybrid(const std::string& prompt, const std::string& system_prompt);

    // HTTP requests
    std::string httpRequest(const std::string& host, int port, 
                            const std::string& model, const std::string& prompt,
                            float temperature, int max_tokens) const;
};

} // namespace language
} // namespace yuki