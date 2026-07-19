#pragma once
#include "MeaningTypes.h"
class GoalBuilder {
public:
    GoalBuilder();
    Goal build(const MeaningState& state) const;
};
