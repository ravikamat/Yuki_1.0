// NeuromorphicSimulator.h -- multi-core LIF network with 3-phase SPSC-barrier.
#pragma once
#include "Neuron.h"
#include "NeuromodulatorState.h"
#include "DevelopmentalEngine.h"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <cstdint>
#include <string>

namespace ync {

struct alignas(64) SpikePacket {
    uint32_t source;
    uint32_t target;
    float    weight;
    uint8_t  delay;
};

struct DelayedSpike {
    uint32_t target;
    float    weight;
    uint64_t arrival_time;
};

template<typename T, size_t Capacity>
class alignas(64) SPSCQueue {
    std::vector<T>                    buffer_;
    alignas(64) std::atomic<size_t>   write_idx_{0};
    alignas(64) std::atomic<size_t>   read_idx_{0};
public:
    SPSCQueue() : buffer_(Capacity) {}

    // Explicit move ctor -- atomics cannot be copy/move constructed by default.
    SPSCQueue(SPSCQueue&& o) noexcept
        : buffer_(std::move(o.buffer_)) {
        write_idx_.store(o.write_idx_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        read_idx_.store(o.read_idx_.load(std::memory_order_relaxed),  std::memory_order_relaxed);
    }
    SPSCQueue& operator=(SPSCQueue&& o) noexcept {
        buffer_    = std::move(o.buffer_);
        write_idx_.store(o.write_idx_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        read_idx_.store(o.read_idx_.load(std::memory_order_relaxed),  std::memory_order_relaxed);
        return *this;
    }
    SPSCQueue(const SPSCQueue&)            = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool push(const T& item) {
        size_t w    = write_idx_.load(std::memory_order_relaxed);
        size_t next = (w + 1) % Capacity;
        if (next == read_idx_.load(std::memory_order_acquire)) return false;
        buffer_[w] = item;
        write_idx_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        if (r == write_idx_.load(std::memory_order_acquire)) return false;
        item = buffer_[r];
        read_idx_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return read_idx_.load(std::memory_order_relaxed) ==
               write_idx_.load(std::memory_order_relaxed);
    }
};

struct CorePartition {
    uint32_t neuron_start = 0;
    uint32_t neuron_end   = 0;
    std::thread worker;

    static constexpr size_t SPIKE_QUEUE_SIZE = 65536;
    std::vector<SPSCQueue<SpikePacket, SPIKE_QUEUE_SIZE>> inbound_queues;
    std::vector<std::vector<DelayedSpike>> delay_buffers;

    alignas(64) std::atomic<bool>     start_flag{false};
    alignas(64) std::atomic<uint64_t> cycle_done{0};
    alignas(64) std::atomic<bool>     shutdown{false};

    // Default ctor needed for vector<CorePartition>
    CorePartition() = default;

    // Explicit move ctor -- thread, atomics, and vector<SPSCQueue> are move-only.
    CorePartition(CorePartition&& o) noexcept
        : neuron_start(o.neuron_start),
          neuron_end(o.neuron_end),
          worker(std::move(o.worker)),
          inbound_queues(std::move(o.inbound_queues)),
          delay_buffers(std::move(o.delay_buffers)) {
        start_flag.store(o.start_flag.load(std::memory_order_relaxed), std::memory_order_relaxed);
        cycle_done.store(o.cycle_done.load(std::memory_order_relaxed), std::memory_order_relaxed);
        shutdown.store(o.shutdown.load(std::memory_order_relaxed),    std::memory_order_relaxed);
    }
    CorePartition& operator=(CorePartition&& o) noexcept {
        neuron_start   = o.neuron_start;
        neuron_end     = o.neuron_end;
        worker         = std::move(o.worker);
        inbound_queues = std::move(o.inbound_queues);
        delay_buffers  = std::move(o.delay_buffers);
        start_flag.store(o.start_flag.load(std::memory_order_relaxed), std::memory_order_relaxed);
        cycle_done.store(o.cycle_done.load(std::memory_order_relaxed), std::memory_order_relaxed);
        shutdown.store(o.shutdown.load(std::memory_order_relaxed),    std::memory_order_relaxed);
        return *this;
    }
    CorePartition(const CorePartition&)            = delete;
    CorePartition& operator=(const CorePartition&) = delete;
};

struct SimulatorConfig {
    uint32_t num_neurons          = 10000;
    uint32_t num_cores            = 4;
    uint32_t timestep_ms          = 1;
    uint32_t sensory_neurons      = 128;
    uint32_t motor_neurons        = 16;
    float    connectivity_density = 0.05f;
};

struct YNCOutput {
    std::vector<float> motor_activations;
    float    global_valence = 0.0f;
    float    global_arousal = 0.0f;
    float    uncertainty    = 1.0f;
    uint64_t simulated_ms   = 0;
};

class NeuromorphicSimulator {
public:
    SimulatorConfig     config;
    std::vector<Neuron> neurons;
    std::vector<CorePartition> cores;
    NeuromodulatorState nms;
    DevelopmentalEngine dev_engine;

    alignas(64) std::atomic<uint64_t> global_time{0};
    alignas(64) std::atomic<bool>     running{false};
    alignas(64) std::atomic<bool>     shutdown_requested{false};

    void initialize(const SimulatorConfig& cfg, uint32_t seed = 42);
    void start();
    void stop();
    void step();
    void runFor(uint32_t steps);

    void injectSensory(const std::vector<float>& sensory_vector);
    YNCOutput readMotor() const;

    void deliverReward(float magnitude);
    void deliverPunishment(float magnitude);
    void deliverSurprise(float free_energy);

    void saveCheckpoint(const std::string& path) const;
    bool loadCheckpoint(const std::string& path);

    // Sparse activation tracking
    std::vector<uint8_t> neuron_active_; // 1 = active (fired in last 100ms), 0 = inactive
    static constexpr uint32_t ACTIVITY_WINDOW_MS = 100;

    void updateActivityMask(uint64_t now);
    bool isNeuronActive(uint32_t neuron_id, uint64_t now) const;

private:
    std::mt19937 rng_;
    alignas(64) std::atomic<uint64_t> steps_requested_{0};
    alignas(64) std::atomic<uint64_t> steps_completed_{0};
    void workerLoop(uint32_t core_id);
    void routeSpikes(uint32_t core_id, uint64_t now);
    void applyPlasticity(uint32_t core_id, uint64_t now);
    void initializeConnectivity(uint32_t seed);
};

} // namespace ync
