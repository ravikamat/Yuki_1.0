#pragma once
#include "CoreBus.h"
#include "../brain/memory/NeuralPopulation.h"  // PACL Phase 4: CognitiveMoment types
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace yuki {
namespace gw {

struct Coalition {
    std::string module_id;
    Topic topic = Topic::NONE;
    float salience = 0.0f;
    Message message;
};

// ── PACL Phase 4: CognitiveMoment — bound snapshot of workspace state ─────────
// Produced by bind(NeuralWorkspace) and readable via peek().
// Completely separate from existing Coalition/broadcast machinery.
struct ModuleContribution {
    std::string     module_id;
    float           salience  = 0.0f;
    int64_t         concept_id = -1;
    float           firing_rate = 0.0f;
};

struct CognitiveMoment {
    uint64_t                         moment_id     = 0;
    memory::Hypervector              semantic_binding;     // XOR of all active population consensus vectors
    float                            emotional_valence = 0.0f;  // [-1, +1]
    float                            arousal           = 0.0f;  // [0, 1]
    float                            uncertainty       = 0.0f;  // normalized entropy [0, 1]
    std::vector<ModuleContribution>  contributors;
    std::chrono::steady_clock::time_point timestamp;
    bool                             valid = false;
};

class GlobalWorkspace {
public:
    static GlobalWorkspace& instance();

    void init(float threshold = 0.25f, uint32_t broadcast_interval_ms = 10);

    // Modules submit salience-competing messages here instead of direct CoreBus publish
    void compete(const Coalition& coalition);

    // Background thread: every 10ms picks winner and broadcasts to CoreBus
    void start();
    void stop();

    // Current winning coalition (for introspection)
    Coalition currentWinner() const;

    // ── PACL Phase 4: CognitiveMoment binding ────────────────────────────────
    // Produces a CognitiveMoment from NeuralWorkspace state.
    // Called by PACL cortex modules — does NOT touch existing broadcast flow.
    CognitiveMoment bind(const memory::NeuralWorkspace& workspace,
                         float emotional_valence = 0.0f,
                         float arousal           = 0.0f);

    // Non-destructive read of the last bound CognitiveMoment.
    // Returns invalid (valid == false) if bind() has never been called.
    CognitiveMoment peek() const;

private:
    GlobalWorkspace() = default;
    void broadcastLoop();

    std::vector<Coalition> buffer_;
    mutable std::mutex buf_mtx_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    float threshold_ = 0.25f;
    uint32_t interval_ms_ = 10;
    Coalition winner_;
    mutable std::mutex winner_mtx_;

    // ── PACL Phase 4: last bound moment ──────────────────────────────────────
    CognitiveMoment         last_moment_;
    mutable std::mutex      moment_mtx_;
    std::atomic<uint64_t>   moment_counter_{0};
};

} // namespace gw
} // namespace yuki
