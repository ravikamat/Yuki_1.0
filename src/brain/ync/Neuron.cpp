// Neuron.cpp — LIF + STDP + homeostasis + energy budget implementation.
#include "Neuron.h"
#include <algorithm>
#include <cmath>

namespace ync {

// ── ReceptorProfile ─────────────────────────────────────────────────────────

ReceptorProfile ReceptorProfile::random(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    ReceptorProfile r;
    r.dopamine_sens      = dist(rng);
    r.acetylcholine_sens = dist(rng);
    r.serotonin_sens     = dist(rng);
    r.noradrenaline_sens = dist(rng);
    r.excitatory_bias    = (dist(rng) > 0.5f) ? 1.0f : -1.0f;
    return r;
}

float ReceptorProfile::affinity(const ReceptorProfile& other) const {
    float dot  = dopamine_sens      * other.dopamine_sens
               + acetylcholine_sens * other.acetylcholine_sens
               + serotonin_sens     * other.serotonin_sens
               + noradrenaline_sens * other.noradrenaline_sens;
    float norm1 = std::sqrt(dopamine_sens*dopamine_sens
                          + acetylcholine_sens*acetylcholine_sens
                          + serotonin_sens*serotonin_sens
                          + noradrenaline_sens*noradrenaline_sens);
    float norm2 = std::sqrt(other.dopamine_sens*other.dopamine_sens
                          + other.acetylcholine_sens*other.acetylcholine_sens
                          + other.serotonin_sens*other.serotonin_sens
                          + other.noradrenaline_sens*other.noradrenaline_sens);
    if (norm1 < 1e-6f || norm2 < 1e-6f) return 0.0f;
    return std::clamp(dot / (norm1 * norm2), -1.0f, 1.0f);
}

// ── Neuron move ctor/assign ──────────────────────────────────────────────────
// std::atomic<float> is non-movable — we load/store explicitly.

Neuron::Neuron(Neuron&& o) noexcept
    : id(o.id), receptor(o.receptor),
      axons(std::move(o.axons)),
      dendrite_sources(std::move(o.dendrite_sources)) {
    v_membrane.store(       o.v_membrane.load(std::memory_order_relaxed),        std::memory_order_relaxed);
    threshold.store(        o.threshold.load(std::memory_order_relaxed),          std::memory_order_relaxed);
    adaptation.store(       o.adaptation.load(std::memory_order_relaxed),         std::memory_order_relaxed);
    last_spike_time.store(  o.last_spike_time.load(std::memory_order_relaxed),    std::memory_order_relaxed);
    energy.store(           o.energy.load(std::memory_order_relaxed),             std::memory_order_relaxed);
    calcium.store(          o.calcium.load(std::memory_order_relaxed),            std::memory_order_relaxed);
    firing_rate_ema.store(  o.firing_rate_ema.load(std::memory_order_relaxed),    std::memory_order_relaxed);
}

Neuron& Neuron::operator=(Neuron&& o) noexcept {
    if (this == &o) return *this;
    id = o.id; receptor = o.receptor;
    axons = std::move(o.axons);
    dendrite_sources = std::move(o.dendrite_sources);
    v_membrane.store(       o.v_membrane.load(std::memory_order_relaxed),        std::memory_order_relaxed);
    threshold.store(        o.threshold.load(std::memory_order_relaxed),          std::memory_order_relaxed);
    adaptation.store(       o.adaptation.load(std::memory_order_relaxed),         std::memory_order_relaxed);
    last_spike_time.store(  o.last_spike_time.load(std::memory_order_relaxed),    std::memory_order_relaxed);
    energy.store(           o.energy.load(std::memory_order_relaxed),             std::memory_order_relaxed);
    calcium.store(          o.calcium.load(std::memory_order_relaxed),            std::memory_order_relaxed);
    firing_rate_ema.store(  o.firing_rate_ema.load(std::memory_order_relaxed),    std::memory_order_relaxed);
    return *this;
}

// ── LIF integration ─────────────────────────────────────────────────────────

bool Neuron::integrate(float input_current, uint64_t now_ms,
                       float global_dopamine, float global_noradrenaline) {
    if (!isAlive()) return false;

    uint64_t last_spike = last_spike_time.load(std::memory_order_relaxed);
    if (now_ms > last_spike &&
        (now_ms - last_spike) < static_cast<uint64_t>(REFRACTORY_MS)) {
        v_membrane.store(0.0f, std::memory_order_relaxed);
        firing_rate_ema.store(
            firing_rate_ema.load(std::memory_order_relaxed) * 0.95f,
            std::memory_order_relaxed);
        return false;
    }

    float v     = v_membrane.load(std::memory_order_relaxed);
    float adapt = adaptation.load(std::memory_order_relaxed);
    float thresh = threshold.load(std::memory_order_relaxed);

    float dv = -(v - V_REST) / TAU_MEMBRANE + input_current;
    v += dv;

    float eff_thresh = std::max(thresh + adapt - global_noradrenaline * 0.2f, 0.2f);

    bool fired = false;
    if (v > eff_thresh) {
        v = 0.0f;
        adapt += ADAPTATION_INCREMENT;
        last_spike_time.store(now_ms, std::memory_order_release); // RELEASE: paired with acquire in applySTDP
        float e = energy.load(std::memory_order_relaxed) - ENERGY_COST_PER_SPIKE;
        energy.store(std::max(e, 0.0f), std::memory_order_relaxed);
        calcium.store(
            std::min(calcium.load(std::memory_order_relaxed) + 0.1f, 1.0f),
            std::memory_order_relaxed);
        fired = true;
        firing_rate_ema.store(
            firing_rate_ema.load(std::memory_order_relaxed) * 0.95f + 0.05f,
            std::memory_order_relaxed);
        for (auto& axon : axons) axon.last_pre_spike = now_ms;
    } else {
        firing_rate_ema.store(
            firing_rate_ema.load(std::memory_order_relaxed) * 0.95f,
            std::memory_order_relaxed);
    }

    adapt -= adapt / TAU_ADAPTATION;
    adaptation.store(adapt, std::memory_order_relaxed);
    v_membrane.store(v, std::memory_order_relaxed);
    return fired;
}

// ── STDP ────────────────────────────────────────────────────────────────────

void Neuron::applySTDP(uint64_t now_ms, float learning_rate, float dopamine,
                       const std::vector<Neuron>& all_neurons) {
    if (axons.empty()) return;
    static constexpr float A_plus    = 0.01f;
    static constexpr float A_minus   = 0.0105f;
    static constexpr float tau_stdp  = 20.0f;
    static constexpr uint64_t kWindowMs = 100;

    for (auto& axon : axons) {
        uint64_t t_pre = axon.last_pre_spike;
        if (t_pre == 0 || now_ms - t_pre > kWindowMs) continue;
        if (axon.target_id >= all_neurons.size()) continue;

        const Neuron& post = all_neurons[axon.target_id];
        // ACQUIRE: another core's worker may have just fired this neuron
        uint64_t t_post = post.last_spike_time.load(std::memory_order_acquire);
        if (t_post == 0) continue;

        float dt = static_cast<float>(
            static_cast<int64_t>(t_post) - static_cast<int64_t>(t_pre));
        float dw = 0.0f;
        if (dt > 0.0f)      dw =  A_plus  * std::exp(-dt / tau_stdp);
        else if (dt < 0.0f) dw = -A_minus * std::exp( dt / tau_stdp);

        dw *= (1.0f + dopamine * 2.0f);
        axon.weight = std::clamp(axon.weight + learning_rate * dw, -2.0f, 2.0f);
    }
}

// ── Homeostasis ──────────────────────────────────────────────────────────────

void Neuron::homeostaticScale(float target_rate) {
    float current_rate = firing_rate_ema.load(std::memory_order_relaxed);
    if (current_rate < 1e-6f || target_rate < 1e-6f) return;

    float ratio = current_rate / target_rate;
    float scale = 1.0f;
    if      (ratio > 1.2f) scale = 0.99f;
    else if (ratio < 0.8f) scale = 1.01f;

    if (std::abs(scale - 1.0f) > 0.001f) {
        for (auto& axon : axons)
            axon.weight = std::clamp(axon.weight * scale, -2.0f, 2.0f);
    }
}

void Neuron::recoverEnergy(float amount) {
    float e = energy.load(std::memory_order_relaxed) + amount;
    energy.store(std::min(e, 1.0f), std::memory_order_relaxed);
}

} // namespace ync
