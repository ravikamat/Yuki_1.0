#pragma once
#include <array>
#include <string>
#include <vector>

namespace yuki {
namespace memory {

// Encodes 8 heuristic text scores + source tag into a dense vector
// for HNSWLib indexing. Output dimension = 24 to align with VSE BeliefState.
struct MemoryEncoder {
    static constexpr size_t OUTPUT_DIM = 24;

    // Convert 8 scores into 24-dim vector using fixed projection matrix
    // + one-hot source encoding + timestamp drift features.
    std::vector<float> encodeScores(
        float question, float command, float emotional,
        float technical, float urgency, float greeting,
        float action, float polarity,
        const std::string& source_tag,
        uint64_t timestamp_ms
    ) const;

    // Encode raw text for KnowledgeDaemon facts (TF-like heuristic)
    std::vector<float> encodeText(const std::string& text) const;

private:
    static std::array<float, OUTPUT_DIM> projectHeuristic(
        float q, float c, float e, float t, float u, float g, float a, float p
    );
};

} // namespace memory
} // namespace yuki
