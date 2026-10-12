#include "LatentMPCPlanner.h"
#include "brain/core/Logger.h"
#include <numeric>
#include <cmath>
#include <limits>

namespace yuki {
namespace brain {
namespace planning {

LatentMPCPlanner::LatentMPCPlanner(const LatentMPCConfig& cfg)
    : config_(cfg), rng_(std::random_device{}()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LatentMPCPlanner",
        "Initialized Latent MPC Planner (CEM: " + std::to_string(cfg.n_samples) + " samples, " +
        std::to_string(cfg.horizon) + " horizon, level " + std::to_string(cfg.planning_level) + ")");
}

MPCResult LatentMPCPlanner::plan(const PlanningContext& context) {
    if (!latent_world_model_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LatentMPCPlanner",
            "No LatentWorldModel set!");
        return {};
    }
    
    MPCResult result;
    int action_dim = 12; // Number of actions
    
    // Initialize action distribution
    std::vector<std::vector<float>> mean(config_.horizon, std::vector<float>(action_dim, 0.0f));
    std::vector<std::vector<float>> std(config_.horizon, std::vector<float>(action_dim, config_.init_variance));
    
    initDistribution(mean, std, context);
    
    // CEM iterations
    float prev_best_cost = std::numeric_limits<float>::infinity();
    
    for (int iter = 0; iter < config_.n_iterations; ++iter) {
        result.iterations_run = iter + 1;
        
        // Sample action sequences
        auto samples = sampleActionSequences(mean, std, config_.n_samples);
        
        // Evaluate each sample
        std::vector<float> costs(config_.n_samples);
        std::vector<predictive::LatentWorldModel::ImaginationResult> trajectories(config_.n_samples);
        
        for (int s = 0; s < config_.n_samples; ++s) {
            // Imagine trajectory in latent space
            trajectories[s] = latent_world_model_->imagine(
                context.current_rssm_states, samples[s], config_.horizon);
            
            // Evaluate cost
            costs[s] = evaluateTrajectory(trajectories[s], context);
        }
        
        // Find elite samples
        std::vector<int> indices(config_.n_samples);
        std::iota(indices.begin(), indices.end(), 0);
        std::partial_sort(indices.begin(), indices.begin() + config_.n_elite, indices.end(),
                         [&](int a, int b) { return costs[a] < costs[b]; });
        
        // Store elite costs
        result.elite_costs.resize(config_.n_elite);
        for (int i = 0; i < config_.n_elite; ++i) {
            result.elite_costs[i] = costs[indices[i]];
        }
        
        // Check convergence
        float best_cost = costs[indices[0]];
        if (std::abs(best_cost - prev_best_cost) < 1e-4f) {
            result.converged = true;
            break;
        }
        prev_best_cost = best_cost;
        
        // Update distribution
        updateDistribution(samples, costs, mean, std);
        
        // Decay variance
        for (auto& row : std) {
            for (float& v : row) {
                v = std::max(v * config_.variance_decay, config_.min_variance);
            }
        }
    }
    
    // Get best action sequence
    result.mean_actions = mean;
    result.std_actions = std;
    result.best_cost = prev_best_cost;
    
    // Generate final trajectory with mean actions
    result.predicted_trajectory = latent_world_model_->imagine(
        context.current_rssm_states, mean, config_.horizon);
    
    // Convert mean actions to result format
    result.action_sequence = mean;
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "LatentMPCPlanner",
        "Plan complete: cost=" + std::to_string(result.best_cost) + 
        ", iterations=" + std::to_string(result.iterations_run) +
        ", converged=" + (result.converged ? "true" : "false"));
    
    return result;
}

MPCResult LatentMPCPlanner::planWarmStart(const PlanningContext& context, const MPCResult& prev_result) {
    if (!config_.warm_start) {
        return plan(context);
    }
    
    MPCResult result;
    int action_dim = 12;
    
    // Warm-start from previous result
    std::vector<std::vector<float>> mean = prev_result.mean_actions;
    std::vector<std::vector<float>> std = prev_result.std_actions;
    
    // Shift actions forward (receding horizon)
    if (mean.size() == config_.horizon) {
        for (int i = 0; i < config_.horizon - 1; ++i) {
            mean[i] = mean[i + 1];
            std[i] = std[i + 1];
        }
        // Last step: random
        for (int j = 0; j < action_dim; ++j) {
            mean[config_.horizon - 1][j] = 0.0f;
            std[config_.horizon - 1][j] = config_.init_variance;
        }
    }
    
    // Mix with fresh initialization
    std::vector<std::vector<float>> fresh_mean(config_.horizon, std::vector<float>(action_dim, 0.0f));
    std::vector<std::vector<float>> fresh_std(config_.horizon, std::vector<float>(action_dim, config_.init_variance));
    initDistribution(fresh_mean, fresh_std, context);
    
    float mix = config_.warm_start_mix;
    for (int i = 0; i < config_.horizon; ++i) {
        for (int j = 0; j < action_dim; ++j) {
            mean[i][j] = mix * mean[i][j] + (1.0f - mix) * fresh_mean[i][j];
            std[i][j] = mix * std[i][j] + (1.0f - mix) * fresh_std[i][j];
        }
    }
    
    // Run CEM
    float prev_best_cost = prev_result.best_cost;
    
    for (int iter = 0; iter < config_.n_iterations; ++iter) {
        result.iterations_run = iter + 1;
        
        auto samples = sampleActionSequences(mean, std, config_.n_samples);
        
        std::vector<float> costs(config_.n_samples);
        for (int s = 0; s < config_.n_samples; ++s) {
            auto traj = latent_world_model_->imagine(context.current_rssm_states, samples[s], config_.horizon);
            costs[s] = evaluateTrajectory(traj, context);
        }
        
        std::vector<int> indices(config_.n_samples);
        std::iota(indices.begin(), indices.end(), 0);
        std::partial_sort(indices.begin(), indices.begin() + config_.n_elite, indices.end(),
                         [&](int a, int b) { return costs[a] < costs[b]; });
        
        result.elite_costs.resize(config_.n_elite);
        for (int i = 0; i < config_.n_elite; ++i) {
            result.elite_costs[i] = costs[indices[i]];
        }
        
        float best_cost = costs[indices[0]];
        if (std::abs(best_cost - prev_best_cost) < 1e-4f) {
            result.converged = true;
            break;
        }
        prev_best_cost = best_cost;
        
        updateDistribution(samples, costs, mean, std);
        
        for (auto& row : std) {
            for (float& v : row) {
                v = std::max(v * config_.variance_decay, config_.min_variance);
            }
        }
    }
    
    result.mean_actions = mean;
    result.std_actions = std;
    result.best_cost = prev_best_cost;
    result.predicted_trajectory = latent_world_model_->imagine(context.current_rssm_states, mean, config_.horizon);
    result.action_sequence = mean;
    
    return result;
}

MPCResult LatentMPCPlanner::replan(const PlanningContext& context, const MPCResult& prev_result, int steps_executed) {
    // For online MPC: we've executed some steps, replan from current state
    return planWarmStart(context, prev_result);
}

float LatentMPCPlanner::evaluateTrajectory(const predictive::LatentWorldModel::ImaginationResult& traj,
                                            const PlanningContext& context) const {
    float cost = 0.0f;
    int level = config_.planning_level;
    
    if (traj.trajectory.empty()) return std::numeric_limits<float>::infinity();
    
    // Goal cost
    if (!context.goal_state.empty()) {
        for (const auto& step : traj.trajectory) {
            const auto& state = step[level].deterministic;
            cost += config_.goal_weight * goalCost(state, context.goal_state);
        }
    }
    
    // Control cost
    if (!traj.predicted_obs.empty() && !traj.predicted_obs[0].empty()) {
        // Use action magnitude as control cost
        // This is simplified - would use actual actions
        cost += config_.control_weight * 0.01f * traj.trajectory.size();
    }
    
    // Smoothness cost
    cost += config_.smoothness_weight * smoothnessCost(traj.predicted_obs);
    
    // Safety cost
    for (const auto& step : traj.trajectory) {
        const auto& state = step[level].deterministic;
        cost += config_.safety_weight * safetyCost(state, context);
    }
    
    // Causal cost
    if (config_.use_causal_world_model && causal_world_model_) {
        cost += config_.causal_weight * causalCost(traj, context);
    }
    
    return cost;
}

std::vector<std::vector<std::vector<float>>> LatentMPCPlanner::sampleActionSequences(
    const std::vector<std::vector<float>>& mean,
    const std::vector<std::vector<float>>& std,
    int n_samples) const {
    
    int horizon = mean.size();
    int action_dim = mean[0].size();
    
    std::vector<std::vector<std::vector<float>>> samples(n_samples, 
        std::vector<std::vector<float>>(horizon, std::vector<float>(action_dim)));
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normal(0.0f, 1.0f);
    
    for (int s = 0; s < n_samples; ++s) {
        for (int t = 0; t < horizon; ++t) {
            for (int d = 0; d < action_dim; ++d) {
                float sample = mean[t][d] + std[t][d] * normal(rng_);
                samples[s][t][d] = sample;
            }
        }
    }
    
    // Clip to bounds
    for (auto& seq : samples) {
        clipActions(seq);
    }
    
    return samples;
}

void LatentMPCPlanner::updateDistribution(const std::vector<std::vector<std::vector<float>>>& samples,
                                           const std::vector<float>& costs,
                                           std::vector<std::vector<float>>& mean,
                                           std::vector<std::vector<float>>& std) const {
    int horizon = mean.size();
    int action_dim = mean[0].size();
    int n_elite = config_.n_elite;
    
    // Get elite indices
    std::vector<int> indices(samples.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::partial_sort(indices.begin(), indices.begin() + n_elite, indices.end(),
                     [&](int a, int b) { return costs[a] < costs[b]; });
    
    // Compute new mean and std from elite samples
    for (int t = 0; t < horizon; ++t) {
        for (int d = 0; d < action_dim; ++d) {
            float sum = 0.0f;
            for (int i = 0; i < n_elite; ++i) {
                sum += samples[indices[i]][t][d];
            }
            float new_mean = sum / n_elite;
            
            float var_sum = 0.0f;
            for (int i = 0; i < n_elite; ++i) {
                float diff = samples[indices[i]][t][d] - new_mean;
                var_sum += diff * diff;
            }
            float new_std = std::sqrt(var_sum / n_elite + 1e-8f);
            
            mean[t][d] = new_mean;
            std[t][d] = std::max(new_std, config_.min_variance);
        }
    }
}

void LatentMPCPlanner::initDistribution(std::vector<std::vector<float>>& mean,
                                         std::vector<std::vector<float>>& std,
                                         const PlanningContext& context) const {
    int action_dim = 12;
    
    // If we have a default goal, bias toward actions that achieve it
    if (!default_goal_.empty() && policy_engine_) {
        // Get heuristic policy for goal
        // Simplified: use policy engine to get initial action bias
        // This would use the policy engine's heuristic policies
    }
    
    // Initialize with small random actions
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
    for (int t = 0; t < config_.horizon; ++t) {
        for (int d = 0; d < action_dim; ++d) {
            mean[t][d] = dist(rng_);
            std[t][d] = config_.init_variance;
        }
    }
}

void LatentMPCPlanner::clipActions(std::vector<std::vector<float>>& actions) const {
    if (config_.action_bounds.empty()) return;
    
    int action_dim = config_.action_bounds.size();
    for (auto& step : actions) {
        for (int d = 0; d < action_dim && d < (int)step.size(); ++d) {
            step[d] = std::clamp(step[d], config_.action_bounds[d].first, config_.action_bounds[d].second);
        }
    }
}

float LatentMPCPlanner::goalCost(const std::vector<float>& state, const std::vector<float>& goal) const {
    float cost = 0.0f;
    int dim = std::min(state.size(), goal.size());
    for (int i = 0; i < dim; ++i) {
        float diff = state[i] - goal[i];
        cost += diff * diff;
    }
    return cost / std::max(1, dim);
}

float LatentMPCPlanner::safetyCost(const std::vector<float>& state, const PlanningContext& context) const {
    if (context.safety_constraints.empty()) return 0.0f;
    
    float cost = 0.0f;
    int dim = std::min(state.size(), context.safety_constraints.size());
    for (int i = 0; i < dim; ++i) {
        if (state[i] > context.safety_constraints[i]) {
            float violation = state[i] - context.safety_constraints[i];
            cost += violation * violation;
        }
    }
    return cost;
}

float LatentMPCPlanner::smoothnessCost(const std::vector<std::array<std::vector<float>, 5>>& obs_sequence) const {
    if (obs_sequence.size() < 2) return 0.0f;
    
    float cost = 0.0f;
    int level = config_.planning_level;
    
    for (size_t t = 1; t < obs_sequence.size(); ++t) {
        const auto& curr = obs_sequence[t][level];
        const auto& prev = obs_sequence[t-1][level];
        int dim = std::min(curr.size(), prev.size());
        for (int d = 0; d < dim; ++d) {
            float diff = curr[d] - prev[d];
            cost += diff * diff;
        }
    }
    
    return cost / (obs_sequence.size() * std::max(1, (int)obs_sequence[0][level].size()));
}

float LatentMPCPlanner::causalCost(const predictive::LatentWorldModel::ImaginationResult& traj,
                                    const PlanningContext& context) const {
    if (!causal_world_model_) return 0.0f;
    
    // Simplified: check if trajectory violates known causal constraints
    // In practice, would simulate counterfactuals and check for adverse effects
    float cost = 0.0f;
    
    // Penalize trajectories that lead to high uncertainty
    for (const auto& step : traj.trajectory) {
        int level = config_.planning_level;
        // Check stochastic uncertainty
        for (float var : step[level].stochastic) {
            cost += 0.01f * var * var;
        }
    }
    
    return cost;
}

} // namespace planning
} // namespace brain
} // namespace yuki