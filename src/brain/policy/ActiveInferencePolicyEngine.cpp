#include "brain/policy/ActiveInferencePolicyEngine.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/core/Logger.h"
#include "brain/planning/LatentMPCPlanner.h"
#include "brain/predictive/LatentWorldModel.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace yuki {
namespace brain {
namespace policy {

ActiveInferencePolicyEngine::ActiveInferencePolicyEngine(const PolicyEngineConfig& cfg)
    : config_(cfg), last_replan_(std::chrono::steady_clock::now()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ActiveInferencePolicyEngine", 
        "Initialized with MPC: " + std::string(config_.use_mpc ? "enabled" : "disabled"));
}

PolicyResult ActiveInferencePolicyEngine::selectPolicy(const field::CognitiveSnapshot& snap) {
    auto start = std::chrono::steady_clock::now();
    PolicyResult result;

    // Check if we should use MPC slow path
    bool use_mpc = config_.use_mpc && shouldUseMPC(snap);
    
    if (use_mpc && mpc_planner_ && latent_world_model_) {
        // Slow path: MPC planning in latent space
        Policy mpc_policy = runMPCPlanner(snap);
        current_policy_ = mpc_policy;
        result.used_mpc = true;
        result.rationale = current_policy_.description;
    } else {
        // Fast path: heuristic + random policies
        bool replan = policy_cache_.empty();
        if (!replan) {
            replan = shouldReplan(snap);
        }
        if (replan) {
            generatePolicies(snap);
            last_replan_ = std::chrono::steady_clock::now();
            result.re_planned = true;
        }

        // Evaluate all policies
        for (auto& policy : policy_cache_) {
            policy.efe = evaluatePolicy(policy, snap);
        }

        // Select via softmax over -EFE
        const int selected_idx = selectPolicySoftmax(policy_cache_);
        if (selected_idx >= 0 && selected_idx < static_cast<int>(policy_cache_.size())) {
            current_policy_ = policy_cache_[static_cast<size_t>(selected_idx)];
        } else {
            current_policy_ = Policy{};
            current_policy_.actions = {PolicyEngineConfig::ACTION_WAIT};
            current_policy_.description = "Fallback: wait";
        }

        result.used_mpc = false;
        result.rationale = current_policy_.description;
    }

    result.selected_policy = result.used_mpc ? -2 : 0;  // -2 indicates MPC
    result.selected_action = current_policy_.actions.empty()
                                 ? PolicyEngineConfig::ACTION_NONE
                                 : current_policy_.actions.front();
    result.selected_efe = current_policy_.efe;
    result.evaluated_policies = policy_cache_;

    auto end = std::chrono::steady_clock::now();
    result.compute_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return result;
}

bool ActiveInferencePolicyEngine::shouldUseMPC(const field::CognitiveSnapshot& snap) const {
    // Trigger MPC when surprise is high or precision is low
    if (snap.total_surprise > config_.mpc_surprise_threshold) return true;
    if (snap.precision.intent < config_.mpc_precision_threshold) return true;
    if (snap.precision.entity < config_.mpc_precision_threshold) return true;
    if (snap.precision.tone < config_.mpc_precision_threshold) return true;
    
    // Also trigger on high free energy
    if (snap.vse.free_energy > 2.0f) return true;
    
    // Trigger on active curiosity drive with low competence
    if (snap.drives.curiosity > 0.7f && snap.vse.entropy > 0.8f) return true;
    
    return false;
}

Policy ActiveInferencePolicyEngine::runMPCPlanner(const field::CognitiveSnapshot& snap) {
    Policy policy;
    policy.is_mpc = true;
    
    if (!mpc_planner_ || !latent_world_model_) {
        policy.actions = {PolicyEngineConfig::ACTION_WAIT};
        policy.description = "MPC unavailable, fallback to wait";
        policy.efe = std::numeric_limits<float>::infinity();
        return policy;
    }
    
    auto context = buildMPCContext(snap);
    
    // Use warm-start if available
    planning::MPCResult mpc_result;
    if (config_.mpc_warm_start && !last_mpc_result_.action_sequence.empty()) {
        mpc_result = mpc_planner_->planWarmStart(context, last_mpc_result_);
    } else {
        mpc_result = mpc_planner_->plan(context);
    }
    
    last_mpc_result_ = mpc_result;
    
    // Convert MPC action sequence to policy
    if (!mpc_result.action_sequence.empty()) {
        // Take first action from sequence
        const auto& first_action = mpc_result.action_sequence[0];
        
        // Convert continuous action to discrete action
        int best_action = 0;
        float max_val = first_action[0];
        for (size_t i = 1; i < first_action.size(); ++i) {
            if (first_action[i] > max_val) {
                max_val = first_action[i];
                best_action = static_cast<int>(i);
            }
        }
        
        // Ensure action is in valid range
        best_action = std::clamp(best_action, 0, PolicyEngineConfig::NUM_ACTIONS - 1);
        policy.actions = {best_action};
        
        // Use negative cost as EFE (lower cost = better)
        policy.efe = mpc_result.best_cost;
        policy.description = "MPC: " + actionToString(best_action) + 
                            " (cost=" + std::to_string(mpc_result.best_cost) + 
                            ", iter=" + std::to_string(mpc_result.iterations_run) + ")";
    } else {
        policy.actions = {PolicyEngineConfig::ACTION_WAIT};
        policy.description = "MPC failed, fallback to wait";
        policy.efe = std::numeric_limits<float>::infinity();
    }
    
    return policy;
}

planning::PlanningContext ActiveInferencePolicyEngine::buildMPCContext(
    const field::CognitiveSnapshot& snap) const {
    
    planning::PlanningContext context;
    
    // Current RSSM states (simplified - would come from latent world model)
    if (latent_world_model_) {
        context.current_rssm_states = latent_world_model_->getRSSMStates();
    }
    
    // Current observations per level
    // Map VSE belief to observations
    for (int l = 0; l < 5; ++l) {
        context.current_observations[l].assign(128, 0.0f);  // Placeholder
    }
    
    // Goal from drives and intent
    context.goal_level = config_.mpc_planning_level;
    context.goal_state.assign(128, 0.0f);
    
    // Set goal based on active drives
    if (snap.drives.curiosity > 0.5f) {
        // Curiosity goal: maximize information gain
        context.goal_state[0] = 1.0f;
    }
    if (snap.drives.social > 0.5f) {
        // Social goal: empathize
        context.goal_state[1] = 1.0f;
    }
    if (snap.precision.intent < 0.4f) {
        // Clarification goal
        context.goal_state[2] = 1.0f;
    }
    
    // Safety constraints
    context.safety_constraints.assign(128, 1.0f);  // Upper bounds
    context.safety_threshold = 0.5f;
    
    // Belief features
    context.belief_features.assign(24, 0.0f);
    for (int i = 0; i < 8 && i < (int)snap.vse.q_intent.size(); ++i) {
        context.belief_features[i] = snap.vse.q_intent[i];
    }
    context.belief_features[8] = snap.vse.safety_mass;
    context.belief_features[9] = snap.vse.surprise_budget;
    context.belief_features[10] = snap.vse.entropy;
    context.belief_features[11] = snap.drives.curiosity;
    context.belief_features[12] = snap.drives.social;
    context.belief_features[13] = snap.drives.energy;
    
    return context;
}

void ActiveInferencePolicyEngine::step(const field::CognitiveSnapshot& snap) {
    if (shouldReplan(snap)) {
        selectPolicy(snap);
    }
}

bool ActiveInferencePolicyEngine::shouldReplan(const field::CognitiveSnapshot& snap) const {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_replan_ < config_.min_replan_interval) return false;

    // Re-plan when aggregate surprise exceeds threshold
    if (snap.total_surprise > config_.surprise_replan_threshold) return true;

    // Re-plan when the current policy is exhausted
    if (current_policy_.actions.empty()) return true;

    // Re-plan when precision drops significantly
    if (snap.precision.intent < config_.precision_drop_threshold) return true;
    if (snap.precision.entity < config_.precision_drop_threshold) return true;

    return false;
}

void ActiveInferencePolicyEngine::generatePolicies(const field::CognitiveSnapshot& snap) {
    policy_cache_.clear();
    policy_cache_.reserve(static_cast<size_t>(config_.num_policies));

    int heuristic_count = 0;
    if (config_.use_heuristic_policies) {
        const int n = std::min(config_.num_heuristic, config_.num_policies);
        for (int type = 0; type < n; ++type) {
            policy_cache_.push_back(generateHeuristicPolicy(snap, type));
        }
        heuristic_count = n;
    }

    if (config_.use_random_policies) {
        std::mt19937 rng(static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<int> action_dist(PolicyEngineConfig::ACTION_NONE,
                                                       PolicyEngineConfig::NUM_ACTIONS - 1);
        std::uniform_int_distribution<int> len_dist(1, 3);
        const int n = std::min(config_.num_random,
                               config_.num_policies - heuristic_count);
        for (int i = 0; i < n; ++i) {
            policy_cache_.push_back(generateRandomPolicy(snap));
            (void)action_dist; (void)len_dist; (void)rng;
        }
    }

    if (policy_cache_.empty()) {
        Policy wait;
        wait.actions = {PolicyEngineConfig::ACTION_WAIT};
        wait.description = "No-op: wait";
        policy_cache_.push_back(std::move(wait));
    }
}

Policy ActiveInferencePolicyEngine::generateHeuristicPolicy(const field::CognitiveSnapshot& snap, int type) {
    Policy policy;
    policy.actions.reserve(2);
    policy.is_mpc = false;

    switch (type % 8) {
        case 0:  // Low intent precision -> clarify intent
            if (snap.precision.intent < 0.5f) {
                policy.actions = {PolicyEngineConfig::ACTION_CLARIFY_INTENT};
                policy.description = "Heuristic: clarify ambiguous intent";
            }
            break;
        case 1:  // Low entity precision -> clarify entity
            if (snap.precision.entity < 0.5f) {
                policy.actions = {PolicyEngineConfig::ACTION_CLARIFY_ENTITY};
                policy.description = "Heuristic: clarify missing entity";
            }
            break;
        case 2:  // Low tone precision -> clarify tone
            if (snap.precision.tone < 0.5f) {
                policy.actions = {PolicyEngineConfig::ACTION_CLARIFY_TONE};
                policy.description = "Heuristic: clarify emotional tone";
            }
            break;
        case 3:  // High intent precision + question-like input -> answer
            if (snap.precision.intent >= 0.5f) {
                policy.actions = {PolicyEngineConfig::ACTION_ANSWER_QUERY};
                policy.description = "Heuristic: answer query";
            }
            break;
        case 4:  // Command-like input -> execute
            if (snap.precision.source >= 0.5f) {
                policy.actions = {PolicyEngineConfig::ACTION_EXECUTE_COMMAND};
                policy.description = "Heuristic: execute command";
            }
            break;
        case 5:  // Social drive active -> empathize
            if (snap.drives.social > 0.6f) {
                policy.actions = {PolicyEngineConfig::ACTION_EMPATHIZE};
                policy.description = "Heuristic: empathize (social drive)";
            }
            break;
        case 6:  // Curiosity active -> research
            if (snap.drives.curiosity > 0.6f) {
                policy.actions = {PolicyEngineConfig::ACTION_RESEARCH};
                policy.description = "Heuristic: research (curiosity drive)";
            }
            break;
        case 7:  // Low energy -> wait
            if (snap.drives.energy < 0.4f) {
                policy.actions = {PolicyEngineConfig::ACTION_WAIT};
                policy.description = "Heuristic: conserve energy";
            }
            break;
        default:
            break;
    }

    if (policy.actions.empty()) {
        policy.actions = {PolicyEngineConfig::ACTION_WAIT};
        policy.description = actionToString(PolicyEngineConfig::ACTION_WAIT);
    }
    return policy;
}

Policy ActiveInferencePolicyEngine::generateRandomPolicy(const field::CognitiveSnapshot& snap) {
    (void)snap;
    static thread_local std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()) ^
        static_cast<unsigned>(policy_cache_.size()));
    std::uniform_int_distribution<int> action_dist(PolicyEngineConfig::ACTION_CLARIFY_INTENT,
                                                   PolicyEngineConfig::NUM_ACTIONS - 1);
    std::uniform_int_distribution<int> len_dist(1, 3);

    Policy policy;
    policy.is_mpc = false;
    const int len = len_dist(rng);
    policy.actions.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) {
        policy.actions.push_back(action_dist(rng));
    }
    policy.description = "Random policy (" + std::to_string(len) + " steps)";
    return policy;
}

float ActiveInferencePolicyEngine::evaluatePolicy(const Policy& policy, const field::CognitiveSnapshot& snap) const {
    // Expected free energy:
    //   pragmatic term  - low free energy in the current belief state (goal)
    //   epistemic term  - information gain from resolving uncertain state
    float efe = 0.0f;

    // Cost per action (deeper policies cost more)
    efe += 0.1f * static_cast<float>(policy.actions.size());

    // Pragmatic value: how far the current belief is from a low-energy state
    efe += 0.5f * snap.vse.free_energy;

    // Epistemic value: acting to resolve high entropy / surprise is rewarded
    efe -= 0.3f * snap.vse.entropy;
    efe -= 0.2f * snap.vse.surprise_budget;

    // Drive alignment: rewarding actions that satisfy active drives
    if (!policy.actions.empty()) {
        const int a = policy.actions.front();
        if (a == PolicyEngineConfig::ACTION_EMPATHIZE)  efe -= 0.2f * snap.drives.social;
        if (a == PolicyEngineConfig::ACTION_RESEARCH)   efe -= 0.2f * snap.drives.curiosity;
        if (a == PolicyEngineConfig::ACTION_ANSWER_QUERY && snap.precision.intent > 0.7f) efe -= 0.2f;
        if (a == PolicyEngineConfig::ACTION_CLARIFY_INTENT && snap.precision.intent < 0.4f) efe -= 0.3f;
    }

    // Safety penalty
    efe += 0.5f * (1.0f - snap.vse.safety_mass);

    // MPC policies get a small bonus for being more informed
    if (policy.is_mpc) {
        efe -= 0.1f;
    }

    return efe;
}

int ActiveInferencePolicyEngine::selectPolicySoftmax(const std::vector<Policy>& policies) const {
    if (policies.empty()) return -1;

    // Softmax over -EFE (lower EFE is better)
    std::vector<float> logits;
    logits.reserve(policies.size());
    for (const auto& p : policies) {
        logits.push_back(-p.efe / std::max(config_.efe_temperature, 1e-3f));
    }

    // Numerically stable softmax
    const float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (auto& l : logits) {
        l = std::exp(l - max_logit);
        sum += l;
    }
    if (sum <= 0.0f) return -1;

    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    static thread_local std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    const float r = uniform(rng);

    float cumulative = 0.0f;
    for (size_t i = 0; i < logits.size(); ++i) {
        cumulative += logits[i] / sum;
        if (r <= cumulative) return static_cast<int>(i);
    }
    return static_cast<int>(policies.size()) - 1;
}

std::string ActiveInferencePolicyEngine::actionToString(int action) {
    switch (action) {
        case PolicyEngineConfig::ACTION_NONE:            return "NONE";
        case PolicyEngineConfig::ACTION_CLARIFY_INTENT:  return "CLARIFY_INTENT";
        case PolicyEngineConfig::ACTION_CLARIFY_ENTITY:  return "CLARIFY_ENTITY";
        case PolicyEngineConfig::ACTION_CLARIFY_TONE:    return "CLARIFY_TONE";
        case PolicyEngineConfig::ACTION_ANSWER_QUERY:    return "ANSWER_QUERY";
        case PolicyEngineConfig::ACTION_EXECUTE_COMMAND: return "EXECUTE_COMMAND";
        case PolicyEngineConfig::ACTION_EMPATHIZE:       return "EMPATHIZE";
        case PolicyEngineConfig::ACTION_RESEARCH:        return "RESEARCH";
        case PolicyEngineConfig::ACTION_USE_TOOL:        return "USE_TOOL";
        case PolicyEngineConfig::ACTION_DELEGATE:        return "DELEGATE";
        case PolicyEngineConfig::ACTION_WAIT:            return "WAIT";
        case PolicyEngineConfig::ACTION_ABORT:           return "ABORT";
        default:                                          return "UNKNOWN";
    }
}

void ActiveInferencePolicyEngine::addGoal(const std::string& description, float priority) {
    Goal goal;
    goal.description = description;
    goal.priority = priority;
    goal.created = std::chrono::steady_clock::now();
    active_goals_.push_back(std::move(goal));
}

void ActiveInferencePolicyEngine::completeGoal(const std::string& description) {
    for (auto& goal : active_goals_) {
        if (goal.description == description) {
            goal.progress = 1.0f;
            break;
        }
    }
}

} // namespace policy
} // namespace brain
} // namespace yuki