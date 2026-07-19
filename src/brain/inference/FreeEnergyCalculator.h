#pragma once
#include "BeliefState.h"
#include <vector>

namespace yuki::inference {

class GenerativeModel;

struct Policy {
    std::vector<float> parameters;
    std::string description;
    float responseLength() const { return parameters.empty() ? 0.0f : parameters[0]; }
    float tone() const { return parameters.size() < 2 ? 0.0f : parameters[1]; }
    float detailLevel() const { return parameters.size() < 3 ? 0.0f : parameters[2]; }
    float waitTime() const { return parameters.size() < 4 ? 0.0f : parameters[3]; }
    float proactivity() const { return parameters.size() < 5 ? 0.0f : parameters[4]; }
    float toolUse() const { return parameters.size() < 6 ? 0.0f : parameters[5]; }
    float verbosity() const { return parameters.size() < 7 ? 0.0f : parameters[6]; }
    float confidenceThreshold() const { return parameters.size() < 8 ? 0.0f : parameters[7]; }
};

class FreeEnergyCalculator {
public:
    FreeEnergyCalculator();
    float computeF(const BeliefState& belief,
                   const std::vector<float>& prediction_error,
                   const std::vector<float>& precision) const;
    float computeG(const Policy& policy,
                   const BeliefState& current_belief,
                   const GenerativeModel& model) const;
    std::vector<float> policyGradient(const Policy& policy,
                                        const BeliefState& current_belief,
                                        const GenerativeModel& model,
                                        float epsilon = 1e-3f) const;
    // Select policy by gradient descent on G(π)
    // Optimized: caches G values, supports early termination, adaptive learning rate
    Policy optimizePolicy(const std::vector<Policy>& initial_candidates,
                          const BeliefState& current_belief,
                          const GenerativeModel& model,
                          size_t max_iterations = 50,
                          float learning_rate = 0.05f) const;

    // Fast path: if belief hasn't changed much, return cached policy
    bool hasCachedPolicy(const BeliefState& current_belief) const;
    Policy getCachedPolicy() const;
    void invalidateCache();
private:
    float simulateExpectedF_(const Policy& policy,
                             const BeliefState& belief,
                             const GenerativeModel& model) const;
    float finiteDifferenceG_(const Policy& policy,
                            const BeliefState& belief,
                            const GenerativeModel& model,
                            size_t param_idx,
                            float epsilon) const;
};
}
