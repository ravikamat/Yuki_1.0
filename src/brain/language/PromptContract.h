// ============================================================================
// YUKI v1.0 - PromptContract Specification
// Structured contract for LLM generation prompts loaded via ConfigManager.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace yuki {
namespace language {

struct PromptContract {
    std::string systemRules;
    std::string taskSpec;
    std::string evidenceBlock;
    std::string actionPolicy;
    std::string outputSchema;
    std::string styleSpec;

    // Build complete monolithic prompt from structured contracts
    std::string buildFullPrompt() const {
        std::string full;
        full.reserve(2048);

        if (!systemRules.empty()) {
            full += "=== SYSTEM RULES ===\n" + systemRules + "\n\n";
        }
        if (!taskSpec.empty()) {
            full += "=== TASK SPECIFICATION ===\n" + taskSpec + "\n\n";
        }
        if (!evidenceBlock.empty()) {
            full += "=== EVIDENCE & CONTEXT ===\n" + evidenceBlock + "\n\n";
        }
        if (!actionPolicy.empty()) {
            full += "=== ACTION POLICY ===\n" + actionPolicy + "\n\n";
        }
        if (!outputSchema.empty()) {
            full += "=== OUTPUT SCHEMA & FORMAT ===\n" + outputSchema + "\n\n";
        }
        if (!styleSpec.empty()) {
            full += "=== STYLE & TONE ===\n" + styleSpec + "\n";
        }
        return full;
    }
};

} // namespace language
} // namespace yuki
