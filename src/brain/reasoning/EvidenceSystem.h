#pragma once
// EvidenceSystem.h — Evidence building + verification (merged from EvidenceGraph + Verifier)
#include "BrainTypes.h"
#include <string>
#include <vector>

// ── §3.6 Evidence Layer ───────────────────────────────────────────────────────

class EvidenceGraphBuilder {
public:
    EvidenceGraph build(const std::vector<AgentResult>& results,
                        const std::string& directResponse,
                        const PatternFrame& frame) const;

    void addClaim(EvidenceGraph& graph,
                  const std::string& claim,
                  const std::string& sourceType,
                  const std::string& sourceId,
                  float confidence) const;

    float computeTrust(const EvidenceGraph& graph) const;

    std::vector<std::pair<std::string,std::string>>
        findContradictions(const EvidenceGraph& graph) const;

private:
    static std::string makeNodeId(int idx);
    static bool claimsContradict(const std::string& a, const std::string& b);
    static bool claimsSupport(const std::string& a, const std::string& b);
};

// ── §9 Verification ───────────────────────────────────────────────────────────

class Verifier {
public:
    VerificationReport verify(const PatternFrame&    frame,
                               const SynthesisResult& synthesis,
                               const EvidenceGraph&   graph) const;

    VerificationReport verifyAdvanced(
        const PatternFrame&                  frame,
        const SynthesisResult&               synthesis,
        const EvidenceGraph&                 graph,
        const std::vector<AgentResult>&      agentResults) const;

private:
    static int  extractContradictionCount(const std::vector<AgentResult>& results);
    static int  countDistinctSources(const EvidenceGraph& graph);
    static bool slotFilled(const std::string& slot, const EvidenceGraph& graph);
};
