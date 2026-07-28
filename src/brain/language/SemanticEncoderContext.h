// ============================================================================
// YUKI v1.0 - Contextual Semantic Encoder Structures
// Defines subword entries, sense prototypes, and contextual embeddings.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace yuki {
namespace language {

struct SubwordEntry {
    uint64_t hash = 0;
    std::vector<float> embedding;
};

struct SensePrototype {
    std::string token;
    std::vector<float> centroid;
    uint32_t count = 0;
    std::vector<uint32_t> neighborIds;
};

struct ContextualEmbedding {
    std::vector<float> base;
    std::vector<float> context;
    std::vector<float> fused;
    float ambiguity = 0.0f;
};

} // namespace language
} // namespace yuki
