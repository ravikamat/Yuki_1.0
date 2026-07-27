#include "CurriculumGenerator.h"
#include <algorithm>

namespace yuki::learning::neural {

CurriculumTask CurriculumGenerator::generate_next_task() {
    CurriculumTask task;
    task.difficulty_level = current_stage_;
    
    size_t dim = 4 * current_stage_;
    task.input_data = Matrix::random(1, dim, -1.0f, 1.0f);
    task.target_data = Matrix::ones(1, 1);
    return task;
}

void CurriculumGenerator::update_competence(float score) {
    competence_ = 0.8f * competence_ + 0.2f * score;
    if (competence_ > 0.85f && current_stage_ < 10) {
        current_stage_++;
        competence_ = 0.5f;
    }
}

} // namespace yuki::learning::neural
