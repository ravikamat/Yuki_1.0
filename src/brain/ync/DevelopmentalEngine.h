// DevelopmentalEngine.h — 5-stage cortical maturation controller.
#pragma once
#include "Neuron.h"
#include "NeuromodulatorState.h"
#include "GrowthCone.h"
#include <vector>
#include <random>
#include <cstdint>

namespace ync {

enum class DevelopmentalStage {
    NEUROGENESIS,    // Random neuron genesis, max growth
    SYNAPTOGENESIS,  // Rapid synapse formation, high dopamine
    CRITICAL_PERIOD, // STDP peak, competition for survival
    PRUNING,         // Weak synapse elimination
    STABILIZATION    // Mature, slow homeostasis
};

class DevelopmentalEngine {
public:
    struct StageParams {
        uint32_t target_neurons;
        uint32_t synapses_per_neuron;
        float    learning_rate;
        float    dopamine_baseline;
        float    pruning_threshold;
        bool     allow_new_synapses;
    };

    DevelopmentalStage stage         = DevelopmentalStage::NEUROGENESIS;
    float              stage_progress = 0.0f;
    uint64_t           simulated_ms   = 0;

    static StageParams paramsForStage(DevelopmentalStage s);

    void initialize(uint32_t seed_neuron_count, std::vector<Neuron>& neurons, std::mt19937& rng);
    void tick(uint64_t timestep_ms, std::vector<Neuron>& neurons,
              NeuromodulatorState& nms, std::mt19937& rng);
    void advanceStage();

private:
    void runNeurogenesis(std::vector<Neuron>& neurons, std::mt19937& rng);
    void runSynaptogenesis(std::vector<Neuron>& neurons, NeuromodulatorState& nms, std::mt19937& rng);
    void runCriticalPeriod(std::vector<Neuron>& neurons, NeuromodulatorState& nms);
    void runPruning(std::vector<Neuron>& neurons);
    void runStabilization(std::vector<Neuron>& neurons);
    uint64_t msForStage(DevelopmentalStage s) const noexcept;
};

} // namespace ync
