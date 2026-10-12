#pragma once

#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/field/CognitiveSnapshot.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/common/PlanningTypes.h"

// Forward declaration to break circular dependency
namespace yuki { namespace brain { namespace planning { class LatentMPCPlanner; } } }

namespace yuki {
namespace brain {
namespace policy {

struct Policy {
    std::vector<int> actions;
    float efe = 0.0f;
    float prior = 1.0f;
    std::string description;
    bool is_mpc = false;
};

struct PolicyEngineConfig {
    int planning_horizon = 5;
    int num_policies = 32;
    float efe_temperature = 1.0f;
    
    static constexpr int NUM_ACTIONS = 12;
    static constexpr int ACTION_NONE = 0;
    static constexpr int ACTION_CLARIFY_INTENT = 1;
    static constexpr int ACTION_CLARIFY_ENTITY = 2;
    static constexpr int ACTION_CLARIFY_TONE = 3;
    static constexpr int ACTION_ANSWER_QUERY = 4;
    static constexpr int ACTION_EXECUTE_COMMAND = 5;
    static constexpr int ACTION_EMPATHIZE = 6;
    static constexpr int ACTION_RESEARCH = 7;
    static constexpr int ACTION_USE_TOOL = 8;
    static constexpr int ACTION_DELEGATE = 9;
    static constexpr int ACTION_WAIT = 10;
    static constexpr int ACTION_ABORT = 11;
    
    bool use_heuristic_policies = true;
    bool use_random_policies = true;
    int num_heuristic = 8;
    int num_random = 8;
    float surprise_replan_threshold = 2.0f;
    float precision_drop_threshold = 0.3f;
    std::chrono::milliseconds min_replan_interval{500};
    
    bool use_mpc = true;
    float mpc_surprise_threshold = 1.5f;
    float mpc_precision_threshold = 0.4f;
    int mpc_planning_level = 2;
    int mpc_max_compute_time_ms = 50;
    bool mpc_warm_start = true;
};

struct PolicyResult {
    int selected_policy = -1;
    int selected_action = PolicyEngineConfig::ACTION_NONE;
    float selected_efe = 0.0f;
    std::vector<Policy> evaluated_policies;
    bool re_planned = false;
    bool used_mpc = false;
    std::string rationale;
    std::chrono::nanoseconds compute_time;
    yuki::brain::planning::MPCResult mpc_result;
};

class ActiveInferencePolicyEngine {
public:
    explicit ActiveInferencePolicyEngine(const PolicyEngineConfig& cfg = PolicyEngineConfig{});
    ~ActiveInferencePolicyEngine() = default;
    
    void setGenerativeModel(inference::GenerativeModel* gm) { generative_model_ = gm; }
    void setVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setLatentMPCPlanner(yuki::brain::planning::LatentMPCPlanner* mpc) { mpc_planner_ = mpc; }
    void setConfig(const PolicyEngineConfig& cfg) { config_ = cfg; }
    
    PolicyResult selectPolicy(const field::CognitiveSnapshot& snap);
    void step(const field::CognitiveSnapshot& snap);
    
    const Policy& getCurrentPolicy() const { return current_policy_; }
    int getSelectedAction() const { return current_policy_.actions.empty() ? PolicyEngineConfig::ACTION_NONE : current_policy_.actions[0]; }
    int getActiveGoalCount() const { return static_cast<int>(active_goals_.size()); }
    void addGoal(const std::string& description, float priority = 1.0f);
    void completeGoal(const std::string& description);

private:
    PolicyEngineConfig config_;
    inference::GenerativeModel* generative_model_ = nullptr;
    inference::VariationalStateEstimator* vse_ = nullptr;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    yuki::brain::planning::LatentMPCPlanner* mpc_planner_ = nullptr;
    Policy current_policy_;
    std::vector<Policy> policy_cache_;
    std::chrono::steady_clock::time_point last_replan_;
    yuki::brain::planning::MPCResult last_mpc_result_;
    
    struct Goal {
        std::string description;
        float priority = 1.0f;
        float progress = 0.0f;
        std::chrono::steady_clock::time_point created;
    };
    std::vector<Goal> active_goals_;
    
    void generatePolicies(const field::CognitiveSnapshot& snap);
    bool shouldReplan(const field::CognitiveSnapshot& snap) const;
    bool shouldUseMPC(const field::CognitiveSnapshot& snap) const;
    Policy generateHeuristicPolicy(const field::CognitiveSnapshot& snap, int type);
    Policy generateRandomPolicy(const field::CognitiveSnapshot& snap);
    float evaluatePolicy(const Policy& policy, const field::CognitiveSnapshot& snap) const;
    int selectPolicySoftmax(const std::vector<Policy>& policies) const;
    static std::string actionToString(int action);
    
    Policy runMPCPlanner(const field::CognitiveSnapshot& snap);
    yuki::brain::planning::PlanningContext buildMPCContext(const field::CognitiveSnapshot& snap) const;
};

} // namespace policy
} // namespace brain
} // namespace yuki