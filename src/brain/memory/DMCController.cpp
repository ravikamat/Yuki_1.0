// ═══════════════════════════════════════════════════════════════════════════
// DMCController.cpp — MLP forward pass + head parameter unpacking
// ═══════════════════════════════════════════════════════════════════════════
#include "DMCController.h"

#include <cmath>
#include <random>

namespace yuki::brain::memory {

DMCController::DMCController() {
    std::random_device rd;
    std::mt19937 rng(rd());
    mlp_.initialize_xavier(rng);
}

void DMCController::compute_head_params(
    const std::array<float, INPUT_DIM>& input,
    DMCHeadParams& read_params,
    DMCHeadParams& write_params) const
{
    auto output = mlp_.forward(input);

    // ── Read head parameters ─────────────────────────────────────────────
    // Key: [0..15]
    for (size_t i = 0; i < 16; ++i)
        read_params.key[i] = output[i];

    // Key strength: softplus(output[16]) + 1  → β ≥ 1
    read_params.key_strength = softplus(output[16]) + 1.0f;

    // Interpolation gate: sigmoid(output[17])
    read_params.interpolation_gate = MLP::sigmoid(output[17]);

    // Shift: [18..22], softmax-normalized
    for (size_t i = 0; i < 5; ++i)
        read_params.shift[i] = output[18 + i];
    read_params.normalize_shift();

    // Gamma: softplus(output[23]) + 1  → γ ≥ 1
    read_params.gamma = softplus(output[23]) + 1.0f;
    read_params.clamp_gamma();

    // ── Write head parameters ────────────────────────────────────────────
    // Write key shares first 8 dims of read key; remaining 8 are zero
    for (size_t i = 0; i < 8; ++i)
        write_params.key[i] = read_params.key[i];
    for (size_t i = 8; i < 16; ++i)
        write_params.key[i] = 0.0f;

    // Write key strength: slightly higher (bias toward precise writes)
    write_params.key_strength = read_params.key_strength * 1.1f;

    // Write gate: sigmoid(output[17] + 0.5) — shifted toward content-focus
    write_params.interpolation_gate = MLP::sigmoid(output[17] + 0.5f);

    // Shift and gamma shared with read head
    write_params.shift = read_params.shift;
    write_params.gamma = read_params.gamma;

    // Erase vector: sigmoid([24..31]) → [0,1]^8
    for (size_t i = 0; i < 8; ++i)
        write_params.erase_vector[i] = MLP::sigmoid(output[24 + i]);
    write_params.clamp_erase();

    // Add vector: tanh([24..31]) → [-1,1]^8  (same dims, dual role)
    for (size_t i = 0; i < 8; ++i)
        write_params.add_vector[i] = std::tanh(output[24 + i]);
}

} // namespace yuki::brain::memory
