#pragma once
#include "PrecisionEngine.h"
#include "BeliefState.h"
#include "GenerativeModel.h"
#include "FreeEnergyCalculator.h"
#include "PolicySelector.h"
#include "input/encoding/SensoryObservation.h"
#include <memory>

namespace yuki::inference {
class VariationalStateEstimator {
public:
    VariationalStateEstimator();
    PolicyResult update(
        const yuki::perception::SensoryObservation& observation,
        const PrecisionFactors& factors);
    const BeliefState& currentBelief() const { return belief_state_; }
    BeliefState& mutableBelief() { return belief_state_; }  // P3 FIX: allow text-obs belief update
    // SleepThread: copy-in a saved belief (simulation only, not live inference)
    void setBeliefState(const BeliefState& s) { belief_state_ = s; }
    // P3 FIX: Bayes update of q_intent from 12-dim text observation
    void updateBeliefFromTextObs(const std::vector<float>& text_obs, float lr = 0.15f);
    const PolicyResult& lastPolicy() const { return last_policy_result_; }
    // Performance metrics
    float lastUpdateTimeMs() const { return last_update_time_ms_; }
    int lastEvalCount() const { return last_eval_count_; }
    void reset();
    void reportOutcome(const std::string& source_id, bool was_correct);

    // Persistence
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
private:
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
}
