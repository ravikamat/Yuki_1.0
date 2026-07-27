// GrowthCone.h — axonal pathfinding by receptor affinity.
#pragma once
#include "Neuron.h"
#include <vector>
#include <random>
#include <utility>

namespace ync {

class GrowthCone {
public:
    uint32_t source_neuron_id = 0;
    float    reach            = 50.0f;
    uint8_t  max_synapses     = 20;
    float    chemotaxis       = 0.8f;

    std::vector<std::pair<uint32_t, float>> seek(
        const std::vector<Neuron>& neurons,
        float global_dopamine,
        std::mt19937& rng);

    AxonTerminal formSynapse(uint32_t target_id, float affinity, std::mt19937& rng);

private:
    float connectionProbability(
        const ReceptorProfile& pre,
        const ReceptorProfile& post,
        float distance,
        float dopamine) const;
};

} // namespace ync
