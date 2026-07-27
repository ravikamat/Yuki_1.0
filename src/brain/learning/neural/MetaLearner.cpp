#include "MetaLearner.h"

namespace yuki::learning::neural {

MetaLearner::MetaLearner(const NeuralNetwork& base_model) : meta_model_(base_model) {}

NeuralNetwork MetaLearner::inner_step(const Matrix& task_input, const Matrix& task_target) const {
    NeuralNetwork adapted = meta_model_;
    adapted.train_step(task_input, task_target, alpha_, LossType::MSE);
    return adapted;
}

void MetaLearner::outer_step(const std::vector<Matrix>& task_inputs, const std::vector<Matrix>& task_targets) {
    if (task_inputs.empty() || task_inputs.size() != task_targets.size()) return;

    for (size_t t = 0; t < task_inputs.size(); ++t) {
        NeuralNetwork adapted = inner_step(task_inputs[t], task_targets[t]);
        adapted.train_step(task_inputs[t], task_targets[t], beta_, LossType::MSE);
    }
}

} // namespace yuki::learning::neural
