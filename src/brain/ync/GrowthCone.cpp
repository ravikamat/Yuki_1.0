// GrowthCone.cpp — chemical-affinity-based axonal pathfinding.
#include "GrowthCone.h"
#include <algorithm>
#include <cmath>

namespace ync {

std::vector<std::pair<uint32_t, float>> GrowthCone::seek(
    const std::vector<Neuron>& neurons,
    float global_dopamine,
    std::mt19937& rng) {

    std::vector<std::pair<uint32_t, float>> candidates;
    if (source_neuron_id >= neurons.size()) return candidates;

    const ReceptorProfile& pre = neurons[source_neuron_id].receptor;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (uint32_t i = 0; i < static_cast<uint32_t>(neurons.size()); ++i) {
        if (i == source_neuron_id) continue;
        float distance = std::abs(static_cast<float>(i)
                                - static_cast<float>(source_neuron_id));
        if (distance > reach) continue;
        float p = connectionProbability(pre, neurons[i].receptor, distance, global_dopamine);
        if (dist(rng) < p) candidates.emplace_back(i, p);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (candidates.size() > max_synapses)
        candidates.resize(max_synapses);
    return candidates;
}

AxonTerminal GrowthCone::formSynapse(uint32_t target_id, float affinity, std::mt19937& rng) {
    AxonTerminal axon;
    axon.target_id = target_id;
    std::uniform_real_distribution<float> wdist(0.01f, 0.1f);
    axon.weight = wdist(rng) * affinity;
    std::uniform_int_distribution<int> ddist(1, 19);
    axon.delay          = static_cast<uint8_t>(ddist(rng));
    axon.last_pre_spike = 0;
    axon.eligibility_trace.store(0.0f, std::memory_order_relaxed);
    return axon;
}

float GrowthCone::connectionProbability(
    const ReceptorProfile& pre,
    const ReceptorProfile& post,
    float distance,
    float dopamine) const {
    float affinity       = pre.affinity(post);
    float dopamine_boost = 1.0f + dopamine * 3.0f;
    float local_bias     = std::exp(-distance / reach);
    float p              = ((affinity + 1.0f) * 0.5f) * dopamine_boost * local_bias;
    return std::clamp(p, 0.0f, 1.0f);
}

} // namespace ync
