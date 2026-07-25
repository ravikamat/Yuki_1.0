// PerceptionCorticalModule.h — Parallel percept encoder (PACL Phase 6)
// Encodes perceptual stimuli into NeuralWorkspace populations.
// Runs independently from the 19-stage TurnCoordinator pipeline.
// Rule §18.1: No std::cout/printf.
// Rule §18.4: All constants are constexpr.
#pragma once
#include "brain/memory/NeuralPopulation.h"
#include "brain/memory/Hypervector.h"
#include "infrastructure/NeuralCoreBus.h"
#include <string>
#include <vector>
#include <cstdint>

namespace yuki {
namespace cortex {

// Strength of the initial excitation when encoding a new percept.
constexpr float kPerceptExciteStrength = 0.7f;

// ── PerceptionCorticalModule ──────────────────────────────────────────────────
// Encodes text/audio/visual percepts into NeuralWorkspace populations.
// Thread-safe: activate() delegates to NeuralWorkspace::activate() which
// uses internal locking.
class PerceptionCorticalModule {
public:
    explicit PerceptionCorticalModule(memory::NeuralWorkspace& workspace,
                                      gw::NeuralCoreBus*       bus = nullptr)
        : workspace_(workspace)
        , bus_(bus)
    {}

    // Encode a text percept: creates a seeded hypervector, activates concept.
    // concept_id is derived from a simple hash of the text.
    void encode(const std::string& text, float strength = kPerceptExciteStrength) {
        int64_t concept_id = stableHash(text);
        memory::Hypervector evidence(text);  // text-seeded HV constructor
        workspace_.activate(concept_id, evidence, strength);

        if (bus_) {
            gw::NeuralEvent ev;
            ev.type       = gw::NeuralEventType::ACTIVATION;
            ev.source     = gw::NeuralModuleId::PERCEPTION;
            ev.concept_id = concept_id;
            ev.strength   = strength;
            bus_->tryBroadcast(ev);
        }
    }

    // Encode from a pre-formed hypervector (for visual/audio feature vectors).
    void encodeVector(int64_t concept_id, const memory::Hypervector& hv,
                      float strength = kPerceptExciteStrength) {
        workspace_.activate(concept_id, hv, strength);

        if (bus_) {
            gw::NeuralEvent ev;
            ev.type       = gw::NeuralEventType::ACTIVATION;
            ev.source     = gw::NeuralModuleId::PERCEPTION;
            ev.concept_id = concept_id;
            ev.strength   = strength;
            bus_->tryBroadcast(ev);
        }
    }

private:
    static int64_t stableHash(const std::string& s) {
        // FNV-1a 64-bit hash (same basis as HdcSemanticGraph)
        constexpr uint64_t FNV_PRIME  = 0x00000100000001B3ULL;
        constexpr uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
        uint64_t h = FNV_OFFSET;
        for (unsigned char c : s) {
            h ^= static_cast<uint64_t>(c);
            h *= FNV_PRIME;
        }
        return static_cast<int64_t>(h);
    }

    memory::NeuralWorkspace& workspace_;
    gw::NeuralCoreBus*       bus_;
};

} // namespace cortex
} // namespace yuki
