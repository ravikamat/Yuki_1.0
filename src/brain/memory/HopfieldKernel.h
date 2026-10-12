#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>

namespace yuki::memory {

// C++20 span-compatible view
template<typename T>
struct Span {
    T* data_;
    size_t size_;
    
    Span() : data_(nullptr), size_(0) {}
    Span(T* data, size_t size) : data_(data), size_(size) {}
    explicit Span(std::vector<T>& vec) : data_(vec.data()), size_(vec.size()) {}
    
    // Implicit conversion from containers so Span parameters accept
    // std::vector (and array-like) arguments. Split by element constness:
    // a const container's data() yields const T*, which must never initialize
    // the mutable T* member, and vice versa. Span<U> arguments are excluded so
    // Span<T> → Span<const T> keeps resolving through the conversion operator
    // below, unambiguously.
    template<typename Container>
        requires std::is_const_v<T> &&
                 (!std::is_same_v<std::decay_t<Container>, Span>) &&
                 (!std::is_same_v<std::decay_t<Container>, Span<std::remove_const_t<T>>>) &&
                 requires(const Container& c) { { c.data() } -> std::convertible_to<T*>; c.size(); }
    Span(const Container& c) : data_(c.data()), size_(c.size()) {}

    template<typename Container>
        requires (!std::is_const_v<T>) &&
                 (!std::is_same_v<std::decay_t<Container>, Span>) &&
                 (!std::is_same_v<std::decay_t<Container>, Span<const T>>) &&
                 requires(Container& c) { { c.data() } -> std::convertible_to<T*>; c.size(); }
    Span(Container& c) : data_(c.data()), size_(c.size()) {}
    
    T* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T& operator[](size_t i) const { return data_[i]; }
    T* begin() const { return data_; }
    T* end() const { return data_ + size_; }
    
    // Allow implicit conversion from Span<T> to Span<const T>
    template<typename U>
    operator Span<const U>() const {
        return Span<const U>(data_, size_);
    }
};

/// Numerical utilities and SIMD-accelerated linear algebra kernels
/// for Modern Continuous Hopfield Networks (Ramsauer et al., 2021).
class HopfieldKernel {
public:
    /// Numerically stable Log-Sum-Exp computation
    /// LSE(x) = m + log(sum(exp(x_i - m))) where m = max(x)
    static float logSumExp(Span<const float> x) noexcept {
        if (x.empty()) return 0.0f;
        float max_val = *std::max_element(x.begin(), x.end());
        if (std::isinf(max_val)) return max_val;

        double sum_exp = 0.0;
        for (float val : x) {
            sum_exp += std::exp(static_cast<double>(val - max_val));
        }
        return static_cast<float>(static_cast<double>(max_val) + std::log(sum_exp));
    }

    /// Numerically stable Softmax in-place
    /// x_i = exp(x_i - max(x)) / sum(exp(x_j - max(x)))
    static void softmax(Span<float> x) noexcept {
        if (x.empty()) return;
        float max_val = *std::max_element(x.begin(), x.end());
        
        double sum = 0.0;
        for (float& val : x) {
            val = static_cast<float>(std::exp(static_cast<double>(val - max_val)));
            sum += val;
        }
        if (sum > 1e-12) {
            float inv_sum = static_cast<float>(1.0 / sum);
            for (float& val : x) {
                val *= inv_sum;
            }
        } else {
            float uniform = 1.0f / static_cast<float>(x.size());
            for (float& val : x) {
                val = uniform;
            }
        }
    }

    /// Vector-vector dot product with AVX2 acceleration if available
    static float dotProduct(Span<const float> a, Span<const float> b) noexcept {
        const size_t n = (std::min)(a.size(), b.size());
        if (n == 0) return 0.0f;

        size_t i = 0;
        float total = 0.0f;

#if defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64)
        __m256 sum256 = _mm256_setzero_ps();
        for (; i + 8 <= n; i += 8) {
            __m256 va = _mm256_loadu_ps(a.data() + i);
            __m256 vb = _mm256_loadu_ps(b.data() + i);
            sum256 = _mm256_fmadd_ps(va, vb, sum256);
        }
        // Horizontal add of sum256
        __m128 lo = _mm256_castps256_ps128(sum256);
        __m128 hi = _mm256_extractf128_ps(sum256, 1);
        __m128 sum128 = _mm_add_ps(lo, hi);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);
        total += _mm_cvtss_f32(sum128);
#endif

        for (; i < n; ++i) {
            total += a[i] * b[i];
        }
        return total;
    }

    /// Matrix-Vector Multiplication: scores = beta * (X * query)
    /// X is row-major matrix: [num_patterns x dim]
    /// query is [dim]
    /// scores is [num_patterns]
    static void matrixVectorMul(
        Span<const float> matrix_row_major,
        size_t num_patterns,
        size_t dim,
        Span<const float> query,
        float beta,
        Span<float> scores_out) noexcept
    {
        for (size_t p = 0; p < num_patterns; ++p) {
            Span<const float> pattern_row(matrix_row_major.data() + p * dim, dim);
            scores_out[p] = beta * dotProduct(pattern_row, query);
        }
    }

    /// Transpose Matrix-Vector Multiplication: result = weights * X
    /// weights is [num_patterns]
    /// X is row-major matrix: [num_patterns x dim]
    /// result is [dim]
    static void matrixTransposeVectorMul(
        Span<const float> matrix_row_major,
        size_t num_patterns,
        size_t dim,
        Span<const float> weights,
        Span<float> result_out) noexcept
    {
        std::fill(result_out.begin(), result_out.end(), 0.0f);
        for (size_t p = 0; p < num_patterns; ++p) {
            float w = weights[p];
            if (std::abs(w) < 1e-8f) continue;
            
            const float* row = matrix_row_major.data() + p * dim;
            size_t i = 0;

#if defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64)
            __m256 vw = _mm256_set1_ps(w);
            for (; i + 8 <= dim; i += 8) {
                __m256 vr = _mm256_loadu_ps(result_out.data() + i);
                __m256 vrow = _mm256_loadu_ps(row + i);
                vr = _mm256_fmadd_ps(vw, vrow, vr);
                _mm256_storeu_ps(result_out.data() + i, vr);
            }
#endif
            for (; i < dim; ++i) {
                result_out[i] += w * row[i];
            }
        }
    }

    /// L2 norm of vector
    static float normL2(Span<const float> v) noexcept {
        return std::sqrt((std::max)(0.0f, dotProduct(v, v)));
    }

    /// Normalize vector in place
    static void normalize(Span<float> v) noexcept {
        float norm = normL2(v);
        if (norm > 1e-8f) {
            float inv = 1.0f / norm;
            for (float& x : v) x *= inv;
        }
    }
};

} // namespace yuki::memory
