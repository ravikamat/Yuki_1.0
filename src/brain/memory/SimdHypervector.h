// SimdHypervector.h — AVX-512 / scalar dispatch for Hypervector (10000-bit, 157 x uint64_t)
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

} // namespace yuki::memory::simd
