#include "brain/learning/selfplay/SelfPlayEngine.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] SelfPlayEngine & SyntheticEvaluator..." << std::endl;

    using namespace yuki::learning::selfplay;

    SyntheticEvaluator evaluator;
    SyntheticTask task;
    task.task_id = 1;
    task.input_state = {0.1f, 0.2f};
    task.target_state = {1.0f, 1.0f};

    SelfPlayOutcome outcome;
    outcome.mse_error = 0.05f;
    outcome.execution_time_ms = 10.0f;
    outcome.final_state = {0.95f, 0.95f};

    float reward = evaluator.computeReward(outcome, task);
    assert(reward > 0.0f);
    assert(evaluator.isSolved(outcome, task, 0.1f) == true);

    SelfPlayEngine engine(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    SelfPlayOutcome ep_outcome = engine.runEpisode();
    assert(ep_outcome.reward >= -1.0f && ep_outcome.reward <= 1.0f);

    engine.trainBatch(3, 0.001f);

    std::cout << "[TEST] SelfPlayEngine PASSED!" << std::endl;
    return 0;
}
