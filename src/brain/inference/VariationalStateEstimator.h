#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "PrecisionEngine.h"
#include "BeliefState.h"
#include "GenerativeModel.h"
#include "input/encoding/SensoryObservation.h"
#include "PrecisionPredictor.h"

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

struct PolicyResult {
    Policy selected_policy;
    float final_G = 0.0f;
    float initial_G = 0.0f;
    size_t optimization_steps = 0;
    std::string execution_plan;
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
    Policy optimizePolicy(const std::vector<Policy>& initial_candidates,
                          const BeliefState& current_belief,
                          const GenerativeModel& model,
                          size_t max_iterations = 50,
                          float learning_rate = 0.05f) const;
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

struct RiskSignalVector {
    float executionRisk = 0.0f;
    float pathRisk = 0.0f;
    float rateRisk = 0.0f;
    float uncertaintyRisk = 0.0f;

    float aggregate() const {
        return (std::max)({executionRisk, pathRisk, rateRisk, uncertaintyRisk});
    }
};

class VariationalStateEstimator {
public:
    VariationalStateEstimator();
    RiskSignalVector extractRiskSignals(const std::string& text) const;
    PolicyResult update(
        const yuki::perception::SensoryObservation& observation,
        const PrecisionFactors& factors);
    const BeliefState& currentBelief() const { return belief_state_; }
    BeliefState& mutableBelief() { return belief_state_; }
    void setBeliefState(const BeliefState& s) { belief_state_ = s; }
    float updateBeliefFromTextObs(const std::vector<float>& text_obs, float lr = 0.15f,
                                 const std::string& raw_text = "",
                                 const std::string& prev_raw_text = "",
                                 const std::vector<float>& intent_scores = {});
    void trainPrecision(float predicted, float target,
                        const std::string& raw_text,
                        const std::string& prev_raw_text = "",
                        const std::vector<float>& intent_scores = {});
    const PolicyResult& lastPolicy() const { return last_policy_result_; }
    float lastUpdateTimeMs() const { return last_update_time_ms_; }
    int lastEvalCount() const { return last_eval_count_; }
    void reset();
    void reportOutcome(const std::string& source_id, bool was_correct);

    bool saveGenerativeModel(const std::string& path = "generative_model_v1") {
        return generative_model_.saveMappings(path);
    }
    bool loadGenerativeModel(const std::string& path = "generative_model_v1") {
        return generative_model_.loadMappings(path);
    }
    PrecisionEngine& precisionEngine() { return precision_engine_; }
    GenerativeModel& generativeModel() { return generative_model_; }
    FreeEnergyCalculator& freeEnergyCalculator() { return free_energy_calc_; }
    PolicySelector& policySelector() { return policy_selector_; }
    PrecisionPredictor* precisionPredictor() { return precisionPredictor_.get(); }
private:
    std::unique_ptr<PrecisionPredictor> precisionPredictor_;

    PrecisionEngine precision_engine_;
    GenerativeModel generative_model_;
    BeliefState belief_state_;
    BeliefState prior_belief_;
    FreeEnergyCalculator free_energy_calc_;
    PolicySelector policy_selector_;
    PolicyResult last_policy_result_;
    float last_update_time_ms_ = 0.0f;
    int last_eval_count_ = 0;
    PrecisionFactors extractFactors_(const yuki::perception::SensoryObservation& obs) const;
};

} // namespace yuki::inference
