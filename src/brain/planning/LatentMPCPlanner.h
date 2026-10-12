#pragma once
// LatentMPCPlanner — Cross-Entropy Method in Latent Space
// Plans in the 5-level hierarchical latent space for efficient long-horizon planning

#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <functional>
#include "brain/predictive/LatentWorldModel.h"
#include "brain/causality/CausalWorldModel.h"
#include "brain/common/PlanningTypes.h"

// Forward declaration to break circular dependency
namespace yuki { namespace brain { namespace policy { class ActiveInferencePolicyEngine; } } }

namespace yuki {
namespace brain {
namespace planning {

class LatentMPCPlanner {
public:
    explicit LatentMPCPlanner(const LatentMPCConfig& cfg = LatentMPCConfig{});
    ~LatentMPCPlanner() = default;
    
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setCausalWorldModel(::yuki::brain::causality::CausalWorldModel* cwm) { causal_world_model_ = cwm; }
    void setPolicyEngine(yuki::brain::policy::ActiveInferencePolicyEngine* pe) { policy_engine_ = pe; }
    
    // Main planning function
    MPCResult plan(const PlanningContext& context);
    
    // Plan with warm-start from previous solution
    MPCResult planWarmStart(const PlanningContext& context, const MPCResult& prev_result);
    
    // Replan with updated context (for online MPC)
    MPCResult replan(const PlanningContext& context, const MPCResult& prev_result, int steps_executed);
    
    // Cost function for a trajectory
    float evaluateTrajectory(const predictive::LatentWorldModel::ImaginationResult& traj,
                             const PlanningContext& context) const;
    
    // Sample action sequences from Gaussian distribution
    std::vector<std::vector<std::vector<float>>> sampleActionSequences(
        const std::vector<std::vector<float>>& mean,
        const std::vector<std::vector<float>>& std,
        int n_samples) const;
    
    // Update CEM distribution from elite samples
    void updateDistribution(const std::vector<std::vector<std::vector<float>>>& samples,
                            const std::vector<float>& costs,
                            std::vector<std::vector<float>>& mean,
                            std::vector<std::vector<float>>& std) const;
    
    // Get action bounds
    void setActionBounds(const std::vector<std::pair<float, float>>& bounds) {
        config_.action_bounds = bounds;
    }
    
    // Set goal
    void setGoal(const std::vector<float>& goal, int level = 2) {
        default_goal_ = goal;
        default_goal_level_ = level;
    }
    
    // Get config
    const LatentMPCConfig& getConfig() const { return config_; }
    LatentMPCConfig& getConfig() { return config_; }

private:
    LatentMPCConfig config_;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    ::yuki::brain::causality::CausalWorldModel* causal_world_model_ = nullptr;
    yuki::brain::policy::ActiveInferencePolicyEngine* policy_engine_ = nullptr;
    
    // Default goal for warm-start
    std::vector<float> default_goal_;
    int default_goal_level_ = 2;
    
    // Random generator
    mutable std::mt19937 rng_;
    
    // Initialize action distribution
    void initDistribution(std::vector<std::vector<float>>& mean,
                          std::vector<std::vector<float>>& std,
                          const PlanningContext& context) const;
    
    // Apply action bounds
    void clipActions(std::vector<std::vector<float>>& actions) const;
    
    // Compute goal cost
    float goalCost(const std::vector<float>& state, const std::vector<float>& goal) const;
    
    // Compute safety cost
    float safetyCost(const std::vector<float>& state, const PlanningContext& context) const;
    
    // Compute smoothness cost
    float smoothnessCost(const std::vector<std::array<std::vector<float>, 5>>& obs_sequence) const;
    
    // Compute causal cost
    float causalCost(const predictive::LatentWorldModel::ImaginationResult& traj,
                     const PlanningContext& context) const;
};

} // namespace planning
} // namespace brain
} // namespace yuki