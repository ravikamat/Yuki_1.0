#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "GenerationBackend.h"
#include "LocalTransformer.h"

namespace yuki {

// LocalLLM runtime configuration (namespace scope so the constructor's default
// argument `Config{}` parses cleanly with C++20 nested member initializers..
struct LocalLLMConfig {
    std::string host = "127.0.0.1";
    int port = 11434;
    std::string model = "qwen3:1.7b";
    float default_temperature = 0.7f;
    int default_max_tokens = 512;
    int timeout_ms = 60000;
};

class LocalLLM {
public:
    using Config = LocalLLMConfig;

    struct LegacyGenerationResult {
        std::string text;
        bool success = false;
        float latency_ms = 0.0f;
        int tokens_generated = 0;
        std::string error;
    };

    struct ChatMessage {
        std::string role;
        std::string content;
    };

    explicit LocalLLM(const Config& config = Config{});
    ~LocalLLM();
    LocalLLM(const LocalLLM&) = delete;
    LocalLLM& operator=(const LocalLLM&) = delete;

    bool isAvailable() const;

    LegacyGenerationResult generate(const std::string& prompt,
                                   float temperature = -1.0f,
                                   int max_tokens = -1) const;

    LegacyGenerationResult chat(const std::vector<ChatMessage>& messages,
                               float temperature = -1.0f,
                               int max_tokens = -1) const;

    std::string buildPrompt(const std::string& user_input,
                            const std::string& retrieved_context = "",
                            const std::string& intent_label = "",
                            const std::string& memory_context = "") const;

    yuki::brain::language::GenerationResult routeGenerate(
        yuki::brain::language::BackendKind kind,
        const yuki::brain::language::GenerationRequest& request) const;

    std::string extractJsonField(const std::string& json, const std::string& key) const;
    std::string escapeJson(const std::string& s) const;

    const Config& config() const { return config_; }
    void set_timeout_ms(int ms) { config_.timeout_ms = ms; }
    
    // Access to LocalTransformer for direct use
    std::shared_ptr<yuki::brain::language::LocalTransformer> getLocalTransformer() { return local_transformer_; }
    const std::shared_ptr<yuki::brain::language::LocalTransformer> getLocalTransformer() const { return local_transformer_; }

    // Phase 0.1: GGUF model is for TRAINING/TESTING ONLY
    // Not called during conversation — use training pipeline methods instead
    bool isTrainingOnly() const { return true; }
    
    // Training-specific generation (for distillation corpus, self-play)
    LegacyGenerationResult generateForTraining(const std::string& prompt,
                                                float temperature = -1.0f,
                                                int max_tokens = -1) const;

private:
    Config config_;
    mutable bool last_ping_result_{false};
    mutable std::chrono::steady_clock::time_point last_ping_time_;
    mutable std::string last_ping_error_;
    
    // LocalTransformer as primary backend
    std::shared_ptr<yuki::brain::language::LocalTransformer> local_transformer_;

    std::string httpRequest(const std::string& method,
                            const std::string& path,
                            const std::string& body) const;
    static bool ensureWinsock();
};

} // namespace yuki
