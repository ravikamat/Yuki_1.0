#pragma once
#include "FreeEnergyCalculator.h"
#include "BeliefState.h"
#include <vector>
#include <functional>
#include <string>

namespace yuki::inference {

class GenerativeModel;
class FreeEnergyCalculator;

struct PolicyResult {
    Policy selected_policy;
    float final_G = 0.0f;
    float initial_G = 0.0f;
    size_t optimization_steps = 0;
    std::string execution_plan;
};

class PolicySelector {
public:
    PolicySelector();
    PolicyResult selectPolicy(
        const BeliefState& current_belief,
        const GenerativeModel& model,
        const FreeEnergyCalculator& calculator);
    std::vector<Policy> generateSeedPolicies(const BeliefState& belief) const;
    using ConstraintFn = std::function<bool(const Policy&, const BeliefState&)>;
    void addConstraint(ConstraintFn constraint);
    bool isPolicyValid(const Policy& policy, const BeliefState& belief) const;
    PolicyResult getLastResult() const { return last_result_; }
private:
    std::vector<ConstraintFn> constraints_;
    PolicyResult last_result_;
    static constexpr size_t NUM_SEED_TEMPLATES = 6;
    static const float SEED_TEMPLATES[NUM_SEED_TEMPLATES][8];
};
}
