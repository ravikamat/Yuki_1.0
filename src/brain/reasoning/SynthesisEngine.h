#pragma once
// SynthesisEngine.h — §3.7 Synthesis Layer
#include "BrainTypes.h"
#include <string>

class SynthesisEngine {
public:
    // Build a synthesis plan based on situation requirements
    SynthesisPlan buildPlan(const CognitiveSituation& situation,
                             const EvidenceGraph&      graph) const;

    // Produce final text from evidence + plan
    SynthesisResult synthesize(const SynthesisPlan&    plan,
                                const EvidenceGraph&   graph,
                                const CognitiveSituation& situation) const;

    // Dynamically format an emotional response using KB advice and user context
    std::string synthesizeEmpathy(const std::string& moodLabel,
                                   const std::string& kbAdvice,
                                   const std::string& userName,
                                   const std::string& topDomain) const;
private:
    std::string formatAsText(const std::vector<const EvidenceNode*>& nodes) const;
    std::string formatAsBullets(const std::vector<const EvidenceNode*>& nodes) const;
    std::string formatAsCode(const std::vector<const EvidenceNode*>& nodes) const;
    std::string formatAsReport(const std::vector<const EvidenceNode*>& nodes) const;
    std::string surfaceUncertainty(const EvidenceGraph& graph, float bestConf) const;
};
