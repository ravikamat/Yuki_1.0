#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace yuki::brain::language {

enum class BackendKind {
    LOCAL_TRANSFORMER = 0,
    EXTERNAL_LLM,
    VAE_GRAMMAR
};

enum class GenerationTaskType {
    CHAT = 0,
    RESEARCH_SUMMARY,
    CODE_SYNTHESIS,
    TOOL_REASONING,
    MEMORY_RESPONSE,
    SAFETY_RESPONSE,
    SELF_CRITIQUE,
    SELF_EVAL,
    DISTILLATION,
    SELF_PLAY
};

struct GenerationRequest {
    std::string prompt;
    std::string systemPrompt;
    std::string sessionId;
    std::string taskId;
    GenerationTaskType taskType{GenerationTaskType::CHAT};
    float temperature{0.2f};
    float topP{0.95f};
    int maxTokens{512};
    bool requireDeterminism{false};
    bool highImportance{false};
    bool allowExternalFallback{true};
    bool requestTokenScores{false};
    std::map<std::string, std::string> metadata;
};

struct GenerationResult {
    bool success{false};
    BackendKind backend{BackendKind::EXTERNAL_LLM};
    std::string backendName;
    std::string text;
    std::vector<float> tokenScores;
    float confidence{0.0f};
    float fluencyScore{0.0f};
    float relevanceScore{0.0f};
    float safetyScore{0.0f};
    float estimatedCost{0.0f};
    float elapsedMs{0.0f};
    bool usedFallback{false};
    std::string failureReason;
};

class IGenerationBackend {
public:
    virtual ~IGenerationBackend() = default;
    virtual GenerationResult generate(const GenerationRequest& request) = 0;
    virtual bool available() const = 0;
    virtual BackendKind kind() const = 0;
    virtual std::string name() const = 0;
    virtual float estimateCost(const GenerationRequest& request) const = 0;
};

} // namespace yuki::brain::language
