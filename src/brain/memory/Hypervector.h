// Hypervector.h — Yuki_1.0 Phase D: 64-bit word-packed storage (API preserved)
// Storage changed from std::bitset<10000> to std::array<uint64_t, 157>.
// All public methods have identical signatures. Internal implementation is faster:
//   bind = 157 word XORs,  hammingDistance = 157 popcount64 calls,
//   bundle(2) = 157 word ANDs.
//
// FHRR (Fourier Holographic Reduced Representations) support:
// Phase-based HDC with lossless binding via complex phasor multiplication.
// 2048-dimensional phase vectors (0 to 2π), lossless bind/unbind via phasor arithmetic.

#pragma once
#include <array>
#include <random>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <type_traits>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace yuki::memory {

// Binary HDC (existing)
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

    // HDC algebra (binary)
    Hypervector bind  (const Hypervector& other) const;  // XOR binding (157 word XORs)
    Hypervector bundle(const Hypervector& other) const;  // majority-vote / tie→0 (AND for 2)
    Hypervector permute(size_t shifts)           const;  // cyclic left-shift

    // Similarity
    float  cosineSimilarity(const Hypervector& other) const; // 1 − 2·Hamming/DIM
    size_t hammingDistance (const Hypervector& other) const; // Σ popcount64(w_a XOR w_b)

    // Serialisation
    std::string toHex()                               const;
    static Hypervector fromHex(const std::string& hex);

    // Generators
    static Hypervector random(std::mt19937& gen);
    static Hypervector zero();
    static Hypervector one();

    // Bit accessors
    bool get(size_t idx)          const { return ((words_[idx >> 6] >> (idx & 63)) & 1ULL) != 0; }
    void set(size_t idx, bool val) {
        if (val) words_[idx >> 6] |=  (uint64_t(1) << (idx & 63));
        else     words_[idx >> 6] &= ~(uint64_t(1) << (idx & 63));
    }

    // Raw word access
    const std::array<uint64_t, kWords>& words() const { return words_; }

private:
    std::array<uint64_t, kWords> words_{};

    void fixLastWord_() { words_[kWords - 1] &= kLastWordMask; }
    static size_t popcount64_(uint64_t w);
};

// FHRR (Fourier Holographic Reduced Representations) — Phase-based HDC
// 2048-dimensional phase vectors (0 to 2π), lossless bind/unbind via phasor arithmetic.
struct FHRRPhasor {
    constexpr FHRRPhasor() = default;
    constexpr FHRRPhasor(float r, float i) : real(r), imag(i) {}
    
    // Create from phase angle (unit magnitude)
    static FHRRPhasor fromPhase(float phase) {
        return FHRRPhasor(std::cos(phase), std::sin(phase));
    }
    
    // Get phase angle
    float phase() const {
        return std::atan2(imag, real);
    }
    
    // Complex multiplication (phase addition)
    FHRRPhasor operator*(const FHRRPhasor& other) const {
        return FHRRPhasor(
            real * other.real - imag * other.imag,
            real * other.imag + imag * other.real
        );
    }
    
    // Complex division (phase subtraction)
    FHRRPhasor operator/(const FHRRPhasor& other) const {
        float denom = other.real * other.real + other.imag * other.imag;
        return FHRRPhasor(
            (real * other.real + imag * other.imag) / denom,
            (imag * other.real - real * other.imag) / denom
        );
    }
    
    // Complex conjugate (phase negation)
    FHRRPhasor conjugate() const {
        return FHRRPhasor(real, -imag);
    }
    
    float real = 1.0f;
    float imag = 0.0f;
};

// FHRR (Fourier Holographic Reduced Representations) — Phase-based HDC
// 2048-dimensional phase vectors (0 to 2π), lossless bind/unbind via phasor arithmetic.
class FHRRVector {
public:
    static constexpr size_t DIM = 2048;  // Phase dimensions
    using Phase = float;                 // 0 to 2π
    using Phasor = FHRRPhasor;           // unit magnitude phasor

    FHRRVector();
    explicit FHRRVector(uint64_t seed);           // deterministic from seed
    explicit FHRRVector(const std::string& text); // semantic seeding via trigrams

    // FHRR algebra (lossless, invertible)
    FHRRVector bind  (const FHRRVector& other) const;  // phase addition (complex multiply)
    FHRRVector unbind(const FHRRVector& other) const;  // phase subtraction (complex divide)
    FHRRVector permute(size_t shifts)           const;  // cyclic phase shift

    // Similarity (cosine of phase difference)
    float  cosineSimilarity(const FHRRVector& other) const;
    float  phaseDifference(const FHRRVector& other) const;

    // Exact inverse for lossless unbinding
    FHRRVector inverse() const; // complex conjugate

    // Serialisation
    std::string toHex()                         const;
    static FHRRVector fromHex(const std::string& hex);

    // Generators
    static FHRRVector random(std::mt19937& gen);
    static FHRRVector zero();  // all phases = 0
    static FHRRVector one();   // all phases = π

    // Phase access
    Phase  getPhase(size_t idx) const { return phases_[idx]; }
    void   setPhase(size_t idx, Phase val) { phases_[idx] = std::fmod(val, 2 * M_PI); }

    // Raw phase access (for SIMD)
    const std::vector<Phase>& phases() const { return phases_; }

private:
    std::vector<Phase> phases_;

    static void normalizePhase(Phase& p);
    static Phase  randomPhase(std::mt19937& gen);
};

} // namespace yuki::memory
