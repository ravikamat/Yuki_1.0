#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "brain/metacognition/HypothesisConsumer.h"

namespace yuki {
namespace synthesis {

struct SynthesisSpec {
    metacognition::ActionableHypothesis source_hypothesis;
    uint32_t target_module_id = 0;
    std::string target_module_name;

    enum class ModificationType : uint8_t {
        ADD_FUNCTION = 0,
        MODIFY_FUNCTION = 1,
        ADD_MEMBER = 2,
        MODIFY_MEMBER = 3,
        REWIRE_FEATURE = 4,
        ADJUST_PARAMETER = 5,
        COUNT = 6
    };
    ModificationType mod_type = ModificationType::ADD_FUNCTION;

    uint32_t feature_index = 0;
    std::string parameter_name;
    float parameter_value = 0.0f;
    std::string existing_content;

    struct ValidationCriteria {
        bool must_compile = true;
        bool must_pass_tests = true;
        float min_competence_delta = 0.0f;
        uint32_t target_domain = 0;
    };
    ValidationCriteria validation;
};

} // namespace synthesis
} // namespace yuki
