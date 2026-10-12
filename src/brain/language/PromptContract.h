#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace yuki::language {

struct PromptConstraint {
    std::string type; // "must_include", "must_not_include", "max_length", "schema", "causal_precondition"
    std::string pattern;
    float weight = 1.0f;
};

struct PromptContract {
    std::vector<std::string> must_include;
    std::vector<std::string> must_not_include;
    std::string prefix_lock;
    std::vector<std::string> allowed_continuations;
    float temperature_override = -1.0f;
    size_t max_tokens = 0;
    std::string schema_type = "text"; // "text", "json", "code"
    std::vector<PromptConstraint> constraints;

    struct RetryConstraints {
        std::vector<std::string> must_include;
        std::vector<std::string> must_not_include;
        std::string prefix_lock;
        std::vector<std::string> allowed_continuations;
        float temperature_override = -1.0f;
    };
};

} // namespace yuki::language
