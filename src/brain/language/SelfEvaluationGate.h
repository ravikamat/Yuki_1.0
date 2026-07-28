#pragma once

#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

struct SelfEvalInput {
    std::string query;
    std::string candidate;
    GenerationTaskType taskType{GenerationTaskType::CHAT};
    float localConfidence{0.0f};
    float localFluency{0.0f};
    bool requiresFacts{false};
    bool requiresCodeExactness{false};
};

struct SelfEvalResult {
    float score{0.0f};
    bool approved{false};
    std::string reason;
};

class SelfEvaluationGate {
public:
    SelfEvalResult evaluate(const SelfEvalInput& input) const;
};

} // namespace yuki::brain::language
