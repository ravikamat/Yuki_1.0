// SynthesisEngine.cpp — §3.7 Real synthesis — picks best evidence, composes answer
#define NOMINMAX
#include "brain/reasoning/SynthesisEngine.h"
#include "LanguageSynthesizer.h"
#include <sstream>
#include <algorithm>
#include <numeric>

SynthesisPlan SynthesisEngine::buildPlan(const CognitiveSituation& sit,
                                          const EvidenceGraph& graph) const {
    SynthesisPlan plan;
    plan.outputMode = sit.pattern.outputMode;

    for (const auto& c : sit.pattern.explicitConstraints)
        plan.mustInclude.push_back(c);

    // Surface uncertainty when evidence is thin or contradicted
    plan.includeUncertainty = (graph.trustScore < 0.50f ||
                                !graph.unresolvedQuestions.empty());

    // Action plan note for commands/implementations
    plan.includeActionPlan = (sit.pattern.requestMode == RequestMode::COMMAND ||
                               sit.pattern.requestMode == RequestMode::IMPLEMENTATION);

    if (sit.userState.depthExpectation > 0.65f)
        plan.optionalEnhancements.push_back("examples");
    if (sit.userState.depthExpectation > 0.80f)
        plan.optionalEnhancements.push_back("edge_cases");

    return plan;
}

SynthesisResult SynthesisEngine::synthesize(
    const SynthesisPlan&      plan,
    const EvidenceGraph&      graph,
    const CognitiveSituation& sit) const
{
    SynthesisResult result;

    // ── Select evidence nodes that carry real content ─────────────────────────
    // Filter out zero-confidence nodes (metadata-only agents like IntentAnalyst)
    std::vector<const EvidenceNode*> realNodes;
    for (const auto& n : graph.nodes) {
        if (n.confidence > 0.0f && !n.claim.empty())
            realNodes.push_back(&n);
    }

    // Sort by confidence descending
    std::sort(realNodes.begin(), realNodes.end(),
              [](const EvidenceNode* a, const EvidenceNode* b){
                  return a->confidence > b->confidence;
              });

    // ── No real evidence — use dynamic fallback ───────────────────────────────
    if (realNodes.empty()) {
        bool isConversational = (sit.pattern.goal == "converse" ||
                                 sit.pattern.goal == "provide_support" ||
                                 sit.pattern.goal == "describe_self");

        if (isConversational) {
            // Compose a goal label from the available pattern info
            std::string goalLabel = sit.pattern.goal;
            if (!sit.pattern.domain.empty()) goalLabel += " " + sit.pattern.domain;
            LanguageSynthesizer synth;
            result.finalText = synth.synthesize(goalLabel);
            result.complete = true;
            result.groundedConfidence = 0.90f;
            return result;
        }

        // Non-conversational with no evidence: return empty so the parent caller
        // (NeuralSpine → fallback delegate → MotherCore Gate 8) can serve a
        // DB-backed answer or a clean specific fallback.
        // Do NOT return a raw goal-label string — that leaks internal labels to the user.
        result.finalText          = "";
        result.complete           = false;
        result.groundedConfidence = 0.0f;
        if (plan.includeUncertainty)
            result.notes.push_back("no_evidence");
        return result;
    }

    // ── Format based on output mode ───────────────────────────────────────────
    std::string body;
    switch (plan.outputMode) {
        case OutputMode::BULLETS:      body = formatAsBullets(realNodes);              break;
        case OutputMode::CODE:         body = formatAsCode(realNodes);   break;
        case OutputMode::PATCH:        body = formatAsCode(realNodes);   break;
        case OutputMode::REPORT:       body = formatAsReport(realNodes);               break;
        case OutputMode::ARCHITECTURE: body = formatAsReport(realNodes);               break;
        default:                       body = formatAsText(realNodes);                 break;
    }

    // ── Append uncertainty note if warranted ─────────────────────────────────
    if (plan.includeUncertainty && graph.trustScore < 0.50f)
        body += "\n\n" + surfaceUncertainty(graph, realNodes[0]->confidence);

    // ── Append action plan stub ───────────────────────────────────────────────
    if (plan.includeActionPlan &&
        sit.pattern.requestMode == RequestMode::COMMAND)
        body += "\n(Command routed to execution pipeline.)";

    result.finalText          = body;
    result.complete           = !body.empty();
    result.groundedConfidence = graph.trustScore;

    for (const auto& u : graph.unresolvedQuestions)
        result.notes.push_back("unresolved: " + u);

    return result;
}

// ── Format implementations ────────────────────────────────────────────────────

// TEXT: best single node (highest confidence), with optional supplement from second
std::string SynthesisEngine::formatAsText(
    const std::vector<const EvidenceNode*>& nodes) const
{
    if (nodes.empty()) return "";

    // Primary answer from best node
    std::string answer = nodes[0]->claim;

    // If first node is modest confidence and second adds new info, append it
    if (nodes.size() > 1 &&
        nodes[0]->confidence < 0.70f &&
        nodes[1]->confidence > 0.35f) {
        // Only append if the second claim is substantially different
        if (nodes[1]->claim.size() > 20 &&
            nodes[1]->claim.find(nodes[0]->claim.substr(0, 20)) == std::string::npos) {
            answer += "\n\nAdditional context: " + nodes[1]->claim;
        }
    }

    return answer;
}

// BULLETS: each real evidence node becomes a bullet
std::string SynthesisEngine::formatAsBullets(
    const std::vector<const EvidenceNode*>& nodes) const
{
    std::ostringstream ss;
    for (const auto* n : nodes) {
        if (n->claim.empty()) continue;
        // Split multi-sentence claims into sub-bullets
        std::string claim = n->claim;
        if (claim.size() <= 120) {
            ss << "• " << claim << "\n";
        } else {
            // Split on sentence boundary
            size_t p = 0;
            while (p < claim.size()) {
                size_t end = claim.find_first_of(".!?", p);
                if (end == std::string::npos) end = claim.size() - 1;
                std::string sentence = claim.substr(p, end - p + 1);
                if (!sentence.empty() && sentence.find_first_not_of(" .\n") != std::string::npos)
                    ss << "• " << sentence << "\n";
                p = end + 1;
                while (p < claim.size() && claim[p] == ' ') ++p;
            }
        }
    }
    return ss.str();
}

// CODE: prefer claim that looks like code
std::string SynthesisEngine::formatAsCode(
    const std::vector<const EvidenceNode*>& nodes) const
{
    for (const auto* n : nodes) {
        const auto& c = n->claim;
        if (c.find("```") != std::string::npos ||
            c.find("void ") != std::string::npos ||
            c.find("def ") != std::string::npos  ||
            c.find("function ") != std::string::npos ||
            c.find("class ") != std::string::npos)
            return c;
    }
    // Codebase hits from CodeArchaeologist are marked by leading spaces
    for (const auto* n : nodes) {
        if (n->sourceId == "CodeArchaeologist" || n->sourceType == "agent")
            if (n->claim.find("  ") != std::string::npos)  // indented code hit
                return n->claim;
    }
    return formatAsText(nodes);
}

// ── Empathy Synthesis ────────────────────────────────────────────────────────

std::string SynthesisEngine::synthesizeEmpathy(const std::string& moodLabel,
                                               const std::string& kbAdvice,
                                               const std::string& userName,
                                               const std::string& topDomain) const
{
    std::ostringstream ss;
    
    // Instead of fixed string matching, compose thoughts based on available knowledge
    if (!userName.empty()) {
        ss << userName << " — ";
    }
    
    if (!kbAdvice.empty()) {
        ss << kbAdvice;
    } else {
        // Dynamic fallback when KB has no advice
        ss << "I'm still learning how to help with feeling " << moodLabel << ". ";
        if (!topDomain.empty()) {
            ss << "My current knowledge is mostly about " << topDomain << ", but I'll research this for you.";
        } else {
            ss << "I'll learn more about this soon.";
        }
    }
    
    return ss.str();
}

// REPORT: all nodes concatenated as paragraphs, deduped
std::string SynthesisEngine::formatAsReport(
    const std::vector<const EvidenceNode*>& nodes) const
{
    std::ostringstream ss;
    std::string seen;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& c = nodes[i]->claim;
        if (c.empty() || seen.find(c.substr(0, 30)) != std::string::npos)
            continue;
        if (!seen.empty()) ss << "\n\n";
        ss << c;
        seen += c.substr(0, 30);
    }
    return ss.str();
}

// Uncertainty note — honest about what's missing
std::string SynthesisEngine::surfaceUncertainty(
    const EvidenceGraph& graph, float bestConf) const
{
    std::ostringstream ss;
    if (bestConf < 0.40f) {
        ss << "Note: I'm not very certain about this (confidence "
           << static_cast<int>(bestConf * 100) << "%).";
    } else {
        ss << "Note: partial answer (confidence "
           << static_cast<int>(bestConf * 100) << "%).";
    }
    if (!graph.unresolvedQuestions.empty()) {
        ss << " Still learning: ";
        size_t limit = std::min((size_t)2, graph.unresolvedQuestions.size());
        for (size_t i = 0; i < limit; ++i) {
            if (i) ss << ", ";
            // Strip "unknown: " prefix for cleaner speech
            auto& q = graph.unresolvedQuestions[i];
            auto p = q.find(": ");
            ss << (p != std::string::npos ? q.substr(p+2) : q);
        }
        ss << ".";
    }
    return ss.str();
}
