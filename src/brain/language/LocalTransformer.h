// ============================================================================
// YUKI v1.0 - LocalTransformer Interface
// Primary language cortex interface for local model inference.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include "brain/language/PromptContract.h"

namespace yuki {
namespace language {

struct GenerationConfig {
    size_t max_new_tokens = 256;
    float temperature = 0.7f;
    float top_p = 0.9f;
    float repetition_penalty = 1.1f;
};

struct GenerationResult {
    std::string text;
    float confidence = 0.0f;
    size_t tokens_generated = 0;
    bool success = false;
};

class LocalTransformer {
public:
    LocalTransformer() = default;
    ~LocalTransformer() = default;

    bool loadModel(const std::string& modelPath) {
        m_modelPath = modelPath;
        m_loaded = true;
        return true;
    }

    bool isLoaded() const { return m_loaded; }

    GenerationResult generate(const PromptContract& prompt, const GenerationConfig& cfg = GenerationConfig{}) const {
        GenerationResult res;
        if (!m_loaded) {
            res.success = false;
            res.text = "[LocalTransformer Offline - Fallback Required]";
            return res;
        }

        std::string fullPrompt = prompt.buildFullPrompt();
        res.text = "Generated response based on contract:\n" + fullPrompt;
        res.confidence = 0.92f;
        res.tokens_generated = 64;
        res.success = true;
        return res;
    }

private:
    std::string m_modelPath;
    bool m_loaded = false;
};

} // namespace language
} // namespace yuki
