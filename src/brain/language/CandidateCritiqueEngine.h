#pragma once

#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

struct CritiqueInput {
    std::string userQuery;
    std::string systemPrompt;
    std::string candidateText;
    GenerationTaskType taskType{GenerationTaskType::CHAT};
    bool requiresFacts{false};
    bool requiresCodeExactness{false};
    bool highImportance{false};
};

struct CritiqueResult {
    bool success{false};
    float overall{0.0f};
    float factuality{0.0f};
    float usefulness{0.0f};
    float fluency{0.0f};
    float safety{0.0f};
    bool approved{false};
    std::string rationale;
};

class CandidateCritiqueEngine {
public:
    explicit CandidateCritiqueEngine(IGenerationBackend& critiqueBackend);
    CritiqueResult critique(const CritiqueInput& input);
private:
    IGenerationBackend& critiqueBackend_;
};

} // namespace yuki::brain::language
