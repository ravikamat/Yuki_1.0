// ═══════════════════════════════════════════════════════════════════════════
// DMCController.h — Tiny MLP controller for DMC read/write head parameters
//
// Architecture: Input(64) -> Hidden(128, ReLU) -> Hidden(64, ReLU) -> Output(32)
// Total parameters: ~10K
//
// Input: VSE belief state (24) + last read vector (24) + context (16) = 64
// Output: Head parameters for 1 read head + 1 write head (packed into 32 dims)
//
// Reference:
//   • Graves et al., "Neural Turing Machines", arXiv:1410.5401, 2014
//   • DeepMind, "Hybrid computing using a neural network with dynamic external
//     memory", Nature 538, 471–476, 2016
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace yuki::brain::memory {

// ─────────────────────────────────────────────────────────────────────────
// TinyMLP — fixed-size MLP (no heap on hot path, stack-allocated intermediates)
// ─────────────────────────────────────────────────────────────────────────
template<size_t IN, size_t H1, size_t H2, size_t OUT>
class TinyMLP {
    static_assert(IN > 0 && H1 > 0 && H2 > 0 && OUT > 0,
                  "All dimensions must be > 0");
public:
    // ── Weight storage ───────────────────────────────────────────────────
    std::array<float, IN * H1>  w1;
    std::array<float, H1>       b1;
    std::array<float, H1 * H2>  w2;
    std::array<float, H2>       b2;
    std::array<float, H2 * OUT> w3;
    std::array<float, OUT>      b3;

    // ── Activations ──────────────────────────────────────────────────────
    template<size_t N>
    static void relu_inplace(std::array<float, N>& x) noexcept {
        for (auto& v : x) v = v > 0.0f ? v : 0.0f;
    }

    static float sigmoid(float x) noexcept {
        return 1.0f / (1.0f + std::exp(-x));
    }

    template<size_t N>
    static void softmax_inplace(std::array<float, N>& x) noexcept {
        float mx = *std::max_element(x.begin(), x.end());
        float s  = 0.0f;
        for (auto& v : x) { v = std::exp(v - mx); s += v; }
        for (auto& v : x) v /= s;
    }

    // ── Forward pass (all intermediates on stack) ────────────────────────
    std::array<float, OUT> forward(const std::array<float, IN>& input) const noexcept {
        // Layer 1
        std::array<float, H1> h1{};
        for (size_t o = 0; o < H1; ++o) {
            float s = b1[o];
            for (size_t i = 0; i < IN; ++i) s += input[i] * w1[i * H1 + o];
            h1[o] = s;
        }
        relu_inplace(h1);

        // Layer 2
        std::array<float, H2> h2{};
        for (size_t o = 0; o < H2; ++o) {
            float s = b2[o];
            for (size_t i = 0; i < H1; ++i) s += h1[i] * w2[i * H2 + o];
            h2[o] = s;
        }
        relu_inplace(h2);

        // Layer 3
        std::array<float, OUT> out{};
        for (size_t o = 0; o < OUT; ++o) {
            float s = b3[o];
            for (size_t i = 0; i < H2; ++i) s += h2[i] * w3[i * OUT + o];
            out[o] = s;
        }
        return out;
    }

    // ── Xavier / He initialization ────────────────────────────────────────
    void initialize_xavier(std::mt19937& rng) {
        std::normal_distribution<float> dist;
        auto init = [&](auto& w, size_t fan_in, size_t fan_out) {
            float scale = std::sqrt(2.0f / static_cast<float>(fan_in + fan_out));
            for (auto& v : w) v = dist(rng) * scale;
        };
        init(w1, IN, H1);
        init(w2, H1, H2);
        init(w3, H2, OUT);
        for (auto& v : b1) v = 0.01f;
        for (auto& v : b2) v = 0.01f;
        for (auto& v : b3) v = 0.00f;
    }

    static constexpr size_t param_count() noexcept {
        return (IN * H1 + H1) + (H1 * H2 + H2) + (H2 * OUT + OUT);
    }
};

// ─────────────────────────────────────────────────────────────────────────
// DMCHeadParams — parameters emitted by controller for one read/write head
// ─────────────────────────────────────────────────────────────────────────
struct DMCHeadParams {
    std::array<float, 16> key{};
    float key_strength       = 1.0f;  // β ≥ 1
    float interpolation_gate = 0.5f;  // g ∈ [0,1]
    std::array<float, 5> shift{};     // {-2,-1,0,1,2}, softmax-normalized
    float gamma              = 1.0f;  // γ ≥ 1

    // Write-specific
    std::array<float, 8> erase_vector{};  // ∈ [0,1]^8
    std::array<float, 8> add_vector{};    // ∈ [-1,1]^8

    void normalize_shift() noexcept {
        float s = 0.0f;
        for (auto& v : shift) { v = std::exp(v); s += v; }
        for (auto& v : shift) v /= s;
    }
    void clamp_erase() noexcept {
        for (auto& v : erase_vector)
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
    void clamp_gamma() noexcept { if (gamma < 1.0f) gamma = 1.0f; }
};

// ─────────────────────────────────────────────────────────────────────────
// DMCController — MLP that maps VSE state → head parameters
// ─────────────────────────────────────────────────────────────────────────
class DMCController {
public:
    static constexpr size_t INPUT_DIM  = 64;
    static constexpr size_t HIDDEN1    = 128;
    static constexpr size_t HIDDEN2    = 64;
    static constexpr size_t OUTPUT_DIM = 32;

    using MLP = TinyMLP<INPUT_DIM, HIDDEN1, HIDDEN2, OUTPUT_DIM>;

    DMCController();

    void compute_head_params(
        const std::array<float, INPUT_DIM>& input,
        DMCHeadParams& read_params,
        DMCHeadParams& write_params) const;

    MLP&       mlp()       noexcept { return mlp_; }
    const MLP& mlp() const noexcept { return mlp_; }

    static constexpr size_t parameter_count() noexcept {
        return MLP::param_count();
    }

private:
    MLP mlp_;

    // softplus helper: log(1 + exp(x))
    static float softplus(float x) noexcept {
        return x > 20.0f ? x : std::log(1.0f + std::exp(x));
    }
};

} // namespace yuki::brain::memory
