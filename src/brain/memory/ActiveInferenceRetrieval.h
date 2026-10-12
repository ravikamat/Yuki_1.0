// ActiveInferenceRetrieval.h — Yuki_1.0 Phase A: AIR facade for T1/T2 retrieval
#pragma once
#include "InformationGainEngine.h"
#include "EpisodicStore.h"
#include "HdcSemanticGraph.h"
#include "brain/inference/PrecisionEngine.h"

namespace yuki {
namespace memory {

class ActiveInferenceRetrieval {
public:
    // The T1 store lives in yuki::brain::memory; this facade lives in
    // yuki::memory — qualify explicitly to avoid an unqualified-lookup failure.
    ActiveInferenceRetrieval(
        InformationGainEngine* ige,
        yuki::brain::memory::EpisodicStore* episodic,
        HdcSemanticGraph*      semantic);

    // Retrieve from T1 (episodic) using AIR.
    // Queries recent snapshots, computes IG per candidate, returns episode IDs sorted by gain.
    std::vector<std::string> retrieveEpisodic(
        const std::vector<float>&                         q_current,
        const yuki::brain::inference::PrecisionFactors&   prec,
        size_t                                            top_k) const;

    // Retrieve from T2 (semantic) using AIR.
    // Queries concepts, converts HDC embeddings to Hypervectors, returns IDs sorted by gain.
    std::vector<std::string> retrieveSemantic(
        const std::vector<float>&                         q_current,
        const yuki::brain::inference::PrecisionFactors&   prec,
        size_t                                            top_k) const;

private:
    InformationGainEngine* ige_;
    yuki::brain::memory::EpisodicStore* episodic_;
    HdcSemanticGraph*      semantic_;
};

} // namespace memory
} // namespace yuki
