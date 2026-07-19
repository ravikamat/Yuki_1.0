#include "HypervectorEncoder.h"
#include <cctype>
#include <algorithm>

namespace yuki::memory {

Hypervector HypervectorEncoder::encodeText(const std::string& text) const {
    if (text.empty()) return Hypervector::zero();

    // Lowercase
    std::string lower;
    lower.reserve(text.size());
    for (unsigned char c : text) lower.push_back(static_cast<char>(std::tolower(c)));

    // Extract trigrams
    std::vector<std::string> trigrams;
    for (size_t i = 0; i + 2 < lower.size(); ++i)
        trigrams.push_back(lower.substr(i, 3));
    if (trigrams.empty()) trigrams.push_back(lower);

    // Multi-way majority vote via uint16 counter array — avoids pairwise-bundle zero-collapse
    std::vector<uint16_t> votes(Hypervector::DIM, 0);
    for (size_t i = 0; i < trigrams.size(); ++i) {
        Hypervector tri = encodeTrigram(trigrams[i]);
        Hypervector pos = tri.permute(i % 100); // position encoding
        for (size_t b = 0; b < Hypervector::DIM; ++b)
            if (pos.get(b)) ++votes[b];
    }

    // Threshold at majority (>N/2 votes → 1)
    uint16_t threshold = static_cast<uint16_t>(trigrams.size() / 2);
    Hypervector result;
    for (size_t b = 0; b < Hypervector::DIM; ++b)
        result.set(b, votes[b] > threshold);
    return result;
}

// Position-INSENSITIVE: shared trigrams land on identical bits across texts
Hypervector HypervectorEncoder::encodeTextQuery(const std::string& text) const {
    if (text.empty()) return Hypervector::zero();

    std::string lower;
    lower.reserve(text.size());
    for (unsigned char c : text) lower.push_back(static_cast<char>(std::tolower(c)));

    std::vector<std::string> trigrams;
    for (size_t i = 0; i + 2 < lower.size(); ++i)
        trigrams.push_back(lower.substr(i, 3));
    if (trigrams.empty()) trigrams.push_back(lower);

    // Bipolar accumulator (int32 avoids unsigned underflow)
    std::vector<int32_t> votes(Hypervector::DIM, 0);
    for (const auto& tri : trigrams) {
        Hypervector hv = encodeTrigram(tri);
        for (size_t b = 0; b < Hypervector::DIM; ++b)
            votes[b] += hv.get(b) ? 1 : -1;
    }

    Hypervector result;
    for (size_t b = 0; b < Hypervector::DIM; ++b)
        result.set(b, votes[b] > 0);
    return result;
}

Hypervector HypervectorEncoder::encodeScores(
        float q, float c, float e, float t,
        float u, float g, float a, float p) const {
    // Each of 8 scores occupies 1250 contiguous bits.
    // Score in [-1,1]: map to density 0%..100% of that channel.
    Hypervector result;
    float scores[8] = {q, c, e, t, u, g, a, p};
    for (size_t ch = 0; ch < 8; ++ch) {
        float norm = (std::clamp(scores[ch], -1.0f, 1.0f) + 1.0f) / 2.0f; // [0,1]
        int   num_ones = static_cast<int>(1250.0f * norm);
        num_ones = std::clamp(num_ones, 0, 1250);
        size_t offset = ch * 1250;
        for (int i = 0; i < num_ones; ++i)
            result.set(offset + static_cast<size_t>(i), true);
    }
    return result;
}

Hypervector HypervectorEncoder::encodeSource(const std::string& source) const {
    std::lock_guard<std::mutex> lk(item_mtx_);
    auto it = item_memory_.find(source);
    if (it != item_memory_.end()) return it->second;
    Hypervector hv(std::hash<std::string>{}(source));
    item_memory_[source] = hv;
    return hv;
}

Hypervector HypervectorEncoder::encodeEpisode(
        const std::string& text, const std::string& source,
        float q, float c, float e, float t,
        float u, float g, float a, float p) const {
    return encodeText(text)
           .bind(encodeSource(source))
           .bind(encodeScores(q, c, e, t, u, g, a, p));
}

Hypervector HypervectorEncoder::encodeTrigram(const std::string& tri) const {
    std::lock_guard<std::mutex> lk(item_mtx_);
    auto it = item_memory_.find(tri);
    if (it != item_memory_.end()) return it->second;
    Hypervector hv(std::hash<std::string>{}(tri));
    item_memory_[tri] = hv;
    return hv;
}

} // namespace yuki::memory
