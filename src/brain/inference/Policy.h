// Policy.h — Minimal policy description shared by active-inference plumbing.
// Extracted from SleepThread.h so logic/causality headers can reference
// yuki::brain::inference::Policy without pulling in the whole sleep stack.
#pragma once

#include <string>
#include <vector>

namespace yuki::brain::inference {

// Minimal policy description for counterfactual replay. The full policy
// engine (inference::Policy + FreeEnergyCalculator) was moved to not_in_use;
// counterfactual replay only needs parameter vectors and a label.
struct Policy {
    std::vector<float> parameters;
    std::string        description;
};

} // namespace yuki::brain::inference
