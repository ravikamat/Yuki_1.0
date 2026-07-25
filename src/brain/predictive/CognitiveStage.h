#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <cassert>

namespace yuki::predictive {

enum class StageId : uint8_t {
    S1_BOOT_PROBE = 0,
    S2_SIGNAL_CONDITIONING,
    S3_TEMPORAL_ALIGNMENT,
    S4_SALIENCE_GATING,
    S5_MODALITY_ENCODING,
    S6_MULTIMODAL_FUSION,
    S7_ACTIVE_INFERENCE_BELIEF,
    S8_PRECISION_PREDICTION,
    S9_WORKING_MEMORY_BROADCAST,
    S10_PERCEPTUAL_CATEGORIZATION,
    S11_MEMORY_RETRIEVAL,
    S12_FREE_ENERGY_MINIMIZATION,
    S13_CHAIN_RECONSTRUCTION,
    S14_POLICY_SELECTION,
    S15_INTROSPECTION_PROFILING,
    S16_DECISION_SUBSTRATE,
    S17_RESPONSE_SYNTHESIS,
    S18_APPROVAL_GATE,
    S19_METACOGNITIVE_ROUTING
};

struct StageConfig {
    StageId id;
    const char* internalName;
    size_t timeoutMs;
    uint32_t requiredInputMask;
    uint32_t outputMask;
};

class StageRegistry {
public:
    static const StageConfig& configFor(StageId id) {
        static constexpr std::array<StageConfig, 19> kStageTable = {{
            { StageId::S1_BOOT_PROBE, "BOOT_PROBE", 100, 0x0, 0x1 },
            { StageId::S2_SIGNAL_CONDITIONING, "SIGNAL_CONDITIONING", 100, 0x1, 0x3 },
            { StageId::S3_TEMPORAL_ALIGNMENT, "TEMPORAL_ALIGNMENT", 100, 0x3, 0x7 },
            { StageId::S4_SALIENCE_GATING, "SALIENCE_GATING", 100, 0x7, 0xF },
            { StageId::S5_MODALITY_ENCODING, "MODALITY_ENCODING", 200, 0xF, 0x1F },
            { StageId::S6_MULTIMODAL_FUSION, "MULTIMODAL_FUSION", 200, 0x1F, 0x3F },
            { StageId::S7_ACTIVE_INFERENCE_BELIEF, "ACTIVE_INFERENCE_BELIEF", 300, 0x3F, 0x7F },
            { StageId::S8_PRECISION_PREDICTION, "PRECISION_PREDICTION", 200, 0x7F, 0xFF },
            { StageId::S9_WORKING_MEMORY_BROADCAST, "WORKING_MEMORY_BROADCAST", 100, 0xFF, 0x1FF },
            { StageId::S10_PERCEPTUAL_CATEGORIZATION, "PERCEPTUAL_CATEGORIZATION", 200, 0x1FF, 0x3FF },
            { StageId::S11_MEMORY_RETRIEVAL, "MEMORY_RETRIEVAL", 300, 0x3FF, 0x7FF },
            { StageId::S12_FREE_ENERGY_MINIMIZATION, "FREE_ENERGY_MINIMIZATION", 200, 0x7FF, 0xFFF },
            { StageId::S13_CHAIN_RECONSTRUCTION, "CHAIN_RECONSTRUCTION", 300, 0xFFF, 0x1FFF },
            { StageId::S14_POLICY_SELECTION, "POLICY_SELECTION", 200, 0x1FFF, 0x3FFF },
            { StageId::S15_INTROSPECTION_PROFILING, "INTROSPECTION_PROFILING", 100, 0x3FFF, 0x7FFF },
            { StageId::S16_DECISION_SUBSTRATE, "DECISION_SUBSTRATE", 200, 0x7FFF, 0xFFFF },
            { StageId::S17_RESPONSE_SYNTHESIS, "RESPONSE_SYNTHESIS", 500, 0xFFFF, 0x1FFFF },
            { StageId::S18_APPROVAL_GATE, "APPROVAL_GATE", 200, 0x1FFFF, 0x3FFFF },
            { StageId::S19_METACOGNITIVE_ROUTING, "METACOGNITIVE_ROUTING", 200, 0x3FFFF, 0x7FFFF }
        }};
        static_assert(kStageTable.size() == 19, "Stage table must contain exactly 19 stages");
        size_t idx = static_cast<size_t>(id);
        assert(idx < 19);
        return kStageTable[idx];
    }
};

} // namespace yuki::predictive
