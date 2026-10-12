#pragma once
#include <array>
#include <string>
#include <vector>
#include <memory>

// Forward declarations
namespace yuki { namespace swarm { class IProceduralStore; } }

#include "brain/memory/HeuristicMemory.h"

namespace yuki {
namespace memory {

// Encodes 8 heuristic text scores + source tag into a dense vector
// for HNSWLib indexing. Output dimension = 24 to align with VSE BeliefState.
//
// Amplification factors (default 2.0 for all 8 channels) are learnable
// via IProceduralStore when a store is provided at construction.
// Without a store, behaviour is identical to the original struct.
class MemoryEncoder {
public:
    static constexpr size_t OUTPUT_DIM = 24;
    static constexpr float  DEFAULT_AMP = 2.0f;

    /// Default constructor — uses fixed 2.0f amplification (original behaviour).
    MemoryEncoder();

    /// Explicit destructor declared here, defined in MemoryEncoder.cpp where
    /// ProceduralHeuristicAccess is fully defined (via HeuristicMemory.h).
    /// Required because unique_ptr<forward-declared type> instantiates
    /// default_delete<T> which calls ~T — and T must be complete at that point.
    ~MemoryEncoder();

    /// Memory-backed constructor: loads per-channel amplification factors
    /// from IProceduralStore. onEncodingOutcome() adjusts them after retrieval.
    explicit MemoryEncoder(yuki::swarm::IProceduralStore& store);

    /// Convert 8 scores into 24-dim vector.
    std::vector<float> encodeScores(
        float question, float command, float emotional,
        float technical, float urgency, float greeting,
        float action, float polarity,
        const std::string& source_tag,
        uint64_t timestamp_ms
    ) const;

    /// Encode raw text for KnowledgeDaemon facts (TF-like heuristic).
    std::vector<float> encodeText(const std::string& text) const;

    /// Call after memory retrieval: reinforce or penalize per-channel amplification.
    /// channelIndex in [0,7]: 0=question, 1=command, 2=emotional, 3=technical,
    ///                         4=urgency,  5=greeting, 6=action,   7=polarity.
    void onEncodingOutcome(int channelIndex, bool retrievalGood);

private:
    std::unique_ptr<yuki::brain::memory::ProceduralHeuristicAccess> heuristic_;
    std::array<float, 8> ampFactors_{};

    void loadHeuristicValues();

    static std::array<float, OUTPUT_DIM> projectHeuristic(
        float q, float c, float e, float t, float u, float g, float a, float p
    );
};

} // namespace memory
} // namespace yuki
