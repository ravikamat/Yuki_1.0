#pragma once
#include "Matrix.h"
#include <vector>

namespace yuki::learning::neural {

struct CurriculumTask {
    uint32_t difficulty_level{1};
    Matrix input_data;
    Matrix target_data;
};

class CurriculumGenerator {
    uint32_t current_stage_{1};
    float competence_{0.0f};

public:
    CurriculumGenerator() = default;

    CurriculumTask generate_next_task();
    void update_competence(float score);
    uint32_t current_stage() const { return current_stage_; }
    float competence() const { return competence_; }
};

} // namespace yuki::learning::neural
