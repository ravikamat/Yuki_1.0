#pragma once
#include "Hypervector.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace yuki::memory {

// Encodes text / heuristic scores / full episodes into 10,000-bit hypervectors.
// Kept separate from MemoryEncoder (float-vec) to preserve backward compatibility.
class HypervectorEncoder {
public:
    // Encode text using trigram binding with cyclic permutation (position-sensitive)
    Hypervector encodeText(const std::string& text) const;

    // Position-INSENSITIVE encoding for cross-text similarity queries.
    // Bundles trigrams WITHOUT permutation so shared trigrams align to the same bits.
    Hypervector encodeTextQuery(const std::string& text) const;

    // Encode 8 heuristic scores into hypervector (1250 bits per score channel)
    Hypervector encodeScores(float question, float command, float emotional,
                             float technical, float urgency, float greeting,
                             float action, float polarity) const;

    // Encode source tag as item memory (random but deterministic)
    Hypervector encodeSource(const std::string& source) const;

    // Full episode: text XOR source XOR scores
    Hypervector encodeEpisode(const std::string& text, const std::string& source,
                              float q, float c, float e, float t,
                              float u, float g, float a, float p) const;

private:
    Hypervector encodeTrigram(const std::string& tri) const;

    mutable std::unordered_map<std::string, Hypervector> item_memory_;
    mutable std::mutex                                    item_mtx_;
};

} // namespace yuki::memory
