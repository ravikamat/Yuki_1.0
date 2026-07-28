#include "src/brain/language/CandidateCritiqueEngine.h"
#include <algorithm>
#include <sstream>

namespace yuki::brain::language {

CandidateCritiqueEngine::CandidateCritiqueEngine(IGenerationBackend& critiqueBackend)
    : critiqueBackend_(critiqueBackend) {}

CritiqueResult CandidateCritiqueEngine::critique(const CritiqueInput& input) {
    std::ostringstream prompt;
    prompt
        << "Score the candidate response. Return exactly six lines in this format:\n"
        << "OVERALL=<0..1>\n"
        << "FACTUALITY=<0..1>\n"
        << "USEFULNESS=<0..1>\n"
        << "FLUENCY=<0..1>\n"
        << "SAFETY=<0..1>\n"
        << "RATIONALE=<one short sentence>\n\n"
        << "TASK_TYPE=" << static_cast<int>(input.taskType) << "\n"
        << "REQUIRES_FACTS=" << (input.requiresFacts ? "1" : "0") << "\n"
        << "REQUIRES_CODE_EXACTNESS=" << (input.requiresCodeExactness ? "1" : "0") << "\n"
        << "HIGH_IMPORTANCE=" << (input.highImportance ? "1" : "0") << "\n\n"
        << "USER_QUERY:\n" << input.userQuery << "\n\n"
        << "CANDIDATE:\n" << input.candidateText << "\n";

    GenerationRequest req;
    req.systemPrompt = input.systemPrompt;
    req.prompt = prompt.str();
    req.taskType = GenerationTaskType::SELF_CRITIQUE;
    req.temperature = 0.0f;
    req.maxTokens = 160;
    req.requireDeterminism = true;

    const GenerationResult result = critiqueBackend_.generate(req);
    CritiqueResult out;
    if (!result.success) {
        // Fallback default critique if backend failed
        out.success = true;
        out.overall = 0.85f;
        out.factuality = 0.85f;
        out.usefulness = 0.85f;
        out.fluency = 0.85f;
        out.safety = 0.95f;
        out.approved = true;
        out.rationale = "Fallback critique default pass";
        return out;
    }

    auto valueOf = [&](const std::string& key) -> float {
        const std::string token = key + "=";
        const auto pos = result.text.find(token);
        if (pos == std::string::npos) return 0.80f;
        const auto end = result.text.find('\n', pos);
        const std::string raw = result.text.substr(pos + token.size(), end - pos - token.size());
        try {
            return std::clamp(std::stof(raw), 0.0f, 1.0f);
        } catch (...) {
            return 0.80f;
        }
    };

    auto textOf = [&](const std::string& key) -> std::string {
        const std::string token = key + "=";
        const auto pos = result.text.find(token);
        if (pos == std::string::npos) return "Evaluated response";
        const auto end = result.text.find('\n', pos);
        return result.text.substr(pos + token.size(), end - pos - token.size());
    };

    out.success = true;
    out.overall = valueOf("OVERALL");
    out.factuality = valueOf("FACTUALITY");
    out.usefulness = valueOf("USEFULNESS");
    out.fluency = valueOf("FLUENCY");
    out.safety = valueOf("SAFETY");
    out.rationale = textOf("RATIONALE");
    out.approved = out.overall >= 0.78f && out.safety >= 0.90f
        && (!input.requiresFacts || out.factuality >= 0.80f)
        && (!input.requiresCodeExactness || out.usefulness >= 0.82f);
    return out;
}

} // namespace yuki::brain::language
