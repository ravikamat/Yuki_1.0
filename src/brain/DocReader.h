#pragma once
#include "ExecutionTypes.h"
#include <string>

class DocReader {
public:
    std::string learn(const GoalModel& model);
};
