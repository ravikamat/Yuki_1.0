// Neuron.h — LIF neuron with atomic<float> state, STDP, homeostasis, energy budget.
#pragma once
#include <atomic>
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>

namespace ync {

struct alignas(64) ReceptorProfile {
    float dopamine_sens      = 0.5f;
    float acetylcholine_sens = 0.5f;
    float serotonin_sens     = 0.5f;
    float noradrenaline_sens = 0.5f;
    float excitatory_bias    = 1.0f;

    static ReceptorProfile random(std::mt19937& rng);
    float affinity(const ReceptorProfile& other) const;
};

struct AxonTerminal {
    uint32_t target_id       = 0;
    float    weight          = 0.0f;
    uint8_t  delay           = 1;
    uint64_t last_pre_spike  = 0;
    std::atomic<float> eligibility_trace{0.0f};

    // Explicit move (atomic<float> is non-movable by default)
    AxonTerminal() = default;
    AxonTerminal(AxonTerminal&& o) noexcept
        : target_id(o.target_id), weight(o.weight), delay(o.delay),
          last_pre_spike(o.last_pre_spike) {
        eligibility_trace.store(o.eligibility_trace.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
    }
    AxonTerminal& operator=(AxonTerminal&& o) noexcept {
        target_id = o.target_id; weight = o.weight; delay = o.delay;
        last_pre_spike = o.last_pre_spike;
        eligibility_trace.store(o.eligibility_trace.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
        return *this;
    }
    AxonTerminal(const AxonTerminal&)            = delete;
    AxonTerminal& operator=(const AxonTerminal&) = delete;
};

class alignas(64) Neuron {
public:
    static constexpr float TAU_MEMBRANE         = 20.0f;
    static constexpr float V_REST               = 0.0f;
    static constexpr float REFRACTORY_MS        = 2.0f;
    static constexpr float THRESHOLD_BASE       = 1.0f;
    static constexpr float ADAPTATION_INCREMENT = 0.1f;
    static constexpr float ENERGY_COST_PER_SPIKE = 0.05f;
    static constexpr float TAU_ADAPTATION       = 100.0f;
    static constexpr float TARGET_FIRING_RATE   = 0.05f;

    uint32_t       id = 0;
    ReceptorProfile receptor;

    std::atomic<float>    v_membrane{0.0f};
    std::atomic<float>    threshold{THRESHOLD_BASE};
    std::atomic<float>    adaptation{0.0f};
    std::atomic<uint64_t> last_spike_time{0};
    std::atomic<float>    energy{1.0f};
    std::atomic<float>    calcium{0.0f};
    std::atomic<float>    firing_rate_ema{0.0f};

    std::vector<AxonTerminal> axons;
    std::vector<uint32_t>     dendrite_sources;

    // Explicit move ctor/assign (std::atomic members make default deleted)
    Neuron() = default;
    Neuron(Neuron&& o) noexcept;
    Neuron& operator=(Neuron&& o) noexcept;
    Neuron(const Neuron&)            = delete;
    Neuron& operator=(const Neuron&) = delete;

    bool integrate(float input_current, uint64_t now_ms,
                   float global_dopamine, float global_noradrenaline);
    void applySTDP(uint64_t now_ms, float learning_rate, float dopamine,
                   const std::vector<Neuron>& all_neurons);
    void homeostaticScale(float target_rate);
    void recoverEnergy(float amount);
    bool isAlive() const noexcept {
        return energy.load(std::memory_order_relaxed) > 0.01f;
    }
};

} // namespace ync
