// ActiveInferenceRetrieval.cpp — T1 + T2 retrieval via InformationGainEngine
// T1 candidates come from EpisodicStore::queryRecentSnapshots(); T2 candidates
// come from HdcSemanticGraph::getAllConceptStats().
#include "brain/memory/ActiveInferenceRetrieval.h"
#include <algorithm>
#include <functional>

namespace yuki {
namespace memory {

ActiveInferenceRetrieval::ActiveInferenceRetrieval(
    InformationGainEngine* ige,
    yuki::brain::memory::EpisodicStore* episodic,
    HdcSemanticGraph*      semantic)
    : ige_(ige), episodic_(episodic), semantic_(semantic) {}

// ── Helper: deterministic uint64 seed from a string id ────────────────────────
static uint64_t hashId(const std::string& id) {
    std::hash<std::string> h;
    uint64_t seed = h(id);
    // Mix for better bit distribution (xorshift64)
    seed ^= seed >> 33;
    seed *= 0xff51afd7ed558ccdULL;
    seed ^= seed >> 33;
    seed *= 0xc4ceb9fe1a85ec53ULL;
    seed ^= seed >> 33;
    return seed;
}

// ── T1 Episodic retrieval ─────────────────────────────────────────────────────
// Candidate pool = most recent episodes (bounded so IG ranking stays cheap).
std::vector<std::string> ActiveInferenceRetrieval::retrieveEpisodic(
    const std::vector<float>&          q_current,
    const yuki::brain::inference::PrecisionFactors& prec,
    size_t                             top_k) const
{
    if (!ige_ || !episodic_) return {};

    constexpr size_t kPoolSize = 256;
    auto records = episodic_->queryRecentSnapshots(kPoolSize, false);
    if (records.empty()) return {};

    std::vector<std::pair<std::string, Hypervector>> candidates;
    candidates.reserve(records.size());
    for (const auto& r : records) {
        const std::string id = "ep_" + std::to_string(r.id);
        // Seed from the episode text so semantically similar episodes share
        // bits; fall back to the id when the text is empty.
        if (r.text.empty()) candidates.emplace_back(id, Hypervector(hashId(id)));
        else                candidates.emplace_back(id, Hypervector(r.text));
    }

    auto ranked = ige_->rankCandidates(candidates, q_current, prec);

    std::vector<std::string> ids;
    ids.reserve(std::min(top_k, ranked.size()));
    for (size_t i = 0; i < top_k && i < ranked.size(); ++i)
        ids.push_back(ranked[i].id);
    return ids;
}

// ── T2 Semantic retrieval ─────────────────────────────────────────────────────
// getAllConceptStats() returns ConceptStats{name/id, ...} (added previous session)
std::vector<std::string> ActiveInferenceRetrieval::retrieveSemantic(
    const std::vector<float>&          q_current,
    const yuki::brain::inference::PrecisionFactors& prec,
    size_t                             top_k) const
{
    if (!ige_ || !semantic_) return {};

    auto cstats = semantic_->getAllConceptStats();
    if (cstats.empty()) return {};

    std::vector<std::pair<std::string, Hypervector>> candidates;
    candidates.reserve(cstats.size());
    for (const auto& c : cstats) {
        // HdcConcept::id → text-seeded HV (deterministic)
        candidates.emplace_back(c.id, Hypervector(c.id));
    }

    auto ranked = ige_->rankCandidates(candidates, q_current, prec);

    std::vector<std::string> ids;
    ids.reserve(std::min(top_k, ranked.size()));
    for (size_t i = 0; i < top_k && i < ranked.size(); ++i)
        ids.push_back(ranked[i].id);
    return ids;
}

} // namespace memory
} // namespace yuki
