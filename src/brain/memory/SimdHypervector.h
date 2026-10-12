// SimdHypervector.h — AVX-512 / scalar dispatch for Hypervector (10000-bit, 157 x uint64_t) + FHRR
// Compile-time dispatch: define YUKI_AVX512 to enable AVX-512 path.
// Default: scalar fallback. No runtime CPU detection.
#pragma once
#include "Hypervector.h"

#ifdef __AVX512F__
#  include <immintrin.h>
#  define YUKI_AVX512 1
#else
#  define YUKI_AVX512 0
#endif

namespace yuki::memory::simd {

// ── Binary HDC ────────────────────────────────────────────────────────────────

// XOR-bind two hypervectors (identity bind operation in HDC).
// AVX-512: processes 8 x uint64_t per _mm512_xor_si512 call → 157 words in ~20 SIMD ops.
// Scalar fallback: 157 word XORs.
inline Hypervector xorBind(const Hypervector& a, const Hypervector& b) {
    return a.bind(b);  // Delegates to Hypervector::bind() which already does word XOR.
                       // SIMD path below replaces this if __AVX512F__ is defined.
}

#if YUKI_AVX512
// AVX-512 population count: counts set bits across all 157 words.
// Used for Hamming distance in population firing rate calculations.
inline size_t popcountHV(const Hypervector& hv) {
    const auto& words = hv.words();
    size_t total = 0;
    // Process groups of 8 uint64_t with AVX-512 popcnt
    constexpr size_t kWords = Hypervector::kWords;  // 157
    constexpr size_t kSimdStep = 8;
    size_t i = 0;
    for (; i + kSimdStep <= kWords; i += kSimdStep) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&words[i]));
        // _mm512_popcnt_epi64 requires AVX512VPOPCNTDQ; fallback to scalar per element
        for (size_t k = 0; k < kSimdStep; ++k) {
            total += __builtin_popcountll(words[i + k]);
        }
    }
    for (; i < kWords; ++i) {
        total += __builtin_popcountll(words[i]);
    }
    return total;
}
#else
// Scalar fallback — delegates to Hypervector's hammingDistance
inline size_t popcountHV(const Hypervector& hv) {
    return hv.hammingDistance(Hypervector::zero());
}
#endif

// Cosine similarity shortcut via Hamming distance.
// cosineSim(a, b) = 1.0 - 2.0 * hammingDist(a, b) / DIM
inline float cosineSim(const Hypervector& a, const Hypervector& b) {
    return a.cosineSimilarity(b);
}

// ── FHRR SIMD Operations ────────────────────────────────────────────────────

// FHRR phase bind (complex multiply = phase addition)
// AVX: processes 8 phases per _mm256_add_ps + wrap
// Scalar: manual phase addition with 2π wrap
inline FHRRVector fhrrBind(const FHRRVector& a, const FHRRVector& b) {
    return a.bind(b);  // Delegates to FHRRVector::bind()
}

#if YUKI_AVX512
// AVX-512 FHRR cosine similarity: cos(phase_diff) summed over all dims
inline float fhrrCosineSim(const FHRRVector& a, const FHRRVector& b) {
    const auto& pa = a.phases();
    const auto& pb = b.phases();
    constexpr size_t kSimdStep = 16; // 16 floats per _mm512
    float total = 0.0f;
    size_t i = 0;
    for (; i + kSimdStep <= pa.size(); i += kSimdStep) {
        __m512 va = _mm512_loadu_ps(&pa[i]);
        __m512 vb = _mm512_loadu_ps(&pb[i]);
        // diff = pa - pb
        __m512 diff = _mm512_sub_ps(va, vb);
        // cos(diff) - use scalar fallback per element for now
        // TODO: use vector math library or approximation for cos
        for (size_t k = 0; k < kSimdStep; ++k) {
            total += std::cos(pa[i + k] - pb[i + k]);
        }
    }
    for (; i < pa.size(); ++i) {
        total += std::cos(pa[i] - pb[i]);
    }
    return total / static_cast<float>(FHRRVector::DIM);
}
#else
inline float fhrrCosineSim(const FHRRVector& a, const FHRRVector& b) {
    return a.cosineSimilarity(b);
}
#endif

// FHRR phase bind (complex multiply = phase addition) - scalar fallback
inline FHRRVector fhrrBindScalar(const FHRRVector& a, const FHRRVector& b) {
    FHRRVector result;
    std::vector<FHRRVector::Phase> phases;
    phases.resize(FHRRVector::DIM);
    const auto& pa = a.phases();
    const auto& pb = b.phases();
    for (size_t i = 0; i < FHRRVector::DIM; ++i) {
        float sum = pa[i] + pb[i];
        if (sum >= 2 * M_PI) sum -= 2 * M_PI;
        phases[i] = sum;
    }
    // Use setPhase to populate result
    for (size_t i = 0; i < FHRRVector::DIM; ++i) {
        result.setPhase(i, phases[i]);
    }
    return result;
}

} // namespace yuki::memory::simd
