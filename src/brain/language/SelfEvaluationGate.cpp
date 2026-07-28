#include "src/brain/language/SelfEvaluationGate.h"
#include <algorithm>

namespace yuki::brain::language {

SelfEvalResult SelfEvaluationGate::evaluate(const SelfEvalInput& input) const {
    float score = 0.0f;

    score += 0.35f * std::clamp(input.localConfidence, 0.0f, 1.0f);
    score += 0.25f * std::clamp(input.localFluency, 0.0f, 1.0f);

    const size_t length = input.candidate.size();
    const bool nonTrivial = length >= 48;
    const bool notOverlong = length <= 6000;
    const bool noEmptyResponse = !input.candidate.empty();

    score += noEmptyResponse ? 0.10f : 0.0f;
    score += nonTrivial ? 0.10f : 0.0f;
    score += notOverlong ? 0.05f : 0.0f;

    const bool mentionsUncertainty = input.candidate.find("not sure") != std::string::npos
        || input.candidate.find("uncertain") != std::string::npos;
    if (input.requiresFacts && mentionsUncertainty) {
        score -= 0.10f;
    }

    if (input.requiresCodeExactness) {
        const bool hasStructuredCode = input.candidate.find("```") != std::string::npos
            || input.candidate.find("#include") != std::string::npos
            || input.candidate.find("class ") != std::string::npos;
        score += hasStructuredCode ? 0.15f : -0.15f;
    }

    score = std::clamp(score, 0.0f, 1.0f);

    SelfEvalResult out;
    out.score = score;
    out.approved = score >= (input.requiresCodeExactness ? 0.82f : 0.72f)
        && (!input.requiresFacts || input.localConfidence >= 0.74f);
    out.reason = out.approved ? "local-acceptable" : "fallback-needed";
    return out;
}

} // namespace yuki::brain::language
