// NeuromorphicSimulator.cpp — 3-phase barrier (integrate → route → plasticity).
#include "NeuromorphicSimulator.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <cstring>

namespace ync {

// ── Initialization ─────────────────────────────────────────────────────────

void NeuromorphicSimulator::initialize(const SimulatorConfig& cfg, uint32_t seed) {
    config = cfg;
    rng_.seed(seed);
    global_time.store(0, std::memory_order_relaxed);
    shutdown_requested.store(false, std::memory_order_relaxed);
    running.store(false, std::memory_order_relaxed);
    steps_requested_.store(0, std::memory_order_relaxed);
    steps_completed_.store(0, std::memory_order_relaxed);

    dev_engine.initialize(config.num_neurons, neurons, rng_);
    initializeConnectivity(seed);
    neuron_active_.resize(config.num_neurons, 1); // All active initially

    cores.resize(config.num_cores);
    uint32_t neurons_per_core = config.num_neurons / config.num_cores;
    for (uint32_t c = 0; c < config.num_cores; ++c) {
        auto& part          = cores[c];
        part.neuron_start   = c * neurons_per_core;
        part.neuron_end     = (c == config.num_cores - 1)
                            ? config.num_neurons
                            : (c + 1) * neurons_per_core;
        part.inbound_queues.resize(config.num_cores);
        part.delay_buffers.resize(20);
        part.start_flag.store(false,  std::memory_order_release);
        part.cycle_done.store(0,      std::memory_order_release);
        part.shutdown.store(false,    std::memory_order_release);
    }
}

void NeuromorphicSimulator::initializeConnectivity(uint32_t seed) {
    std::mt19937 rng(seed + 1);
    std::uniform_int_distribution<uint32_t> target_dist(0, config.num_neurons - 1);
    std::uniform_int_distribution<int>      delay_dist(1, 19);
    std::uniform_real_distribution<float>   weight_dist(0.01f, 0.1f);

    uint32_t max_synapses =
        static_cast<uint32_t>(config.num_neurons * config.connectivity_density);

    for (uint32_t i = 0; i < config.num_neurons; ++i) {
        uint32_t n_syn = std::min(max_synapses, config.num_neurons / 10u);
        for (uint32_t s = 0; s < n_syn; ++s) {
            uint32_t target = target_dist(rng);
            if (target == i) continue;
            bool exists = false;
            for (const auto& axon : neurons[i].axons)
                if (axon.target_id == target) { exists = true; break; }
            if (!exists) {
                AxonTerminal axon;
                axon.target_id = target;
                axon.weight    = weight_dist(rng) * neurons[i].receptor.excitatory_bias;
                axon.delay     = static_cast<uint8_t>(delay_dist(rng));
                axon.last_pre_spike = 0;
                axon.eligibility_trace.store(0.0f, std::memory_order_relaxed);
                neurons[i].axons.push_back(std::move(axon));
                if (target < neurons.size())
                    neurons[target].dendrite_sources.push_back(i);
            }
        }
    }
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void NeuromorphicSimulator::start() {
    if (running.load(std::memory_order_acquire)) return;
    shutdown_requested.store(false, std::memory_order_release);
    running.store(true, std::memory_order_release);
    for (uint32_t c = 0; c < config.num_cores; ++c)
        cores[c].worker = std::thread(&NeuromorphicSimulator::workerLoop, this, c);
}

void NeuromorphicSimulator::stop() {
    shutdown_requested.store(true, std::memory_order_release);
    for (auto& part : cores) part.start_flag.store(true, std::memory_order_release);
    for (auto& part : cores)
        if (part.worker.joinable()) part.worker.join();
    running.store(false, std::memory_order_release);
}

// ── Step / RunFor ──────────────────────────────────────────────────────────

void NeuromorphicSimulator::step() {
    if (!running.load(std::memory_order_acquire)) {
        // Single-threaded fallback: run one cycle synchronously on main thread.
        uint64_t now = global_time.load(std::memory_order_relaxed);
        float dopamine      = nms.dopamine.load(std::memory_order_relaxed);
        float noradrenaline = nms.noradrenaline.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < static_cast<uint32_t>(neurons.size()); ++i) {
            if (!neurons[i].isAlive()) continue;
            neurons[i].integrate(0.0f, now, dopamine, noradrenaline);
        }
        nms.decay();
        global_time.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // Threaded mode: request one step and wait for completion.
    uint64_t target = steps_completed_.load(std::memory_order_relaxed) + 1;
    steps_requested_.fetch_add(1, std::memory_order_release);
    // Wake all cores
    for (auto& part : cores) part.start_flag.store(true, std::memory_order_release);
    // Wait for all cores to finish this step
    while (steps_completed_.load(std::memory_order_acquire) < target)
        std::this_thread::yield();
}

void NeuromorphicSimulator::runFor(uint32_t steps) {
    for (uint32_t i = 0; i < steps; ++i) step();
}

// ── Worker Loop (3-phase barrier) ──────────────────────────────────────────

// ── Worker Loop (counted-trigger, 3-phase barrier per cycle) ─────────────

// ── Sparse Activation Tracking ──────────────────────────────────────────────

void NeuromorphicSimulator::updateActivityMask(uint64_t now) {
    uint32_t n_neurons = static_cast<uint32_t>(neurons.size());
    if (neuron_active_.size() < n_neurons) {
        neuron_active_.resize(n_neurons, 0);
    }
    for (uint32_t i = 0; i < n_neurons; ++i) {
        uint64_t last_spike = neurons[i].last_spike_time.load(std::memory_order_acquire);
        if (last_spike > 0 && (now - last_spike) < ACTIVITY_WINDOW_MS) {
            neuron_active_[i] = 1;
        } else {
            neuron_active_[i] = 0;
        }
    }
}

bool NeuromorphicSimulator::isNeuronActive(uint32_t neuron_id, uint64_t now) const {
    if (neuron_id < neuron_active_.size() && neuron_active_[neuron_id]) return true;
    // Also active if any presynaptic neighbor fired recently
    if (neuron_id < neurons.size()) {
        for (uint32_t src : neurons[neuron_id].dendrite_sources) {
            if (src < neurons.size()) {
                uint64_t last_spike = neurons[src].last_spike_time.load(std::memory_order_acquire);
                if (last_spike > 0 && (now - last_spike) < ACTIVITY_WINDOW_MS) {
                    return true;
                }
            }
        }
    }
    return false;
}

void NeuromorphicSimulator::workerLoop(uint32_t core_id) {
    auto& part = cores[core_id];
    static constexpr uint32_t SPIN_LIMIT = 50000;

    while (!shutdown_requested.load(std::memory_order_acquire)) {
        // Wait for a step to be requested (start_flag is used as a wake hint)
        uint32_t spin = 0;
        while (!part.start_flag.load(std::memory_order_acquire) &&
               !shutdown_requested.load(std::memory_order_acquire)) {
            if (++spin > SPIN_LIMIT) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                spin = 0;
            } else {
                std::this_thread::yield();
            }
        }
        if (shutdown_requested.load(std::memory_order_acquire)) break;
        part.start_flag.store(false, std::memory_order_release);

        uint64_t now        = global_time.load(std::memory_order_relaxed);
        // ACQUIRE: see main thread updates via deliverReward/Punishment/etc.
        float dopamine      = nms.dopamine.load(std::memory_order_acquire);
        float noradrenaline = nms.noradrenaline.load(std::memory_order_acquire);

        // ── Phase 1: INTEGRATE ──────────────────────────────────────────
        for (uint32_t i = part.neuron_start; i < part.neuron_end; ++i) {
            if (!neurons[i].isAlive()) continue;

            // Sparse skip: if neuron inactive and no presynaptic neighbor active, skip
            if (!isNeuronActive(i, now)) {
                // Still decay membrane potential
                float v = neurons[i].v_membrane.load(std::memory_order_relaxed);
                v -= v / Neuron::TAU_MEMBRANE;
                neurons[i].v_membrane.store(v, std::memory_order_relaxed);
                continue;
            }

            float input_current = 0.0f;
            size_t buf_idx = static_cast<size_t>(now % 20);
            for (const auto& spike : part.delay_buffers[buf_idx]) {
                if (spike.target == i) input_current += spike.weight;
            }
            part.delay_buffers[buf_idx].clear();
            bool fired = neurons[i].integrate(input_current, now, dopamine, noradrenaline);
            if (fired) {
                for (const auto& axon : neurons[i].axons) {
                    uint32_t target_core = (config.num_cores > 1)
                        ? std::min(axon.target_id / (config.num_neurons / config.num_cores),
                                   config.num_cores - 1)
                        : 0;
                    SpikePacket sp{i, axon.target_id, axon.weight, axon.delay};
                    uint32_t spin_push = 0;
                    while (!part.inbound_queues[target_core].push(sp)) {
                        if (++spin_push > 10000) break; // drop spike if queue full
                        std::this_thread::yield();
                    }
                }
            }
        }

        // Barrier 1: all cores must finish integrate before routing
        part.cycle_done.fetch_add(1, std::memory_order_release);
        uint64_t barrier1 = part.cycle_done.load(std::memory_order_relaxed);
        for (const auto& other : cores) {
            while (other.cycle_done.load(std::memory_order_acquire) < barrier1) {
                if (shutdown_requested.load(std::memory_order_acquire)) break;
                std::this_thread::yield();
            }
        }

        // ── Phase 2: ROUTE SPIKES ──────────────────────────────────────
        for (uint32_t src = 0; src < config.num_cores; ++src) {
            SpikePacket sp;
            while (part.inbound_queues[src].pop(sp)) {
                size_t buf_idx = static_cast<size_t>((now + sp.delay) % 20);
                part.delay_buffers[buf_idx].push_back(
                    DelayedSpike{sp.target, sp.weight, now + sp.delay});
            }
        }

        // Barrier 2: all cores must finish routing before plasticity
        part.cycle_done.fetch_add(1, std::memory_order_release);
        uint64_t barrier2 = part.cycle_done.load(std::memory_order_relaxed);
        for (const auto& other : cores) {
            while (other.cycle_done.load(std::memory_order_acquire) < barrier2) {
                if (shutdown_requested.load(std::memory_order_acquire)) break;
                std::this_thread::yield();
            }
        }

        // ── Phase 3: PLASTICITY (every 10 cycles) ──────────────────────
        if (now % 10 == 0) applyPlasticity(core_id, now);

        // ── Phase 4: DEVELOPMENT (core 0 only, every 1000 cycles) ──────
        if (core_id == 0 && now % 1000 == 0)
            dev_engine.tick(1, neurons, nms, rng_);

        // Update sparse activity mask every 10ms (core 0 only to avoid data races)
        if (core_id == 0 && now % 10 == 0) {
            updateActivityMask(now);
        }

        // Final barrier: all done, then core 0 advances global_time
        part.cycle_done.fetch_add(1, std::memory_order_release);
        uint64_t barrier3 = part.cycle_done.load(std::memory_order_relaxed);
        for (const auto& other : cores) {
            while (other.cycle_done.load(std::memory_order_acquire) < barrier3) {
                if (shutdown_requested.load(std::memory_order_acquire)) break;
                std::this_thread::yield();
            }
        }

        // Core 0 does global bookkeeping
        if (core_id == 0) {
            nms.decay();
            global_time.fetch_add(1, std::memory_order_relaxed);
            steps_completed_.fetch_add(1, std::memory_order_release);
        }
    }
}

void NeuromorphicSimulator::applyPlasticity(uint32_t core_id, uint64_t now) {
    auto& part = cores[core_id];
    // ACQUIRE: main thread may have just updated NMS via deliverReward/Punishment
    float dopamine = nms.dopamine.load(std::memory_order_acquire);
    auto  params   = DevelopmentalEngine::paramsForStage(dev_engine.stage);
    for (uint32_t i = part.neuron_start; i < part.neuron_end; ++i) {
        neurons[i].applySTDP(now, params.learning_rate, dopamine, neurons);
        neurons[i].homeostaticScale(Neuron::TARGET_FIRING_RATE);
    }
}

// ── I/O ────────────────────────────────────────────────────────────────────

void NeuromorphicSimulator::injectSensory(const std::vector<float>& sensory_vector) {
    size_t n = std::min(sensory_vector.size(),
                        static_cast<size_t>(config.sensory_neurons));
    for (size_t i = 0; i < n; ++i)
        neurons[i].v_membrane.store(sensory_vector[i] * 2.0f, std::memory_order_relaxed);
}

YNCOutput NeuromorphicSimulator::readMotor() const {
    YNCOutput out;
    out.motor_activations.resize(config.motor_neurons);
    uint32_t motor_start = config.num_neurons - config.motor_neurons;
    for (uint32_t i = 0; i < config.motor_neurons; ++i) {
        float v = neurons[motor_start + i].v_membrane.load(std::memory_order_relaxed);
        out.motor_activations[i] = std::clamp(v / 2.0f, 0.0f, 1.0f);
    }
    out.global_valence = (nms.dopamine.load(std::memory_order_relaxed) - 0.5f) * 2.0f;
    out.global_arousal = nms.noradrenaline.load(std::memory_order_relaxed);
    out.simulated_ms   = global_time.load(std::memory_order_relaxed);

    float sum = 0.0f;
    for (float a : out.motor_activations) sum += a;
    if (sum > 1e-6f) {
        float entropy = 0.0f;
        for (float a : out.motor_activations) {
            float p = a / sum;
            if (p > 1e-6f) entropy -= p * std::log2(p);
        }
        float max_entropy = std::log2(static_cast<float>(config.motor_neurons));
        out.uncertainty   = entropy / max_entropy;
    } else {
        out.uncertainty = 1.0f;
    }
    return out;
}

void NeuromorphicSimulator::deliverReward(float magnitude)       { nms.onReward(magnitude); }
void NeuromorphicSimulator::deliverPunishment(float magnitude)   { nms.onPunishment(magnitude); }
void NeuromorphicSimulator::deliverSurprise(float free_energy)   { nms.onSurprise(free_energy); }

// ── Checkpoint ─────────────────────────────────────────────────────────────

void NeuromorphicSimulator::saveCheckpoint(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return;

    static constexpr uint32_t kMagic   = 0x594E434B;
    static constexpr uint16_t kVersion = 1;
    ofs.write(reinterpret_cast<const char*>(&kMagic),           sizeof(kMagic));
    ofs.write(reinterpret_cast<const char*>(&kVersion),         sizeof(kVersion));
    ofs.write(reinterpret_cast<const char*>(&config.num_neurons), sizeof(config.num_neurons));

    uint64_t total_synapses = 0;
    for (const auto& n : neurons) total_synapses += n.axons.size();
    ofs.write(reinterpret_cast<const char*>(&total_synapses), sizeof(total_synapses));

    uint64_t t = global_time.load(std::memory_order_relaxed);
    ofs.write(reinterpret_cast<const char*>(&t),                sizeof(t));
    ofs.write(reinterpret_cast<const char*>(&dev_engine.stage), sizeof(dev_engine.stage));
    ofs.write(reinterpret_cast<const char*>(&dev_engine.stage_progress),
              sizeof(dev_engine.stage_progress));

    for (const auto& n : neurons) {
        ofs.write(reinterpret_cast<const char*>(&n.id), sizeof(n.id));
        float v      = n.v_membrane.load(std::memory_order_relaxed);
        float thresh = n.threshold.load(std::memory_order_relaxed);
        float adapt  = n.adaptation.load(std::memory_order_relaxed);
        uint64_t ls  = n.last_spike_time.load(std::memory_order_relaxed);
        float en     = n.energy.load(std::memory_order_relaxed);
        float ca     = n.calcium.load(std::memory_order_relaxed);
        float rate   = n.firing_rate_ema.load(std::memory_order_relaxed);
        ofs.write(reinterpret_cast<const char*>(&v),     sizeof(v));
        ofs.write(reinterpret_cast<const char*>(&thresh),sizeof(thresh));
        ofs.write(reinterpret_cast<const char*>(&adapt), sizeof(adapt));
        ofs.write(reinterpret_cast<const char*>(&ls),    sizeof(ls));
        ofs.write(reinterpret_cast<const char*>(&en),    sizeof(en));
        ofs.write(reinterpret_cast<const char*>(&ca),    sizeof(ca));
        ofs.write(reinterpret_cast<const char*>(&rate),  sizeof(rate));

        uint32_t axon_count = static_cast<uint32_t>(n.axons.size());
        ofs.write(reinterpret_cast<const char*>(&axon_count), sizeof(axon_count));
        for (const auto& axon : n.axons) {
            ofs.write(reinterpret_cast<const char*>(&axon.target_id), sizeof(axon.target_id));
            ofs.write(reinterpret_cast<const char*>(&axon.weight),    sizeof(axon.weight));
            ofs.write(reinterpret_cast<const char*>(&axon.delay),     sizeof(axon.delay));
        }
    }

    size_t offset = 0;
    uint8_t nms_buf[32];
    nms.serialize(nms_buf, offset);
    ofs.write(reinterpret_cast<const char*>(nms_buf), static_cast<std::streamsize>(offset));
}

bool NeuromorphicSimulator::loadCheckpoint(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    static constexpr uint32_t kMagic   = 0x594E434B;
    static constexpr uint16_t kVersion = 1;

    uint32_t magic; uint16_t version;
    ifs.read(reinterpret_cast<char*>(&magic),   sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (magic != kMagic || version != kVersion) return false;

    uint32_t num_neurons;
    ifs.read(reinterpret_cast<char*>(&num_neurons), sizeof(num_neurons));
    if (num_neurons != config.num_neurons) return false;

    uint64_t total_synapses;
    ifs.read(reinterpret_cast<char*>(&total_synapses), sizeof(total_synapses));

    uint64_t t;
    ifs.read(reinterpret_cast<char*>(&t), sizeof(t));
    global_time.store(t, std::memory_order_relaxed);

    ifs.read(reinterpret_cast<char*>(&dev_engine.stage),
             sizeof(dev_engine.stage));
    ifs.read(reinterpret_cast<char*>(&dev_engine.stage_progress),
             sizeof(dev_engine.stage_progress));

    for (auto& n : neurons) {
        ifs.read(reinterpret_cast<char*>(&n.id), sizeof(n.id));
        float v, thresh, adapt, en, ca, rate; uint64_t ls;
        ifs.read(reinterpret_cast<char*>(&v),     sizeof(v));
        ifs.read(reinterpret_cast<char*>(&thresh),sizeof(thresh));
        ifs.read(reinterpret_cast<char*>(&adapt), sizeof(adapt));
        ifs.read(reinterpret_cast<char*>(&ls),    sizeof(ls));
        ifs.read(reinterpret_cast<char*>(&en),    sizeof(en));
        ifs.read(reinterpret_cast<char*>(&ca),    sizeof(ca));
        ifs.read(reinterpret_cast<char*>(&rate),  sizeof(rate));
        n.v_membrane.store(v,      std::memory_order_relaxed);
        n.threshold.store(thresh,  std::memory_order_relaxed);
        n.adaptation.store(adapt,  std::memory_order_relaxed);
        n.last_spike_time.store(ls,std::memory_order_relaxed);
        n.energy.store(en,         std::memory_order_relaxed);
        n.calcium.store(ca,        std::memory_order_relaxed);
        n.firing_rate_ema.store(rate, std::memory_order_relaxed);

        uint32_t axon_count;
        ifs.read(reinterpret_cast<char*>(&axon_count), sizeof(axon_count));
        n.axons.resize(axon_count);
        for (auto& axon : n.axons) {
            ifs.read(reinterpret_cast<char*>(&axon.target_id), sizeof(axon.target_id));
            ifs.read(reinterpret_cast<char*>(&axon.weight),    sizeof(axon.weight));
            ifs.read(reinterpret_cast<char*>(&axon.delay),     sizeof(axon.delay));
            axon.last_pre_spike = 0;
            axon.eligibility_trace.store(0.0f, std::memory_order_relaxed);
        }
    }

    size_t offset = 0;
    uint8_t nms_buf[32];
    ifs.read(reinterpret_cast<char*>(nms_buf), 16);
    nms.deserialize(nms_buf, offset);
    return true;
}

} // namespace ync
