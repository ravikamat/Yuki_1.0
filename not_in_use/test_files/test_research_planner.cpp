#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/core/ToolRegistry.h"
#include <iostream>
#include <cassert>

int main() {
    yuki::research::ToolRegistry registry;
    yuki::research::ResearchPlanner planner(&registry);

    auto goals = planner.decompose("Build Android app for tracking expenses");
    assert(!goals.empty());

    auto gaps = planner.detectGaps(goals);
    assert(gaps.size() == goals.size());

    return 0;
}
