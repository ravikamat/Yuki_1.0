// ============================================================================
// YUKI v1.0 - GeneratorSelector Implementation
// ============================================================================
#include "brain/language/GeneratorSelector.h"

namespace yuki {
namespace language {

// Explicit translation unit for GeneratorSelector methods if expanded in future
std::string toString(GeneratorType type) {
    switch (type) {
        case GeneratorType::TRANSFORMER_PRIMARY: return "TRANSFORMER_PRIMARY";
        case GeneratorType::REASONING_VERBALIZATION: return "REASONING_VERBALIZATION";
        case GeneratorType::VAE_PLUS_TRANSFORMER: return "VAE_PLUS_TRANSFORMER";
        case GeneratorType::PLANNER_AND_TOOL: return "PLANNER_AND_TOOL";
        case GeneratorType::CLARIFY_QUESTION: return "CLARIFY_QUESTION";
        case GeneratorType::SAFE_DEFERRAL: return "SAFE_DEFERRAL";
        case GeneratorType::PCFG_FALLBACK: return "PCFG_FALLBACK";
        default: return "UNKNOWN";
    }
}

} // namespace language
} // namespace yuki
