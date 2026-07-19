#include "ResponseActPlanner.h"
#include "core/ResponseResolver.h"
#include <algorithm>
#include <cctype>

ResponseActPlanner::ResponseActPlanner() {}

UtterancePlan ResponseActPlanner::build(
    const MeaningState& meaning,
    const FactBundle& facts,
    const std::optional<VerificationBundle>& verification) {

    if (verification.has_value()) {
        if (verification->pendingApproval) return buildApprovalPlan(*verification);
        if (verification->success)         return buildTaskSuccessPlan(*verification);
        return buildTaskFailurePlan(*verification);
    }

    return buildKnowledgeOrDefaultPlan(meaning, facts);
}

UtterancePlan ResponseActPlanner::buildApprovalPlan(const VerificationBundle& vb) {
    UtterancePlan plan;
    plan.act               = ResponseAct::APPROVAL_REQUEST;
    plan.userFacingSummary = vb.approval.summary;
    plan.riskySteps        = vb.approval.riskySteps;
    plan.requiresUserReply = true;
    return plan;
}

UtterancePlan ResponseActPlanner::buildTaskSuccessPlan(const VerificationBundle& vb) {
    UtterancePlan plan;
    plan.act               = ResponseAct::TASK_RESULT;
    plan.userFacingSummary = ResponseResolver::instance().resolve("TASK_SUCCESS");
    plan.evidenceLines     = vb.evidence;
    plan.requiresUserReply = false;
    return plan;
}

UtterancePlan ResponseActPlanner::buildTaskFailurePlan(const VerificationBundle& vb) {
    UtterancePlan plan;
    plan.act               = ResponseAct::TASK_FAILED;
    plan.userFacingSummary = ResponseResolver::instance().resolve("TASK_FAILED_EXEC");
    plan.evidenceLines     = vb.evidence;
    plan.requiresUserReply = false;
    return plan;
}

// ── Weak-content detector ──────────────────────────────────────────────────
// Returns true if the summary contains raw HTML or clearly failed extraction.
// Does NOT reject text merely because it mentions "Wikipedia" — a real extracted
// sentence may reference it legitimately.
static bool isRawHtml(const std::string& s) {
    return s.find("Jump to content")    != std::string::npos ||
           s.find("Main menu")          != std::string::npos ||
           s.find("</")                 != std::string::npos ||
           s.find("[[")                 != std::string::npos ||   // wiki markup
           s.find("Retrieved from")     != std::string::npos;
}

// Returns true if the text looks like a good, grounded answer.
// Used to decide whether to serve it directly without any wrapper prefix.
static bool isStrongAnswer(const std::string& s) {
    if (s.size() < 20)  return false;
    if (isRawHtml(s))   return false;
    // Still-learning stubs are not strong
    if (s.find("still learning") != std::string::npos) return false;
    if (s.find("queued for")     != std::string::npos) return false;
    return true;
}

UtterancePlan ResponseActPlanner::buildKnowledgeOrDefaultPlan(
    const MeaningState& meaning,
    const FactBundle& facts)
{
    UtterancePlan plan;

    // ── Confidence behavior overrides ────────────────────────────────────────
    if (meaning.behavior == ConfidenceBehavior::SINGLE_CLARIFY) {
        plan.act               = ResponseAct::CLARIFY;
        plan.requiresUserReply = true;
        plan.userFacingSummary = ResponseResolver::instance().resolve("CLARIFY_GENERIC");
        return plan;
    }

    if (meaning.behavior == ConfidenceBehavior::HONEST_UNKNOWN) {
        plan.act               = ResponseAct::KNOWLEDGE_ANSWER;
        plan.userFacingSummary = ResponseResolver::instance().resolve("HONEST_UNKNOWN");
        return plan;
    }

    const std::string action = meaning.goal.action;

    // ── KNOWLEDGE_QUERY / RESEARCH_REQUEST ───────────────────────────────────
    if (action == "KNOWLEDGE_QUERY"   ||
        action == "RESEARCH_REQUEST"  ||
        action == "CONVERSATION") {

        // Raw HTML → ask user to rephrase
        if (isRawHtml(facts.summary)) {
            plan.act               = ResponseAct::CLARIFY;
            plan.requiresUserReply = true;
            plan.userFacingSummary = ResponseResolver::instance().resolve("WEB_EXTRACT_FAILED");
            return plan;
        }

        plan.act = ResponseAct::KNOWLEDGE_ANSWER;
        if (isStrongAnswer(facts.summary)) {
            // Serve the answer directly — no wrapper, no prefix
            plan.userFacingSummary = facts.summary;
        } else if (!facts.summary.empty()) {
            // Weak but present — still serve it, user can ask follow-up
            plan.userFacingSummary = facts.summary;
        } else {
            // Empty — use ACKNOWLEDGED as a safe neutral non-generic fallback
            plan.userFacingSummary = ResponseResolver::instance().resolve("ACKNOWLEDGED");
        }
        return plan;
    }

    // ── TASK_REQUEST with no execution path ──────────────────────────────────
    // Emit a focused clarification instead of a generic "ok" acknowledgement.
    if (action == "TASK_REQUEST") {
        const std::string& topic = meaning.goal.target;
        plan.act               = ResponseAct::CLARIFY;
        plan.requiresUserReply = true;
        if (!topic.empty()) {
            plan.userFacingSummary = "What would you like me to do with \"" + topic + "\"? "
                "I can look it up, explain it, or help you build something — just let me know.";
        } else {
            plan.userFacingSummary = ResponseResolver::instance().resolve("CLARIFY_GENERIC");
        }
        return plan;
    }

    // ── Comparison query ─────────────────────────────────────────────────────
    // If GoalBuilder extracted a compare_with entity, build a focused question.
    if (action == "KNOWLEDGE_QUERY") {
        auto it = meaning.goal.parameters.find("compare_with");
        if (it != meaning.goal.parameters.end() && !it->second.empty()) {
            if (!isStrongAnswer(facts.summary)) {
                plan.act               = ResponseAct::CLARIFY;
                plan.requiresUserReply = true;
                plan.userFacingSummary = "Should I compare **" + meaning.goal.target +
                    "** and **" + it->second + "** in terms of features, performance, use cases, "
                    "or something else?";
                return plan;
            }
        }
    }

    // ── Default: acknowledge ─────────────────────────────────────────────────
    plan.act               = ResponseAct::ACKNOWLEDGE;
    plan.userFacingSummary = ResponseResolver::instance().resolve("ACKNOWLEDGED");
    return plan;
}
