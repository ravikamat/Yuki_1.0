#include "ResourceOptimizer.h"
#include "brain/system/ResourceMonitor.h"
#include "brain/organism/EconomyEngine.h"
#include <algorithm>

using namespace yuki::capability;

ResourceOptimizer::ResourceOptimizer(::yuki::system::ResourceMonitor* monitor,
                                     ::yuki::organism::EconomyEngine* economy)
    : monitor_(monitor), economy_(economy) {}

WaveSchedule ResourceOptimizer::computeWaveSchedule(const PathResult& path,
                                                     const CapabilityGraph& graph) {
    WaveSchedule schedule;
    if (!path.feasible || path.node_sequence.empty()) {
        schedule.feasible = false;
        return schedule;
    }

    std::unordered_map<uint32_t, size_t> node_wave_index;
    std::vector<std::vector<uint32_t>> waves;

    for (uint32_t node_id : path.node_sequence) {
        auto node_opt = graph.getNode(node_id);
        if (!node_opt.has_value()) continue;

        size_t target_wave = 0;
        const auto& current = node_opt.value();

        for (size_t i = 0; i < path.node_sequence.size(); ++i) {
            if (path.node_sequence[i] == node_id) break;
            auto earlier_opt = graph.getNode(path.node_sequence[i]);
            if (!earlier_opt.has_value()) continue;
            const auto& earlier = earlier_opt.value();
            bool dependency = false;
            for (const auto& out : earlier.profile.outputs) {
                for (const auto& in : current.profile.inputs) {
                    if (out == in) { dependency = true; break; }
                }
                if (dependency) break;
            }
            if (dependency) {
                auto it = node_wave_index.find(path.node_sequence[i]);
                if (it != node_wave_index.end()) {
                    target_wave = std::max(target_wave, it->second + 1);
                }
            }
        }

        bool placed = false;
        for (size_t w = target_wave; w < waves.size() + 1; ++w) {
            if (w >= waves.size()) waves.emplace_back();

            float wave_ram = 0.0f;
            float wave_cpu = 0.0f;
            for (uint32_t existing : waves[w]) {
                auto e_opt = graph.getNode(existing);
                if (e_opt.has_value()) {
                    wave_ram += e_opt.value().profile.avg_ram_mb;
                    wave_cpu += e_opt.value().profile.avg_cpu_percent;
                }
            }

            auto n_opt = graph.getNode(node_id);
            if (n_opt.has_value()) {
                wave_ram += n_opt.value().profile.avg_ram_mb;
                wave_cpu += n_opt.value().profile.avg_cpu_percent;
            }

            bool lock_conflict = false;
            if (n_opt.has_value()) {
                for (uint32_t existing : waves[w]) {
                    if (!canRunInParallel(existing, node_id, graph)) {
                        lock_conflict = true;
                        break;
                    }
                }
            }

            float avail_ram = getAvailableRam();
            float avail_cpu = getAvailableCpu();

            if (!lock_conflict && wave_ram <= avail_ram && wave_cpu <= avail_cpu) {
                waves[w].push_back(node_id);
                node_wave_index[node_id] = w;
                placed = true;
                break;
            }
        }

        if (!placed) {
            schedule.feasible = false;
            return schedule;
        }
    }

    schedule.waves = std::move(waves);
    schedule.feasible = true;

    for (const auto& wave : schedule.waves) {
        float ram = 0.0f;
        float cpu = 0.0f;
        for (uint32_t nid : wave) {
            auto n_opt = graph.getNode(nid);
            if (n_opt.has_value()) {
                ram += n_opt.value().profile.avg_ram_mb;
                cpu += n_opt.value().profile.avg_cpu_percent;
            }
        }
        schedule.wave_estimated_ram_mb.push_back(ram);
        schedule.wave_estimated_cpu_percent.push_back(cpu);
    }

    return schedule;
}

bool ResourceOptimizer::canRunInParallel(uint32_t, uint32_t, const CapabilityGraph&) {
    return true;
}

bool ResourceOptimizer::allocateResources(uint32_t node_id, const CapabilityGraph& graph) {
    auto node_opt = graph.getNode(node_id);
    if (!node_opt.has_value()) return false;
    const auto& prof = node_opt.value().profile;

    float avail_ram = getAvailableRam();
    float avail_cpu = getAvailableCpu();

    if (prof.avg_ram_mb > avail_ram || prof.avg_cpu_percent > avail_cpu) {
        return false;
    }

    active_allocations_[node_id] = {prof.avg_ram_mb, prof.avg_cpu_percent};
    return true;
}

void ResourceOptimizer::releaseResources(uint32_t node_id, const CapabilityGraph&) {
    active_allocations_.erase(node_id);
}

bool ResourceOptimizer::predictStarvation(const PathResult& path, const CapabilityGraph& graph) {
    float total_ram = 0.0f;
    float total_cpu = 0.0f;
    for (uint32_t nid : path.node_sequence) {
        auto n_opt = graph.getNode(nid);
        if (n_opt.has_value()) {
            total_ram += n_opt.value().profile.avg_ram_mb;
            total_cpu += n_opt.value().profile.avg_cpu_percent;
        }
    }
    return (total_ram > getAvailableRam() * 1.5f) || (total_cpu > getAvailableCpu() * 1.5f);
}

PathFinderConfig ResourceOptimizer::deriveConfigFromSystemState() {
    PathFinderConfig config;

    if (monitor_) {
        auto metrics = monitor_->sampleMetrics();
        if (metrics.ramUsedMb > 1536.0f) {
            config.w_resource = 0.50f;
            config.w_time = 0.15f;
        }
        if (metrics.cpuPercent > 70.0f) {
            config.w_resource = 0.45f;
        }
    }

    if (economy_) {
        double credits = economy_->credits();
        if (credits < 100.0) {
            config.w_monetary = 0.30f;
            config.w_time = 0.15f;
        }
    }

    return config;
}

float ResourceOptimizer::getAvailableRam() const {
    if (!monitor_) return 2048.0f;
    auto metrics = monitor_->sampleMetrics();
    return std::max(0.0f, metrics.ramTotalMb - metrics.ramUsedMb);
}

float ResourceOptimizer::getAvailableCpu() const {
    if (!monitor_) return 85.0f;
    auto metrics = monitor_->sampleMetrics();
    return std::max(0.0f, 85.0f - metrics.cpuPercent);
}
