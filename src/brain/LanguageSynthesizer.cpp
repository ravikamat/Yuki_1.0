#include "LanguageSynthesizer.h"
#include <cctype>

LanguageSynthesizer::LanguageSynthesizer() {}

std::string LanguageSynthesizer::synthesize(const std::string& actPlan) {
    return actPlan;
}

// ── Local: trim trailing whitespace/newlines from a response string ──────────
static std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string LanguageSynthesizer::render(const UtterancePlan& plan) {
    std::string out;

    switch (plan.act) {
        case ResponseAct::APPROVAL_REQUEST:
            out = plan.userFacingSummary;
            if (!plan.riskySteps.empty()) {
                out += "\nRisky steps:\n";
                for (size_t i = 0; i < plan.riskySteps.size(); ++i)
                    out += std::to_string(i + 1) + ". " + plan.riskySteps[i] + "\n";
            }
            break;

        case ResponseAct::TASK_RESULT:
            out = plan.userFacingSummary;
            if (!plan.evidenceLines.empty()) {
                out += "\nOutput:\n";
                for (const auto& line : plan.evidenceLines)
                    out += line + "\n";
            }
            break;

        case ResponseAct::TASK_FAILED:
            out = plan.userFacingSummary;
            if (!plan.evidenceLines.empty()) {
                out += "\nError Output:\n";
                for (const auto& line : plan.evidenceLines)
                    out += line + "\n";
            }
            break;

        case ResponseAct::KNOWLEDGE_ANSWER:
            // Serve the answer as-is — ResponseActPlanner already validated it.
            out = plan.userFacingSummary;
            break;

        case ResponseAct::CLARIFY:
            out = plan.userFacingSummary;
            break;

        case ResponseAct::ACKNOWLEDGE:
            out = plan.userFacingSummary;
            break;

        default:
            // Unknown plan act: return empty string.
            // MotherCore's fallbackDelegate_ or its own smart fallback will handle it.
            out = "";
            break;
    }

    return trimRight(out);
}
