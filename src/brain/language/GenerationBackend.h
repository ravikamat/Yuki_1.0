#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace yuki::brain::language {

enum class BackendKind {
    LOCAL_TRANSFORMER,
    LOCAL_TRANSFORMER_SYCL,
    LOCAL_TRANSFORMER_CPU,
    EXTERNAL_LLM,
    VAE_GRAMMAR,
    GROUNDED
};

enum class GenerationTaskType {
    CHAT,
    CODE_COMPLETION,
    SUMMARIZATION,
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

// C++20 stop_token alternative (avoids std::stop_token thread join requirement) for std::stop_token
struct StopToken {
    bool stopped = false;
    
    bool stop_requested() const { return stopped; }
    void request_stop() { stopped = true; }
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
    bool allowExternalFallback{false};  // Phase 0.1: LLM-free by default; set true only for distillation
    bool requestTokenScores{false};
    StopToken stop_token;
    std::map<std::string, std::string> metadata;
};

enum class GenerationFailureReason : uint8_t {
    None = 0,
    UnsupportedRequest,
    NotConfigured,
    NotAvailable,
    AttestationRejected,
    LeaseRejected,
    HealthCheckFailed,
    ReadinessFailed,
    TransportFailure,
    HttpFailure,
    InvalidResponse,
    Timeout,
    Cancelled,
    ResourceRejected,
    InternalFailure
};

struct GenerationResult {
    bool success{false};
    BackendKind backend{BackendKind::EXTERNAL_LLM};
    BackendKind backendKind{BackendKind::EXTERNAL_LLM};
    std::string backendName;
    std::string text;
    std::vector<int> tokens;
    std::vector<float> tokenScores;
    float confidence{0.0f};
    float fluencyScore{0.0f};
    float relevanceScore{0.0f};
    float safetyScore{0.0f};
    float estimatedCost{0.0f};
    float elapsedMs{0.0f};
    float latencyMs{0.0f};
    int outputTokenCount{0};
    bool usedFallback{false};
    bool fallbackEligible{false};
    GenerationFailureReason generationFailureReason{GenerationFailureReason::None};
    std::string failureReason;
    std::string diagnostic;
    std::string error;

    // Phase 3: Grounded generation extensions
    bool clarificationRequested{false};
    std::string clarificationReason;
    std::vector<int> clarificationTokens;
    float surprise{0.0f};
    float entropy{0.0f};

    // Acceleration extensions (Section 9.1)
    bool accelerated{false};
    std::string deviceName;
    float promptTokensPerSecond{0.0f};
    float decodeTokensPerSecond{0.0f};
};


class IGenerationBackend {
public:
    virtual ~IGenerationBackend() = default;
    virtual GenerationResult generate(const GenerationRequest& request) = 0;
    virtual bool available() const = 0;
    virtual BackendKind kind() const = 0;
    virtual std::string name() const = 0;
    virtual float estimateCost(const GenerationRequest& request) const = 0;
    virtual bool initialize() { return true; }
    virtual bool isAvailable() const { return available(); }
    virtual std::string getBackendName() const { return name(); }
    virtual BackendKind getKind() const { return kind(); }
};

inline GenerationResult renderSafeGenerationFailure(const GenerationResult& input) {
    GenerationResult res = input;
    res.success = false;
    if (res.text.empty()) {
        res.text = "[Generation unavailable: " + (res.failureReason.empty() ? "unspecified error" : res.failureReason) + "]";
    }
    return res;
}

} // namespace yuki::brain::language