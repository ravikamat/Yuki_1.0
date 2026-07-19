// ═══════════════════════════════════════════════════════════════════════════
// CacheOptimizer.h — Cache-oblivious data layout + explicit prefetching
//
// Algorithm: Structure-of-Arrays (SoA) conversion for VSE belief vectors
// + explicit prefetch for non-sequential access patterns
//
// Reference:
//   • Prokop, "Cache-Oblivious Algorithms", MIT Master's Thesis, 1999
//   • Intel 64 and IA-32 Architectures Optimization Reference Manual, §9.2
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <type_traits>

// MSVC intrinsics for _mm_prefetch
#if defined(_MSC_VER)
#  include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#  include <immintrin.h>
#endif

namespace yuki::core {

// ─────────────────────────────────────────────────────────────────────────
// Portable prefetch wrapper
// ─────────────────────────────────────────────────────────────────────────
enum class PrefetchHint : int {
    NTA = 0,   // Non-temporal — don't pollute caches (streaming)
    T0  = 1,   // Temporal L1/L2/L3
    T1  = 2,   // Temporal L2/L3 only
    T2  = 3    // Temporal L3 only
};

inline void prefetch_read(const void* addr,
                          PrefetchHint hint = PrefetchHint::T0) noexcept {
#if defined(_MSC_VER)
    _mm_prefetch(static_cast<const char*>(addr), static_cast<int>(hint));
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(addr, /*read=*/0, static_cast<int>(hint));
#else
    (void)addr; (void)hint;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
// Aligned allocator helpers (cache-line-aligned heap allocation)
// ─────────────────────────────────────────────────────────────────────────
inline void* cache_line_alloc(size_t bytes) noexcept {
#if defined(_MSC_VER)
    return _aligned_malloc(bytes, 64);
#else
    void* ptr = nullptr;
    if (::posix_memalign(&ptr, 64, bytes) != 0) return nullptr;
    return ptr;
#endif
}

inline void cache_line_free(void* ptr) noexcept {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

// ─────────────────────────────────────────────────────────────────────────
// SoAVector<T> — Cache-line-aligned array with prefetch utilities
//
// Replaces AoS (Array of Structs) with SoA (Structure of Arrays).
//
// AoS layout problem (example with 3-field Belief struct, 12 bytes each):
//   [level|conf|ts][level|conf|ts][level|conf|ts]...
//   Iterating over "level" touches every 12 bytes → ~5 structs per 64-byte
//   cache line → 3× cache pressure vs. pure SoA.
//
// SoA layout: separate float level[], float confidence[], double timestamp[]
//   Iterating over level[] touches 16 floats per cache line → ~16×.
// ─────────────────────────────────────────────────────────────────────────
template<typename T>
class SoAVector {
    static_assert(std::is_trivially_copyable<T>::value,
                  "SoAVector<T>: T must be trivially copyable");

public:
    SoAVector() = default;

    explicit SoAVector(size_t n) : size_(n), cap_(n) {
        data_ = static_cast<T*>(cache_line_alloc(n == 0 ? 64 : n * sizeof(T)));
        if (data_) std::memset(data_, 0, n * sizeof(T));
    }

    ~SoAVector() {
        if (data_) cache_line_free(data_);
    }

    // Move-only (no accidental expensive copies)
    SoAVector(SoAVector&& o) noexcept
        : data_(o.data_), size_(o.size_), cap_(o.cap_) {
        o.data_ = nullptr; o.size_ = 0; o.cap_ = 0;
    }
    SoAVector& operator=(SoAVector&& o) noexcept {
        if (this != &o) {
            if (data_) cache_line_free(data_);
            data_ = o.data_; size_ = o.size_; cap_ = o.cap_;
            o.data_ = nullptr; o.size_ = 0; o.cap_ = 0;
        }
        return *this;
    }
    SoAVector(const SoAVector&)            = delete;
    SoAVector& operator=(const SoAVector&) = delete;

    T&       operator[](size_t i)       noexcept { return data_[i]; }
    const T& operator[](size_t i) const noexcept { return data_[i]; }

    T*       data()       noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    size_t size()     const noexcept { return size_; }
    size_t capacity() const noexcept { return cap_;  }
    bool   empty()    const noexcept { return size_ == 0; }

    // ── Prefetch element (i + distance) into the L1 cache ────────────────
    // Call once per loop iteration; the hardware fetcher issues the load
    // ~200 cycles before you actually access data_[i + distance].
    // Default distance of 8 = 512 bytes @ float ≈ 128 floats ahead,
    // tuned for typical DRAM latency of ~200 cycles @ 3 GHz.
    void prefetch_ahead(size_t i, size_t distance = 8) const noexcept {
        size_t target = i + distance;
        if (target < size_) {
            prefetch_read(&data_[target], PrefetchHint::T0);
        }
    }

    // ── Cache-oblivious block iteration ──────────────────────────────────
    // Process elements in blocks that fit in L1 (32 KB on typical x86).
    // Default block_size = 1024 floats = 4 KB (well within L1).
    // Prefetches the next block start while processing the current block.
    // Func signature: void(T& element, size_t index)
    template<typename Func>
    void for_each_block(Func&& f, size_t block_size = 1024) {
        for (size_t bs = 0; bs < size_; bs += block_size) {
            size_t be = std::min(bs + block_size, size_);
            // Prefetch the start of the *next* block
            if (be < size_) prefetch_read(&data_[be], PrefetchHint::T0);
            for (size_t i = bs; i < be; ++i) {
                f(data_[i], i);
            }
        }
    }

private:
    T*     data_ = nullptr;
    size_t size_ = 0;
    size_t cap_  = 0;
};

// ─────────────────────────────────────────────────────────────────────────
// BeliefSoA — SoA layout for VSE belief states
//
// Replaces:
//   struct Belief { float level; float confidence; double timestamp; };
//   std::vector<Belief> beliefs;   // AoS layout
//
// With:
//   BeliefSoA beliefs(N);          // separate arrays per field
//
// Cache behaviour:
//   AoS: sizeof(Belief) = 16, so 4 beliefs per cache line → ~4× misses.
//   SoA: levels[] → 16 floats per cache line, 4× better on level sweep.
// ─────────────────────────────────────────────────────────────────────────
struct BeliefSoA {
    SoAVector<float>  levels;       // Competence / curiosity level [0,1]
    SoAVector<float>  confidences;  // Per-belief confidence        [0,1]
    SoAVector<double> timestamps;   // Last-update time (seconds)
    SoAVector<float>  precisions;   // Inverse variance / precision weight

    BeliefSoA() = default;
    explicit BeliefSoA(size_t n)
        : levels(n), confidences(n), timestamps(n), precisions(n) {}

    // Move-only (all members are move-only)
    BeliefSoA(BeliefSoA&&)            = default;
    BeliefSoA& operator=(BeliefSoA&&) = default;
    BeliefSoA(const BeliefSoA&)            = delete;
    BeliefSoA& operator=(const BeliefSoA&) = delete;

    size_t size() const noexcept { return levels.size(); }

    // Prefetch all four fields at index (i + distance) simultaneously.
    // Issuing all four prefetches at once lets the memory controller pipeline
    // them, reducing the effective stall latency.
    void prefetch_ahead(size_t i, size_t distance = 8) const noexcept {
        levels.prefetch_ahead(i, distance);
        confidences.prefetch_ahead(i, distance);
        timestamps.prefetch_ahead(i, distance);
        precisions.prefetch_ahead(i, distance);
    }
};

// ─────────────────────────────────────────────────────────────────────────
// Prefetch distance recommendation
// Tuned for typical x86-64 desktop/laptop with 3 GHz clock:
//   • DRAM latency ≈ 60 ns ≈ 180 cycles
//   • Iterations per cycle: 1 (pipelined)
//   • Distance = ceil(latency / iter_ns) ≈ ceil(60 / 0.33) ≈ 182 iters
//   • For 4-byte floats, 8 CL × 16 floats/CL = 128 elements
//   • We use 8 cache lines = 512 bytes to match the hardware prefetcher's
//     "adjacent cache line prefetch" that already covers +1 CL.
// ─────────────────────────────────────────────────────────────────────────
static constexpr size_t PREFETCH_DISTANCE_ELEMENTS = 128; // 8 CL × 16 f32

} // namespace yuki::core
