// InformationGainEngine.cpp — exact KL information gain for Active Inference Retrieval
#include "brain/memory/InformationGainEngine.h"
#include "brain/inference/GenerativeModel.h"
#include <algorithm>
#include <functional>
#include <numeric>

namespace yuki {
namespace memory {

InformationGainEngine::InformationGainEngine(const inference::GenerativeModel* gm)
    : generative_model_(gm) {}

// ── p(o|s) computation ────────────────────────────────────────────────────────
// For intent states (s = 0..7): hash the EMA mapping vector → reference HV →
//   Hamming similarity → normalise to [0,1].
// For engagement/urgency states (s = 8..12): uniform 0.5 (no directional mapping).
// For padding dims (s = 13..23): 0.0 (q_current must also be 0 here; skipped in KL).
std::vector<float> InformationGainEngine::computeLikelihoods(const Hypervector& obs) const {
    constexpr size_t N_STATES = 24;
    std::vector<float> likelihoods(N_STATES, 0.5f);

    if (generative_model_) {
        for (int i = 0; i < 8; ++i) {
            // getMapping returns the EMA feature vector for this intent class.
            auto mapping = generative_model_->getMapping(
                static_cast<yuki::IntentClass>(i),
                yuki::perception::Modality::TEXT);

            // Hash the mapping to a deterministic uint64_t seed.
            size_t seed = 0;
            for (float v : mapping) {
                seed ^= std::hash<float>{}(v)
                        + 0x9e3779b97f4a7c15ULL
                        + (seed << 6) + (seed >> 2);
            }

            // Build a reference hypervector for this intent's expected observation.
            Hypervector expected_hv(static_cast<uint64_t>(seed));

            // cosine similarity ∈ [-1, 1] → map to [0, 1]
            float sim = obs.cosineSimilarity(expected_hv);
            likelihoods[static_cast<size_t>(i)] = (sim + 1.0f) * 0.5f;
        }
    }

    // Dims 8-12: uniform (engagement/urgency — no generative model mapping)
    for (size_t s = 8; s < 13; ++s) likelihoods[s] = 0.5f;
    // Dims 13-23: 0.0 (padding — q_current is 0 here, KL contribution skipped)
    for (size_t s = 13; s < N_STATES; ++s) likelihoods[s] = 0.0f;

    return likelihoods;
}

// ── Exact KL divergence ───────────────────────────────────────────────────────
// q'(s|m) ∝ q(s) * p(o=m | s).  Normalise.  KL = Σ_s q'(s) * ln(q'(s)/q(s)).
// Precision-weighted result = KL * overall_precision(prec).
float InformationGainEngine::computeInformationGain(
    const Hypervector&             candidate_obs,
    const std::vector<float>&      q_current,
    const inference::PrecisionFactors& prec) const
{
    constexpr size_t N = 24;
    const size_t     n  = std::min(q_current.size(), N);

    auto likelihoods = computeLikelihoods(candidate_obs);

    // Compute unnormalised q'(s) = q(s) * likelihood(s)
    float qprime[N] = {};
    float sum = 0.0f;
    for (size_t s = 0; s < n; ++s) {
        qprime[s] = q_current[s] * likelihoods[s];
        sum       += qprime[s];
    }

    // If all zero (degenerate likelihood), return 0.
    if (sum <= 0.0f) return 0.0f;

    // Normalise q'
    const float inv_sum = 1.0f / sum;
    for (size_t s = 0; s < N; ++s) qprime[s] *= inv_sum;

    // KL(q' || q) = Σ_s q'(s) * ln(q'(s) / q(s))
    // Skip states where q(s) == 0  (spec: "KL contribution is 0").
    float kl = 0.0f;
    for (size_t s = 0; s < n; ++s) {
        if (q_current[s] <= 0.0f) continue;   // skip zero-mass prior states
        if (qprime[s]   <= 0.0f) continue;    // log(0) undefined; contribution = 0
        kl += qprime[s] * std::log(qprime[s] / q_current[s]);
    }
    if (kl < 0.0f) kl = 0.0f;  // numerical guard

    return kl * overallPrecision(prec);
}

// ── Batch rank ────────────────────────────────────────────────────────────────
std::vector<InformationGainEngine::Candidate>
InformationGainEngine::rankCandidates(
    const std::vector<std::pair<std::string, Hypervector>>& candidates,
    const std::vector<float>&          q_current,
    const inference::PrecisionFactors& prec) const
{
    std::vector<Candidate> out;
    out.reserve(candidates.size());

    for (const auto& [id, hv] : candidates) {
        Candidate c;
        c.id                   = id;
        c.precision_weighted_gain = computeInformationGain(hv, q_current, prec);
        c.information_gain     = c.precision_weighted_gain / std::max(1e-6f, overallPrecision(prec));
        out.push_back(c);
    }

    std::sort(out.begin(), out.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.precision_weighted_gain > b.precision_weighted_gain;
              });
    return out;
}

float InformationGainEngine::overallPrecision(const inference::PrecisionFactors& prec) {
    float p = prec.signal_snr
            * (1.0f - prec.dropout_rate)
            * prec.context_relevance;
    return p > 0.0f ? p : 1e-6f;  // avoid zero precision
}

} // namespace memory
} // namespace yuki
