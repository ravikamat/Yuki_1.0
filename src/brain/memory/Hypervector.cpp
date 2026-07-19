// Hypervector.cpp — Phase D: 64-bit word implementation
#include "brain/memory/Hypervector.h"
#include <intrin.h>      // __popcnt64 (MSVC)
#include <sstream>
#include <iomanip>
#include <cstring>

namespace yuki::memory {

// ── popcount (MSVC intrinsic) ─────────────────────────────────────────────────
size_t Hypervector::popcount64_(uint64_t w) {
    return static_cast<size_t>(__popcnt64(w));
}

// ── Constructors ──────────────────────────────────────────────────────────────
Hypervector::Hypervector() { words_.fill(0); }

Hypervector::Hypervector(uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<uint64_t> dist;
    for (auto& w : words_) w = dist(gen);
    fixLastWord_();
}

Hypervector::Hypervector(const std::string& text) {
    words_.fill(0);
    std::hash<std::string> hasher;
    std::mt19937 gen(static_cast<uint32_t>(hasher(text)));
    std::uniform_int_distribution<size_t> dist(0, DIM - 1);
    for (int i = 0; i < 50; ++i) {
        size_t bit = dist(gen);
        words_[bit >> 6] |= (uint64_t(1) << (bit & 63));
    }
    for (unsigned char c : text) {
        size_t bit = (static_cast<size_t>(
                          hasher(std::string(1, static_cast<char>(c)))) * 31ULL) % DIM;
        words_[bit >> 6] |= (uint64_t(1) << (bit & 63));
    }
    // no need to fixLastWord_: only set bits in [0,DIM)
}

// ── HDC algebra ───────────────────────────────────────────────────────────────
Hypervector Hypervector::bind(const Hypervector& other) const {
    Hypervector result;
    for (size_t w = 0; w < kWords; ++w)
        result.words_[w] = words_[w] ^ other.words_[w];
    // No fixLastWord_ needed: XOR preserves zero extra bits.
    return result;
}

Hypervector Hypervector::bundle(const Hypervector& other) const {
    // Two-vector majority-vote: tie (0,0)→0, (1,1)→1, (0,1)→0 ≡ AND
    Hypervector result;
    for (size_t w = 0; w < kWords; ++w)
        result.words_[w] = words_[w] & other.words_[w];
    return result;
}

Hypervector Hypervector::permute(size_t shifts) const {
    if (shifts == 0) return *this;
    const size_t S = shifts % DIM;
    
    Hypervector result;
    for (size_t i = 0; i < DIM; ++i) {
        if (this->get(i)) {
            result.set((i + S) % DIM, true);
        }
    }
    return result;
}

// ── Similarity ────────────────────────────────────────────────────────────────
size_t Hypervector::hammingDistance(const Hypervector& other) const {
    size_t dist = 0;
    for (size_t w = 0; w < kWords; ++w)
        dist += popcount64_(words_[w] ^ other.words_[w]);
    return dist;
}

float Hypervector::cosineSimilarity(const Hypervector& other) const {
    size_t match = DIM - hammingDistance(other);
    return (2.0f * static_cast<float>(match) / static_cast<float>(DIM)) - 1.0f;
}

// ── Serialisation ─────────────────────────────────────────────────────────────
// Produces 2500 hex chars: nibble 0 = bits 0-3, nibble 1 = bits 4-7, …
// Each nibble comes from 4 consecutive bits starting at bit i, all within one word.
std::string Hypervector::toHex() const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(DIM / 4);  // 2500 chars
    for (size_t i = 0; i < DIM; i += 4) {
        size_t  w      = i >> 6;        // i / 64
        size_t  b      = i & 63;        // i % 64 (always multiple of 4 → safe in one word)
        uint8_t nibble = static_cast<uint8_t>((words_[w] >> b) & 0xFULL);
        result += hex_chars[nibble];
    }
    return result;
}

Hypervector Hypervector::fromHex(const std::string& hex) {
    Hypervector result;
    result.words_.fill(0);
    size_t idx = 0;
    for (char c : hex) {
        if (idx >= DIM) break;
        int val = (c >= '0' && c <= '9') ? (c - '0')
                : (c >= 'a' && c <= 'f') ? (c - 'a' + 10)
                : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0;
        size_t w = idx >> 6;
        size_t b = idx & 63;
        result.words_[w] |= (static_cast<uint64_t>(val) << b);
        idx += 4;
    }
    return result;
}

// ── Generators ────────────────────────────────────────────────────────────────
Hypervector Hypervector::random(std::mt19937& gen) {
    Hypervector result;
    std::uniform_int_distribution<uint64_t> dist;
    for (auto& w : result.words_) w = dist(gen);
    result.fixLastWord_();
    return result;
}

Hypervector Hypervector::zero() {
    Hypervector result;
    result.words_.fill(0);
    return result;
}

Hypervector Hypervector::one() {
    Hypervector result;
    result.words_.fill(~uint64_t(0));
    result.fixLastWord_();
    return result;
}

} // namespace yuki::memory
