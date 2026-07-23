#include "brain/research/core/Synthesizer.h"
#include <cassert>

int main() {
    yuki::research::Synthesizer synth;

    std::vector<yuki::research::SubGoal> goals;
    yuki::research::SubGoal g1;
    g1.goalId = 100;
    goals.push_back(g1);

    std::vector<yuki::research::ToolResult> results;
    yuki::research::ToolResult r1;
    r1.nodeId = 100;
    r1.status = yuki::research::ToolStatus::SUCCESS;
    r1.confidence = 0.9f;
    results.push_back(r1);

    auto pack = synth.synthesize(goals, results);
    assert(pack.overallConfidence > 0.0f);

    return 0;
}
