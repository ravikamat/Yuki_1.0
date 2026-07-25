// MemoryCorticalModule.h — Parallel memory retriever cortex module (PACL Phase 6)
// Retrieves memories and activates matching concept populations in NeuralWorkspace.
// Rule §18.1: No std::cout/printf.
// Rule §18.4: All constants are constexpr.
#pragma once
#include "brain/memory/NeuralPopulation.h"
#include "brain/memory/ParallelMemoryFabric.h"
#include "infrastructure/NeuralCoreBus.h"
#include <string>
#include <cstdint>

namespace yuki {
namespace cortex {

// Minimum confidence for a retrieved memory item to activate a population.
constexpr float kMemoryExciteMinConfidence = 0.3f;

// Excitation strength for retrieved memories (scaled by item confidence).
constexpr float kMemoryExciteBase = 0.5f;

// ── MemoryCorticalModule ──────────────────────────────────────────────────────
// Retrieves from T0–T4 in parallel and activates matching NeuralWorkspace pops.
class MemoryCorticalModule {
public:
    explicit MemoryCorticalModule(memory::NeuralWorkspace&    workspace,
                                  memory::ParallelMemoryFabric& pmf,
                                  gw::NeuralCoreBus*          bus = nullptr)
        : workspace_(workspace)
        , pmf_(pmf)
        , bus_(bus)
    {}

    // Query memory and excite matching concept populations.
    // Returns the number of items that excited the workspace.
    size_t retrieve(const std::string& query,
                    memory::RetrieveMode mode = memory::RetrieveMode::SEMANTIC)
    {
        auto pack = pmf_.retrieveParallel(query, mode);
        size_t activated = 0;

        for (const auto& item : pack.merged) {
            if (item.confidence < kMemoryExciteMinConfidence) continue;

            // Derive concept_id from item key hash
            int64_t concept_id = keyHash(item.key);

            // Evidence hypervector seeded from the key string
            memory::Hypervector evidence(item.key);
            float strength = kMemoryExciteBase * item.confidence;
            workspace_.activate(concept_id, evidence, strength);
            ++activated;

            if (bus_) {
                gw::NeuralEvent ev;
                ev.type       = gw::NeuralEventType::ACTIVATION;
                ev.source     = gw::NeuralModuleId::MEMORY;
                ev.concept_id = concept_id;
                ev.strength   = strength;
                bus_->tryPush(gw::NeuralModuleId::GLOBAL_WS, ev);
            }
        }
        return activated;
    }

private:
    static int64_t keyHash(const std::string& s) {
        constexpr uint64_t FNV_PRIME  = 0x00000100000001B3ULL;
        constexpr uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
        uint64_t h = FNV_OFFSET;
        for (unsigned char c : s) { h ^= static_cast<uint64_t>(c); h *= FNV_PRIME; }
        return static_cast<int64_t>(h);
    }

    memory::NeuralWorkspace&     workspace_;
    memory::ParallelMemoryFabric& pmf_;
    gw::NeuralCoreBus*           bus_;
};

} // namespace cortex
} // namespace yuki
