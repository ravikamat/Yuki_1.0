// DevelopmentalEngine.cpp — 5-stage cortical maturation.
#include "DevelopmentalEngine.h"
#include <algorithm>
#include <cmath>

namespace ync {

DevelopmentalEngine::StageParams DevelopmentalEngine::paramsForStage(DevelopmentalStage s) {
    switch (s) {
        case DevelopmentalStage::NEUROGENESIS:
            return {1000u, 0u, 0.0f, 0.5f, 0.0f, true};
        case DevelopmentalStage::SYNAPTOGENESIS:
            return {1000u, 200u, 0.01f, 0.8f, 0.0f, true};
        case DevelopmentalStage::CRITICAL_PERIOD:
            return {10000u, 100u, 0.05f, 0.6f, 0.0f, true};
        case DevelopmentalStage::PRUNING:
            return {10000u, 20u, 0.02f, 0.3f, 0.05f, false};
        case DevelopmentalStage::STABILIZATION:
            return {50000u, 50u, 0.005f, 0.2f, 0.02f, false};
    }
    return {1000u, 0u, 0.0f, 0.5f, 0.0f, true};
}

uint64_t DevelopmentalEngine::msForStage(DevelopmentalStage s) const noexcept {
    switch (s) {
        case DevelopmentalStage::NEUROGENESIS:    return   3'600'000ULL;
        case DevelopmentalStage::SYNAPTOGENESIS:  return  21'600'000ULL;
        case DevelopmentalStage::CRITICAL_PERIOD: return  86'400'000ULL;
        case DevelopmentalStage::PRUNING:         return  43'200'000ULL;
        case DevelopmentalStage::STABILIZATION:   return UINT64_MAX;
    }
    return UINT64_MAX;
}

void DevelopmentalEngine::initialize(uint32_t seed_neuron_count,
                                     std::vector<Neuron>& neurons,
                                     std::mt19937& rng) {
    neurons.clear();
    neurons.reserve(seed_neuron_count);
    for (uint32_t i = 0; i < seed_neuron_count; ++i) {
        Neuron n;
        n.id       = i;
        n.receptor = ReceptorProfile::random(rng);
        neurons.push_back(std::move(n));
    }
    stage          = DevelopmentalStage::NEUROGENESIS;
    stage_progress = 0.0f;
    simulated_ms   = 0;
}

void DevelopmentalEngine::tick(uint64_t timestep_ms, std::vector<Neuron>& neurons,
                               NeuromodulatorState& nms, std::mt19937& rng) {
    simulated_ms += timestep_ms;
    uint64_t stage_ms = msForStage(stage);
    stage_progress = (stage_ms != UINT64_MAX)
        ? static_cast<float>(simulated_ms % stage_ms) / static_cast<float>(stage_ms)
        : 0.0f;

    auto params = paramsForStage(stage);
    nms.dopamine.store(params.dopamine_baseline, std::memory_order_relaxed);

    switch (stage) {
        case DevelopmentalStage::NEUROGENESIS:    runNeurogenesis(neurons, rng);           break;
        case DevelopmentalStage::SYNAPTOGENESIS:  runSynaptogenesis(neurons, nms, rng);    break;
        case DevelopmentalStage::CRITICAL_PERIOD: runCriticalPeriod(neurons, nms);         break;
        case DevelopmentalStage::PRUNING:         runPruning(neurons);                     break;
        case DevelopmentalStage::STABILIZATION:   runStabilization(neurons);               break;
    }

    // Automatic stage advancement
    if (stage == DevelopmentalStage::NEUROGENESIS    && simulated_ms >= 3'600'000ULL)
        advanceStage();
    else if (stage == DevelopmentalStage::SYNAPTOGENESIS   && simulated_ms >= 25'200'000ULL)
        advanceStage();
    else if (stage == DevelopmentalStage::CRITICAL_PERIOD  && simulated_ms >= 111'600'000ULL)
        advanceStage();
}

void DevelopmentalEngine::advanceStage() {
    switch (stage) {
        case DevelopmentalStage::NEUROGENESIS:    stage = DevelopmentalStage::SYNAPTOGENESIS;  break;
        case DevelopmentalStage::SYNAPTOGENESIS:  stage = DevelopmentalStage::CRITICAL_PERIOD; break;
        case DevelopmentalStage::CRITICAL_PERIOD: stage = DevelopmentalStage::PRUNING;         break;
        case DevelopmentalStage::PRUNING:         stage = DevelopmentalStage::STABILIZATION;   break;
        case DevelopmentalStage::STABILIZATION:                                                  break;
    }
    stage_progress = 0.0f;
}

void DevelopmentalEngine::runNeurogenesis(std::vector<Neuron>& neurons, std::mt19937& rng) {
    static constexpr size_t kSpawnBatch = 100;
    auto params = paramsForStage(DevelopmentalStage::NEUROGENESIS);
    if (neurons.size() < params.target_neurons) {
        size_t to_spawn = std::min(kSpawnBatch,
                                   static_cast<size_t>(params.target_neurons - neurons.size()));
        uint32_t base_id = static_cast<uint32_t>(neurons.size());
        for (size_t i = 0; i < to_spawn; ++i) {
            Neuron n;
            n.id       = base_id + static_cast<uint32_t>(i);
            n.receptor = ReceptorProfile::random(rng);
            neurons.push_back(std::move(n));
        }
    }
}

void DevelopmentalEngine::runSynaptogenesis(std::vector<Neuron>& neurons,
                                             NeuromodulatorState& nms,
                                             std::mt19937& rng) {
    static constexpr size_t kMaxAxons = 200;
    float dopamine = nms.dopamine.load(std::memory_order_relaxed);
    for (auto& neuron : neurons) {
        if (neuron.axons.size() >= kMaxAxons) continue;
        GrowthCone cone;
        cone.source_neuron_id = neuron.id;
        cone.max_synapses     = 20;
        cone.reach            = 100.0f;
        auto targets = cone.seek(neurons, dopamine, rng);
        for (auto& [tid, affinity] : targets) {
            if (neuron.axons.size() >= kMaxAxons) break;
            bool exists = false;
            for (const auto& axon : neuron.axons)
                if (axon.target_id == tid) { exists = true; break; }
            if (!exists) {
                neuron.axons.push_back(cone.formSynapse(tid, affinity, rng));
                if (tid < neurons.size())
                    neurons[tid].dendrite_sources.push_back(neuron.id);
            }
        }
    }
}

void DevelopmentalEngine::runCriticalPeriod(std::vector<Neuron>& neurons,
                                             NeuromodulatorState& /*nms*/) {
    static constexpr size_t kSpawnBatch = 500;
    auto params = paramsForStage(DevelopmentalStage::CRITICAL_PERIOD);
    if (neurons.size() < params.target_neurons) {
        size_t to_spawn = std::min(kSpawnBatch,
                                   static_cast<size_t>(params.target_neurons - neurons.size()));
        uint32_t base_id = static_cast<uint32_t>(neurons.size());
        std::mt19937 rng(static_cast<uint32_t>(simulated_ms));
        for (size_t i = 0; i < to_spawn; ++i) {
            Neuron n;
            n.id       = base_id + static_cast<uint32_t>(i);
            n.receptor = ReceptorProfile::random(rng);
            neurons.push_back(std::move(n));
        }
    }
}

void DevelopmentalEngine::runPruning(std::vector<Neuron>& neurons) {
    size_t total_before = 0;
    for (const auto& n : neurons) total_before += n.axons.size();

    auto params = paramsForStage(DevelopmentalStage::PRUNING);
    for (auto& neuron : neurons) {
        neuron.axons.erase(
            std::remove_if(neuron.axons.begin(), neuron.axons.end(),
                [&](const AxonTerminal& axon) {
                    return std::abs(axon.weight) < params.pruning_threshold;
                }),
            neuron.axons.end());
    }

    size_t total_after = 0;
    for (const auto& n : neurons) total_after += n.axons.size();
    if (total_before > 0 && total_after <= total_before / 2) advanceStage();
}

void DevelopmentalEngine::runStabilization(std::vector<Neuron>& neurons) {
    static constexpr size_t kSpawnBatch = 1000;
    auto params = paramsForStage(DevelopmentalStage::STABILIZATION);
    if (neurons.size() < params.target_neurons) {
        size_t to_spawn = std::min(kSpawnBatch,
                                   static_cast<size_t>(params.target_neurons - neurons.size()));
        uint32_t base_id = static_cast<uint32_t>(neurons.size());
        std::mt19937 rng(static_cast<uint32_t>(simulated_ms));
        for (size_t i = 0; i < to_spawn; ++i) {
            Neuron n;
            n.id       = base_id + static_cast<uint32_t>(i);
            n.receptor = ReceptorProfile::random(rng);
            neurons.push_back(std::move(n));
        }
    }
}

} // namespace ync
