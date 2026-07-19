// EvidenceSystem.cpp — Evidence building + verification (merged from EvidenceGraph + Verifier)
#define NOMINMAX
#include "brain/reasoning/EvidenceSystem.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <set>

// ── Shared helpers ────────────────────────────────────────────────────────────

static std::string ev_toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return r;
}
static bool ev_has(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

// ══════════════════════════════════════════════════════════════════════════════
// EvidenceGraphBuilder
// ══════════════════════════════════════════════════════════════════════════════

std::string EvidenceGraphBuilder::makeNodeId(int idx) {
    return "ev_" + std::to_string(idx);
}

bool EvidenceGraphBuilder::claimsContradict(const std::string& a,
                                             const std::string& b) {
    auto la = ev_toLower(a), lb = ev_toLower(b);
    bool aNeg = ev_has(la,"not ") || ev_has(la," no ") || ev_has(la,"don't") || ev_has(la,"cannot");
    bool bNeg = ev_has(lb,"not ") || ev_has(lb," no ") || ev_has(lb,"don't") || ev_has(lb,"cannot");
    if (aNeg == bNeg) return false;
    std::istringstream sa(la), sb(lb);
    std::string w;
    std::vector<std::string> wordsA, wordsB;
    while (sa >> w) if (w.size()>3) wordsA.push_back(w);
    while (sb >> w) if (w.size()>3) wordsB.push_back(w);
    for (auto& wa : wordsA)
        for (auto& wb : wordsB)
            if (wa == wb) return true;
    return false;
}

bool EvidenceGraphBuilder::claimsSupport(const std::string& a,
                                          const std::string& b) {
    if (a.empty() || b.empty()) return false;
    auto la = ev_toLower(a), lb = ev_toLower(b);
    std::istringstream sa(la), sb(lb);
    std::string w;
    int shared = 0;
    std::vector<std::string> wordsA;
    while (sa >> w) if (w.size()>3) wordsA.push_back(w);
    while (sb >> w) if (w.size()>3)
        if (std::find(wordsA.begin(),wordsA.end(),w) != wordsA.end()) ++shared;
    return shared >= 2;
}

void EvidenceGraphBuilder::addClaim(EvidenceGraph& graph,
                                     const std::string& claim,
                                     const std::string& sourceType,
                                     const std::string& sourceId,
                                     float confidence) const {
    if (claim.empty()) return;
    EvidenceNode node;
    node.nodeId     = makeNodeId((int)graph.nodes.size());
    node.claim      = claim;
    node.sourceType = sourceType;
    node.sourceId   = sourceId;
    node.confidence = confidence;
    node.verified   = (confidence > 0.75f);

    for (auto& existing : graph.nodes) {
        if (claimsSupport(claim, existing.claim)) {
            node.supports.push_back(existing.nodeId);
            existing.supports.push_back(node.nodeId);
        }
        if (claimsContradict(claim, existing.claim)) {
            node.contradicts.push_back(existing.nodeId);
            existing.contradicts.push_back(node.nodeId);
        }
    }
    graph.nodes.push_back(std::move(node));
    graph.trustScore = computeTrust(graph);
}

float EvidenceGraphBuilder::computeTrust(const EvidenceGraph& graph) const {
    if (graph.nodes.empty()) return 0.0f;
    float sum = 0.0f;
    int contradictions = 0;
    for (auto& n : graph.nodes) {
        sum += n.confidence;
        contradictions += (int)n.contradicts.size();
    }
    float avg = sum / (float)graph.nodes.size();
    float penalty = std::min(0.4f, contradictions * 0.05f);
    return std::max(0.0f, avg - penalty);
}

std::vector<std::pair<std::string,std::string>>
EvidenceGraphBuilder::findContradictions(const EvidenceGraph& graph) const {
    std::vector<std::pair<std::string,std::string>> pairs;
    for (auto& n : graph.nodes)
        for (auto& cid : n.contradicts)
            if (n.nodeId < cid)
                pairs.emplace_back(n.nodeId, cid);
    return pairs;
}

EvidenceGraph EvidenceGraphBuilder::build(
    const std::vector<AgentResult>& results,
    const std::string& directResponse,
    const PatternFrame& frame) const
{
    EvidenceGraph graph;

    for (const auto& ar : results) {
        for (const auto& claim : ar.claims)
            addClaim(graph, claim, "agent", ar.agentId, ar.confidence);
        for (const auto& u : ar.unresolved)
            graph.unresolvedQuestions.push_back(u);
    }

    if (graph.nodes.empty() && !directResponse.empty())
        addClaim(graph, directResponse, "direct", "neural_spine", 0.60f);

    for (const auto& slot : frame.unknownSlots) {
        auto q = "missing: " + slot;
        if (std::find(graph.unresolvedQuestions.begin(),
                      graph.unresolvedQuestions.end(), q)
            == graph.unresolvedQuestions.end())
            graph.unresolvedQuestions.push_back(q);
    }

    return graph;
}

// ══════════════════════════════════════════════════════════════════════════════
// Verifier
// ══════════════════════════════════════════════════════════════════════════════

VerificationReport Verifier::verify(const PatternFrame&    frame,
                                     const SynthesisResult& synthesis,
                                     const EvidenceGraph&   graph) const {
    VerificationReport rep;
    float score = 0.0f;

    if (!synthesis.finalText.empty())  score += 0.30f;
    if (synthesis.complete)            score += 0.20f;
    score += graph.trustScore * 0.30f;

    int met = 0, total = (int)frame.explicitConstraints.size();
    for (const auto& c : frame.explicitConstraints) {
        if (synthesis.finalText.find(c) != std::string::npos) ++met;
        else rep.weakClaims.push_back("constraint_unmet: " + c);
    }
    if (total > 0) score += 0.10f * ((float)met / total);

    for (const auto& slot : frame.unknownSlots)
        rep.missingNeeds.push_back(slot);
    if (!frame.unknownSlots.empty()) score -= 0.10f;

    int contradictions = 0;
    for (const auto& n : graph.nodes)
        contradictions += (int)n.contradicts.size();
    score -= contradictions * 0.05f;
    score -= (float)graph.unresolvedQuestions.size() * 0.03f;

    rep.satisfactionScore = std::max(0.0f, std::min(1.0f, score));
    rep.satisfied = (rep.satisfactionScore >= 0.45f &&
                     synthesis.complete &&
                     rep.missingNeeds.empty());
    return rep;
}

int Verifier::extractContradictionCount(const std::vector<AgentResult>& results) {
    for (const auto& r : results) {
        if (r.agentId.find("ContradictionHunter") != std::string::npos ||
            r.metadata.count("contradictions")) {
            auto it = r.metadata.find("contradictions");
            if (it != r.metadata.end()) {
                try { return std::stoi(it->second); } catch (...) {}
            }
        }
    }
    return 0;
}

int Verifier::countDistinctSources(const EvidenceGraph& graph) {
    std::set<std::string> types;
    for (const auto& n : graph.nodes)
        if (!n.sourceType.empty()) types.insert(n.sourceType);
    return (int)types.size();
}

bool Verifier::slotFilled(const std::string& slot, const EvidenceGraph& graph) {
    if (slot.size() < 4) return false;
    std::string key = slot.substr(0, std::min((size_t)20, slot.size()));
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    for (const auto& n : graph.nodes) {
        for (const auto& fs : n.fillsSlots) {
            std::string fsl = fs;
            std::transform(fsl.begin(), fsl.end(), fsl.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (fsl.find(key) != std::string::npos) return true;
        }
        std::string lc = n.claim;
        std::transform(lc.begin(), lc.end(), lc.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (lc.find(key) != std::string::npos && n.confidence > 0.0f) return true;
    }
    return false;
}

VerificationReport Verifier::verifyAdvanced(
    const PatternFrame&             frame,
    const SynthesisResult&          synthesis,
    const EvidenceGraph&            graph,
    const std::vector<AgentResult>& agentResults) const
{
    VerificationReport rep = verify(frame, synthesis, graph);
    float score = rep.satisfactionScore;

    int externalCount = 0;
    for (const auto& n : graph.nodes)
        if (n.sourceType == "external") ++externalCount;
    if (externalCount > 0) {
        float discount = std::min(0.15f, externalCount * 0.05f);
        score -= discount;
        rep.weakClaims.push_back("external_claims: " + std::to_string(externalCount)
                                 + " (trust discounted by "
                                 + std::to_string((int)(discount * 100)) + "%)");
    }

    int distinctSources = countDistinctSources(graph);
    if (distinctSources >= 2) {
        float bonus = std::min(0.08f, (distinctSources - 1) * 0.04f);
        score += bonus;
    }

    int agentContradictions = extractContradictionCount(agentResults);
    if (agentContradictions > 0) {
        float penalty = std::min(0.20f, agentContradictions * 0.07f);
        score -= penalty;
        rep.weakClaims.push_back("contradictions_detected: "
                                 + std::to_string(agentContradictions));
    }

    int filledSlots = 0;
    for (const auto& slot : frame.unknownSlots) {
        if (slotFilled(slot, graph)) {
            ++filledSlots;
            auto it = std::find(rep.missingNeeds.begin(),
                                rep.missingNeeds.end(), slot);
            if (it != rep.missingNeeds.end()) rep.missingNeeds.erase(it);
        }
    }
    if (filledSlots > 0)
        score += std::min(0.10f, filledSlots * 0.05f);

    for (const auto& uq : graph.unresolvedQuestions) {
        if (std::find(rep.missingNeeds.begin(), rep.missingNeeds.end(), uq)
                == rep.missingNeeds.end())
            rep.missingNeeds.push_back(uq);
    }

    rep.satisfactionScore = std::max(0.0f, std::min(1.0f, score));
    rep.satisfied = (rep.satisfactionScore >= 0.45f &&
                     synthesis.complete &&
                     rep.missingNeeds.empty());
    return rep;
}
