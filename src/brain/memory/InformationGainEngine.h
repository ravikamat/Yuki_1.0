// InformationGainEngine.h — Yuki_1.0 Phase A: Exact KL-based AIR retrieval
// IG(m) = Σ_s q'(s|m) * ln(q'(s|m) / q(s))  — exact, deterministic, O(24) per candidate.
#pragma once
#include <vector>
#include <string>
#include <utility>
#include <cmath>
#include "brain/memory/Hypervector.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/PrecisionEngine.h"

namespace yuki {
namespace memory {

class InformationGainEngine {
public:
    struct Candidate {
        std::string id;
        float information_gain         = 0.0f;
        float precision_weighted_gain  = 0.0f;
    };

    // gm may be nullptr (returns uniform likelihoods → IG = 0 for all candidates)
    explicit InformationGainEngine(const yuki::inference::GenerativeModel* gm);

    // Compute exact KL divergence for one candidate hypervector observation.
    // q_current: 24-dim factorised posterior (q_intent[8], q_engagement[3], q_urgency[2], pad[11])
    // prec: precision factors used to weight the final gain.
    // Returns precision-weighted information gain >= 0.
    float computeInformationGain(
        const Hypervector&                         candidate_obs,
        const std::vector<float>&                  q_current,
        const yuki::inference::PrecisionFactors&   prec) const;

    // Batch rank. Returns candidates sorted descending by precision_weighted_gain.
    std::vector<Candidate> rankCandidates(
        const std::vector<std::pair<std::string, Hypervector>>& candidates,
        const std::vector<float>&                q_current,
        const yuki::inference::PrecisionFactors& prec) const;

    // Compute p(o|s) for each of the 24 states from the GenerativeModel EMA mapping.
    // Dims 0–7: intent states (EMA-based likelihood via Hamming similarity).
    // Dims 8–12: engagement/urgency states (uniform 0.5).
    // Dims 13–23: padding states (0.0 → skipped in KL, q must also be 0).
    std::vector<float> computeLikelihoods(const Hypervector& obs) const;

    // Overall precision scalar: snr * (1-dropout) * context_relevance
    static float overallPrecision(const yuki::inference::PrecisionFactors& prec);

private:
    const yuki::inference::GenerativeModel* generative_model_;
};

} // namespace memory
} // namespace yuki
