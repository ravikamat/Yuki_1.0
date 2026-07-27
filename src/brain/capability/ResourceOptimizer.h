#pragma once
#include "CapabilityGraph.h"
#include "PathFinder.h"
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace yuki::system {
class ResourceMonitor;
}

namespace yuki::organism {
class EconomyEngine;
}

namespace yuki::capability {

struct WaveSchedule {
    std::vector<std::vector<uint32_t>> waves;
    std::vector<float> wave_estimated_ram_mb;
    std::vector<float> wave_estimated_cpu_percent;
    bool feasible{false};
};

class ResourceOptimizer {
public:
    ResourceOptimizer(::yuki::system::ResourceMonitor* monitor, ::yuki::organism::EconomyEngine* economy);

    WaveSchedule computeWaveSchedule(const PathResult& path, const CapabilityGraph& graph);
    bool canRunInParallel(uint32_t node_a, uint32_t node_b, const CapabilityGraph& graph);
    bool allocateResources(uint32_t node_id, const CapabilityGraph& graph);
    void releaseResources(uint32_t node_id, const CapabilityGraph& graph);
    bool predictStarvation(const PathResult& path, const CapabilityGraph& graph);
    PathFinderConfig deriveConfigFromSystemState();

private:
    ::yuki::system::ResourceMonitor* monitor_{nullptr};
    ::yuki::organism::EconomyEngine* economy_{nullptr};
    std::unordered_map<uint32_t, std::pair<float, float>> active_allocations_;

    float getAvailableRam() const;
    float getAvailableCpu() const;
};

} // namespace yuki::capability
