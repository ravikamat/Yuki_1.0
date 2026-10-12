#pragma once
// Shared planning types - included by both planning and policy namespaces

#include <vector>
#include <array>
#include <string>
#include "brain/predictive/LatentWorldModel.h"

namespace yuki {
namespace brain {
namespace planning {

struct LatentMPCConfig {
    int n_samples = 512;
    int n_elite = 64;
    int n_iterations = 10;
    int horizon = 20;
    float init_variance = 1.0f;
    float variance_decay = 0.9f;
    float min_variance = 0.01f;
    float goal_weight = 1.0f;
    float control_weight = 0.1f;
    float smoothness_weight = 0.05f;
    float safety_weight = 10.0f;
    int planning_level = 2;
    bool use_causal_world_model = true;
    float causal_weight = 0.5f;
    bool warm_start = true;
    float warm_start_mix = 0.3f;
    std::vector<std::pair<float, float>> action_bounds;
};

struct MPCResult {
    std::vector<std::vector<float>> action_sequence;
    std::vector<std::vector<float>> mean_actions;
    std::vector<std::vector<float>> std_actions;
    float best_cost = 0.0f;
    std::vector<float> elite_costs;
    int iterations_run = 0;
    bool converged = false;
    predictive::LatentWorldModel::ImaginationResult predicted_trajectory;
};

struct PlanningContext {
    std::array<predictive::RSSMState, 5> current_rssm_states;
    std::array<std::vector<float>, 5> current_observations;
    std::vector<float> goal_state;
    std::vector<float> goal_observation;
    int goal_level = 2;
    std::vector<float> safety_constraints;
    float safety_threshold = 0.5f;
    std::vector<float> belief_features;
};

} // namespace planning
} // namespace brain
} // namespace yuki