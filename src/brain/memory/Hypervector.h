// Hypervector.h — Yuki_1.0 Phase D: 64-bit word-packed storage (API preserved)
// Storage changed from std::bitset<10000> to std::array<uint64_t, 157>.
// All public methods have identical signatures. Internal implementation is faster:
//   bind = 157 word XORs,  hammingDistance = 157 popcount64 calls,
//   bundle(2) = 157 word ANDs.
#pragma once
#include <array>
#include <random>
#include <string>
#include <vector>
#include <cstdint>

namespace yuki::memory {

class Hypervector {
public:
    static constexpr size_t DIM    = 10000;
    static constexpr size_t kWords = (DIM + 63) / 64;  // 157 words
    // Mask for the final word: only (DIM % 64) = 16 bits are valid.
    static constexpr uint64_t kLastWordMask = (DIM % 64 == 0)
        ? ~uint64_t(0)
        : (uint64_t(1) << (DIM % 64)) - 1;

    Hypervector();
    explicit Hypervector(uint64_t seed);           // deterministic from seed
    explicit Hypervector(const std::string& text); // semantic seeding via trigrams

    // HDC algebra
    Hypervector bind  (const Hypervector& other) const;  // XOR binding (157 word XORs)
    Hypervector bundle(const Hypervector& other) const;  // majority-vote / tie→0 (AND for 2)
    Hypervector permute(size_t shifts)           const;  // cyclic left-shift

    // Similarity
    float  cosineSimilarity(const Hypervector& other) const; // 1 − 2·Hamming/DIM
    size_t hammingDistance (const Hypervector& other) const; // Σ popcount64(w_a XOR w_b)

    // Serialisation (identical format to bitset version)
    std::string toHex()                               const;
    static Hypervector fromHex(const std::string& hex);

    // Generators
    static Hypervector random(std::mt19937& gen);
    static Hypervector zero();
    static Hypervector one();

    // Bit accessors (API compatible with old get/set)
    bool get(size_t idx)          const { return ((words_[idx >> 6] >> (idx & 63)) & 1ULL) != 0; }
    void set(size_t idx, bool val) {
        if (val) words_[idx >> 6] |=  (uint64_t(1) << (idx & 63));
        else     words_[idx >> 6] &= ~(uint64_t(1) << (idx & 63));
    }

    // Raw word access (for SIMD-friendly SDM counter ops)
    const std::array<uint64_t, kWords>& words() const { return words_; }

private:
    std::array<uint64_t, kWords> words_{};

    // Clear extra bits beyond DIM in the last word (called after every mutating op).
    void fixLastWord_() { words_[kWords - 1] &= kLastWordMask; }

    static size_t popcount64_(uint64_t w);
};

} // namespace yuki::memory
