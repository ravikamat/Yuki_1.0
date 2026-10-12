#include "CausalWorldModel.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace yuki {
namespace brain {
namespace causality {

using Intervention = yuki::causality::Intervention;
using InterventionPolicy = yuki::causality::InterventionPolicy;
using CausalNode = yuki::causality::CausalNode;
using GenerativeBeliefState = yuki::brain::inference::GenerativeBeliefState;

// ─── CausalTransition ────────────────────────────────────────────────────────

CausalTransition::CausalTransition(uint32_t from, uint32_t to, size_t from_dim, size_t to_dim, size_t hidden_dim)
    : from_node(from), to_node(to) {
    
    std::mt19937 rng(42 + from * 1000 + to);
    auto init_weight = [&](size_t rows, size_t cols) {
        std::vector<float> W(rows * cols);
        float scale = std::sqrt(2.0f / (rows + cols));
        std::uniform_real_distribution<float> dist(-scale, scale);
        for (float& v : W) v = dist(rng);
        return W;
    };
    
    // Linear transition
    W = init_weight(to_dim, from_dim);
    b.assign(to_dim, 0.0f);
    
    // Non-linear (MLP)
    W1 = init_weight(hidden_dim, from_dim);
    b1.assign(hidden_dim, 0.0f);
    W2 = init_weight(to_dim, hidden_dim);
    b2.assign(to_dim, 0.0f);
}

std::vector<float> CausalTransition::predict(const std::vector<float>& state) const {
    size_t to_dim = b.size();
    size_t from_dim = state.size();
    
    // Linear prediction
    std::vector<float> linear(to_dim, 0.0f);
    for (size_t i = 0; i < to_dim; ++i) {
        float sum = b[i];
        for (size_t j = 0; j < from_dim && j < W.size() / to_dim; ++j) {
            sum += W[i * from_dim + j] * state[j];
        }
        linear[i] = sum;
    }
    
    // Non-linear prediction (MLP)
    size_t hidden_dim = b1.size();
    std::vector<float> hidden(hidden_dim, 0.0f);
    for (size_t i = 0; i < hidden_dim; ++i) {
        float sum = b1[i];
        for (size_t j = 0; j < from_dim; ++j) {
            sum += W1[i * from_dim + j] * state[j];
        }
        hidden[i] = std::tanh(sum);
    }
    
    std::vector<float> nonlinear(to_dim, 0.0f);
    for (size_t i = 0; i < to_dim; ++i) {
        float sum = b2[i];
        for (size_t j = 0; j < hidden_dim; ++j) {
            sum += W2[i * hidden_dim + j] * hidden[j];
        }
        nonlinear[i] = sum;
    }
    
    // Blend linear and non-linear
    std::vector<float> result(to_dim);
    for (size_t i = 0; i < to_dim; ++i) {
        result[i] = 0.5f * linear[i] + 0.5f * nonlinear[i];
    }
    
    return result;
}

void CausalTransition::update(const std::vector<float>& from_state, const std::vector<float>& to_state, float lr) {
    size_t to_dim = b.size();
    size_t from_dim = from_state.size();
    
    // Compute prediction
    auto pred = predict(from_state);
    
    // Compute error
    std::vector<float> error(to_dim);
    float loss = 0.0f;
    for (size_t i = 0; i < to_dim; ++i) {
        error[i] = to_state[i] - pred[i];
        loss += error[i] * error[i];
    }
    loss /= to_dim;
    
    running_loss = 0.9f * running_loss + 0.1f * loss;
    observation_count++;
    confidence = std::min(1.0f, confidence + lr * 0.1f);
    
    // Update linear weights
    for (size_t i = 0; i < to_dim; ++i) {
        b[i] += lr * error[i];
        for (size_t j = 0; j < from_dim; ++j) {
            W[i * from_dim + j] += lr * error[i] * from_state[j];
        }
    }
    
    // Update non-linear weights (simplified: only output layer)
    size_t hidden_dim = b1.size();
    std::vector<float> hidden(hidden_dim, 0.0f);
    for (size_t i = 0; i < hidden_dim; ++i) {
        float sum = b1[i];
        for (size_t j = 0; j < from_dim; ++j) {
            sum += W1[i * from_dim + j] * from_state[j];
        }
        hidden[i] = std::tanh(sum);
    }
    
    for (size_t i = 0; i < to_dim; ++i) {
        b2[i] += lr * error[i];
        for (size_t j = 0; j < hidden_dim; ++j) {
            W2[i * hidden_dim + j] += lr * error[i] * hidden[j];
        }
    }
}

void CausalTransition::updateMomentum(const CausalTransition& other, float momentum) {
    // Update momentum buffer
    for (size_t i = 0; i < W.size(); ++i) {
        W[i] = momentum * W[i] + (1.0f - momentum) * other.W[i];
    }
    for (size_t i = 0; i < b.size(); ++i) {
        b[i] = momentum * b[i] + (1.0f - momentum) * other.b[i];
    }
    for (size_t i = 0; i < W1.size(); ++i) {
        W1[i] = momentum * W1[i] + (1.0f - momentum) * other.W1[i];
    }
    for (size_t i = 0; i < b1.size(); ++i) {
        b1[i] = momentum * b1[i] + (1.0f - momentum) * other.b1[i];
    }
    for (size_t i = 0; i < W2.size(); ++i) {
        W2[i] = momentum * W2[i] + (1.0f - momentum) * other.W2[i];
    }
    for (size_t i = 0; i < b2.size(); ++i) {
        b2[i] = momentum * b2[i] + (1.0f - momentum) * other.b2[i];
    }
}

std::string CausalTransition::serialize() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << from_node << ';' << to_node << ';'
        << confidence << ';' << observation_count << ';' << running_loss << ';';
    
    auto write_vec = [&](const std::vector<float>& v) {
        for (float val : v) oss << val << ';';
    };
    write_vec(W); write_vec(b); write_vec(W1); write_vec(b1); write_vec(W2); write_vec(b2);
    return oss.str();
}

bool CausalTransition::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    
    if (!std::getline(iss, token, ';')) return false; from_node = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; to_node = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; confidence = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; observation_count = std::stoi(token);
    if (!std::getline(iss, token, ';')) return false; running_loss = std::stof(token);
    
    auto read_vec = [&](std::vector<float>& v) {
        for (float& val : v) {
            if (!std::getline(iss, token, ';')) return false;
            val = std::stof(token);
        }
        return true;
    };
    
    return read_vec(W) && read_vec(b) && read_vec(W1) && read_vec(b1) && read_vec(W2) && read_vec(b2);
}

// ─── InterventionEffect ──────────────────────────────────────────────────────

InterventionEffect::InterventionEffect(uint32_t target, uint32_t interv, bool value, size_t target_dim)
    : target_node(target), intervention_node(interv), intervention_value(value) {
    effect_mean.assign(target_dim, 0.0f);
    effect_cov.assign(target_dim * target_dim, 0.0f);
    // Initialize cov as identity
    for (size_t i = 0; i < target_dim; ++i) {
        effect_cov[i * target_dim + i] = 1.0f;
    }
}

void InterventionEffect::update(const std::vector<float>& observed_effect) {
    size_t dim = effect_mean.size();
    if (observed_effect.size() != dim) return;
    
    sample_count++;
    float alpha = 1.0f / sample_count;
    
    for (size_t i = 0; i < dim; ++i) {
        float delta = observed_effect[i] - effect_mean[i];
        effect_mean[i] += alpha * delta;
    }
    
    // Update covariance (simplified)
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < dim; ++j) {
            float delta_i = observed_effect[i] - effect_mean[i];
            float delta_j = observed_effect[j] - effect_mean[j];
            effect_cov[i * dim + j] = (1.0f - alpha) * effect_cov[i * dim + j] + alpha * delta_i * delta_j;
        }
    }
    
    confidence = std::min(1.0f, confidence + 0.01f);
}

std::vector<float> InterventionEffect::predictEffect(const std::vector<float>& current_state) const {
    // Simplified: return mean effect
    return effect_mean;
}

// ─── CausalWorldModel ────────────────────────────────────────────────────────

CausalWorldModel::CausalWorldModel(const CausalWorldModelConfig& cfg)
    : config_(cfg), rng_(std::random_device{}()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CausalWorldModel", 
        "Initialized CausalWorldModel with transition learning and counterfactual simulation");
}

void CausalWorldModel::initTransition(uint32_t from, uint32_t to, size_t from_dim, size_t to_dim) {
    uint64_t key = edgeKey(from, to);
    if (transitions_.find(key) == transitions_.end()) {
        transitions_.emplace(key, CausalTransition(from, to, from_dim, to_dim));
        transition_momentum_.emplace(key, CausalTransition(from, to, from_dim, to_dim));
    }
}

std::pair<size_t, size_t> CausalWorldModel::getTransitionDims(uint32_t from, uint32_t to) const {
    // Map node IDs to latent dimensions based on level
    // This is a simplified mapping - in practice would use node metadata
    size_t from_level = std::min<size_t>(from / 100, 4);
    size_t to_level = std::min<size_t>(to / 100, 4);
    
    size_t from_dim = deterministic_dim_per_level[from_level] + latent_dim_per_level[from_level];
    size_t to_dim = deterministic_dim_per_level[to_level] + latent_dim_per_level[to_level];
    
    return {from_dim, to_dim};
}

void CausalWorldModel::learnTransition(const brain::inference::BeliefState& current,
                                        const std::vector<float>& action,
                                        const brain::inference::BeliefState& next) {
    // Convert BeliefState to vector representation
    auto beliefToVec = [](const brain::inference::BeliefState& b) {
        std::vector<float> vec;
        for (float v : b.q_intent) vec.push_back(v);
        for (float v : b.q_engagement) vec.push_back(v);
        for (float v : b.q_urgency) vec.push_back(v);
        vec.push_back(b.safety_mass);
        vec.push_back(b.surprise_budget);
        vec.push_back(b.belief_entropy);
        return vec;
    };
    
    std::vector<float> current_vec = beliefToVec(current);
    std::vector<float> next_vec = beliefToVec(next);
    
    // For each edge in the causal graph, update transition
    for (const auto& node : nodes) {
        for (uint32_t child : node.children) {
            // Check if we have a transition model for this edge
            uint64_t key = edgeKey(node.id, child);
            auto it = transitions_.find(key);
            
            if (it == transitions_.end()) {
                // Initialize transition
                auto dims = getTransitionDims(node.id, child);
                initTransition(node.id, child, dims.first, dims.second);
                it = transitions_.find(key);
            }
            
            // Project states to appropriate dimensions
            // Simplified: use intent dimensions
            std::vector<float> from_state(current.q_intent.begin(), current.q_intent.end());
            std::vector<float> to_state(next.q_intent.begin(), next.q_intent.end());
            
            // Update transition
            it->second.update(from_state, to_state, config_.transition_lr);
            
            // Update momentum
            auto mit = transition_momentum_.find(key);
            if (mit != transition_momentum_.end()) {
                mit->second.updateMomentum(it->second, config_.transition_momentum);
            }
        }
    }
}

void CausalWorldModel::learnLatentTransition(const predictive::RSSMState& current,
                                              const std::vector<float>& action,
                                              const predictive::RSSMState& next,
                                              int level) {
    if (level < 0 || level >= 5) return;
    
    // Create composite state (deterministic + stochastic + discrete)
    std::vector<float> current_vec;
    current_vec.insert(current_vec.end(), current.deterministic.begin(), current.deterministic.end());
    current_vec.insert(current_vec.end(), current.stochastic.begin(), current.stochastic.end());
    current_vec.insert(current_vec.end(), current.discrete_probs.begin(), current.discrete_probs.end());
    
    std::vector<float> next_vec;
    next_vec.insert(next_vec.end(), next.deterministic.begin(), next.deterministic.end());
    next_vec.insert(next_vec.end(), next.stochastic.begin(), next.stochastic.end());
    next_vec.insert(next_vec.end(), next.discrete_probs.begin(), next.discrete_probs.end());
    
    // For each edge at this level, update transition
    // Simplified: assume level corresponds to node ID range
    uint32_t level_base = level * 100;
    for (const auto& node : nodes) {
        if (node.id < level_base || node.id >= level_base + 100) continue;
        
        for (uint32_t child : node.children) {
            if (child < level_base || child >= level_base + 100) continue;
            
            uint64_t key = edgeKey(node.id, child);
            auto it = transitions_.find(key);
            
            if (it == transitions_.end()) {
                auto dims = getTransitionDims(node.id, child);
                initTransition(node.id, child, dims.first, dims.second);
                it = transitions_.find(key);
            }
            
            // Use latent states for transition learning
            it->second.update(current_vec, next_vec, config_.transition_lr);
        }
    }
}

brain::inference::BeliefState CausalWorldModel::predictTransition(
    const brain::inference::BeliefState& current,
    const std::vector<float>& action) const {
    
    brain::inference::BeliefState next = current;
    
    // Use generative model if available
    if (generative_model_) {
        // Convert BeliefState to GenerativeBeliefState
        GenerativeBeliefState gen_current;
        for (int i = 0; i < 8; ++i) gen_current.q_intent[i] = current.q_intent[i];
        for (int i = 0; i < 3; ++i) gen_current.q_engagement[i] = current.q_engagement[i];
        for (int i = 0; i < 2; ++i) gen_current.q_urgency[i] = current.q_urgency[i];
        gen_current.safety_mass = current.safety_mass;
        gen_current.surprise_budget = current.surprise_budget;
        gen_current.entropy = current.belief_entropy;
        gen_current.free_energy = current.free_energy;
        
        GenerativeBeliefState gen_next = generative_model_->predictNextBelief(gen_current, 
            action.empty() ? -1 : static_cast<int>(action[0]));
        
        // Convert back
        for (int i = 0; i < 8; ++i) next.q_intent[i] = gen_next.q_intent[i];
        for (int i = 0; i < 3; ++i) next.q_engagement[i] = gen_next.q_engagement[i];
        for (int i = 0; i < 2; ++i) next.q_urgency[i] = gen_next.q_urgency[i];
        next.safety_mass = gen_next.safety_mass;
        next.surprise_budget = gen_next.surprise_budget;
        next.belief_entropy = gen_next.entropy;
        next.free_energy = gen_next.free_energy;
    }
    
    // Refine with causal transitions
    for (const auto& node : nodes) {
        if (node.children.empty()) continue;
        
        for (uint32_t child : node.children) {
            uint64_t key = edgeKey(node.id, child);
            auto it = transitions_.find(key);
            
            if (it != transitions_.end() && it->second.confidence > 0.1f) {
                // Use transition to predict child state
                std::vector<float> from_state(current.q_intent.begin(), current.q_intent.end());
                auto pred = it->second.predict(from_state);
                
                // Blend with generative model prediction
                if (child < 8 && pred.size() == 8) {
                    for (size_t i = 0; i < 8; ++i) {
                        next.q_intent[i] = 0.7f * next.q_intent[i] + 0.3f * pred[i];
                    }
                }
            }
        }
    }
    
    next.normalize();
    return next;
}

predictive::RSSMState CausalWorldModel::predictLatentTransition(
    const predictive::RSSMState& current,
    const std::vector<float>& action,
    int level) const {
    
    predictive::RSSMState next = current;
    
    if (latent_world_model_ && level >= 0 && level < 5) {
        // Use LatentWorldModel for prediction
        std::array<float, 24> dummy_obs{}; // Would need actual observation
        std::array<std::vector<float>, 5> level_obs;
        // This is simplified - would use actual LWM forward
    }
    
    // Apply causal transitions at this level
    uint32_t level_base = level * 100;
    std::vector<float> current_vec;
    current_vec.insert(current_vec.end(), current.deterministic.begin(), current.deterministic.end());
    current_vec.insert(current_vec.end(), current.stochastic.begin(), current.stochastic.end());
    current_vec.insert(current_vec.end(), current.discrete_probs.begin(), current.discrete_probs.end());
    
    for (const auto& node : nodes) {
        if (node.id < level_base || node.id >= level_base + 100) continue;
        
        for (uint32_t child : node.children) {
            if (child < level_base || child >= level_base + 100) continue;
            
            uint64_t key = edgeKey(node.id, child);
            auto it = transitions_.find(key);
            
            if (it != transitions_.end() && it->second.confidence > 0.1f) {
                auto pred = it->second.predict(current_vec);
                // Blend prediction
                for (size_t i = 0; i < std::min(pred.size(), next.deterministic.size()); ++i) {
                    next.deterministic[i] = 0.8f * next.deterministic[i] + 0.2f * pred[i];
                }
            }
        }
    }
    
    return next;
}

InterventionEffect CausalWorldModel::estimateInterventionEffect(
    uint32_t target_node, uint32_t intervention_node, bool value,
    const brain::inference::BeliefState& current_state) const {
    
    uint64_t key = interventionKey(target_node, intervention_node, value);
    auto it = intervention_effects_.find(key);
    
    if (it != intervention_effects_.end()) {
        return it->second;
    }
    
    // Create new effect estimator
    auto dims = getTransitionDims(intervention_node, target_node);
    InterventionEffect effect(target_node, intervention_node, value, dims.second);
    
    // Estimate using causal graph structure
    // Find adjustment set
    std::unordered_set<uint32_t> adjustment_set;
    auto adj_set = findAdjustmentSet(intervention_node, target_node);
    if (adj_set.has_value()) {
        adjustment_set = adj_set.value();
    }
    
    // Simulate intervention using do-calculus
    // Simplified: use direct transition if exists
    uint64_t trans_key = edgeKey(intervention_node, target_node);
    auto trans_it = transitions_.find(trans_key);
    if (trans_it != transitions_.end()) {
        std::vector<float> intervened_state(current_state.q_intent.begin(), current_state.q_intent.end());
        // Set intervention node to value
        if (intervention_node < 8) {
            intervened_state[intervention_node] = value ? 1.0f : 0.0f;
        }
        auto pred = trans_it->second.predict(intervened_state);
        effect.effect_mean = pred;
        effect.confidence = trans_it->second.confidence;
    }
    
    return effect;
}

CounterfactualTrajectory CausalWorldModel::simulateCounterfactual(
    const brain::inference::BeliefState& initial_state,
    const std::vector<Intervention>& interventions,
    const std::vector<std::vector<float>>& policy_actions,
    int horizon) const {
    
    CounterfactualTrajectory traj;
    int h = (horizon > 0) ? horizon : config_.cf_horizon;
    
    brain::inference::BeliefState state = initial_state;
    
    // Apply interventions
    for (const auto& iv : interventions) {
        if (iv.node_id < 8) {
            state.q_intent[iv.node_id] = iv.value ? 1.0f : 0.0f;
        }
    }
    state.normalize();
    
    traj.states.push_back(std::vector<float>(state.q_intent.begin(), state.q_intent.end()));
    
    for (int step = 0; step < h && step < (int)policy_actions.size(); ++step) {
        const auto& action = policy_actions[step];
        
        // Predict next state with interventions
        state = predictTransition(state, action);
        
        // Re-apply interventions (they persist)
        for (const auto& iv : interventions) {
            if (iv.node_id < 8) {
                state.q_intent[iv.node_id] = iv.value ? 1.0f : 0.0f;
            }
        }
        state.normalize();
        
        traj.states.push_back(std::vector<float>(state.q_intent.begin(), state.q_intent.end()));
        traj.actions.push_back(action);
        
        // Compute reward (simplified: negative free energy)
        float reward = -state.free_energy;
        traj.rewards.push_back(reward);
        traj.total_reward += reward;
    }
    
    return traj;
}

predictive::LatentWorldModel::ImaginationResult CausalWorldModel::simulateLatentCounterfactual(
    const predictive::LatentWorldModel::ImaginationResult& factual_trajectory,
    const std::vector<Intervention>& interventions,
    int level) const {
    
    if (!latent_world_model_ || level < 0 || level >= 5) {
        return factual_trajectory;
    }
    
    // Apply interventions to latent states
    predictive::LatentWorldModel::ImaginationResult cf_result = factual_trajectory;
    
    for (auto& step_states : cf_result.trajectory) {
        auto& rssm = step_states[level];
        
        // Apply intervention effect on deterministic state
        for (const auto& iv : interventions) {
            if (iv.node_id < static_cast<int>(rssm.deterministic.size())) {
                rssm.deterministic[iv.node_id] = iv.value ? 1.0f : -1.0f;
            }
        }
    }
    
    return cf_result;
}

std::vector<CounterfactualTrajectory> CausalWorldModel::generateCounterfactuals(
    const brain::inference::BeliefState& state,
    int n_trajectories,
    int horizon) const {
    
    std::vector<CounterfactualTrajectory> trajectories;
    trajectories.reserve(n_trajectories);
    
    // Generate random intervention policies
    for (int i = 0; i < n_trajectories; ++i) {
        std::vector<Intervention> interventions;
        
        // Randomly select nodes to intervene on
        std::uniform_int_distribution<int> node_dist(0, std::min<int>(nodes.size() - 1, 7));
        std::uniform_int_distribution<int> value_dist(0, 1);
        
        int n_interventions = std::uniform_int_distribution<int>(1, 3)(rng_);
        for (int j = 0; j < n_interventions; ++j) {
            Intervention iv;
            iv.node_id = node_dist(rng_);
            iv.value = value_dist(rng_) == 1;
            interventions.push_back(iv);
        }
        
        // Generate random policy actions
        std::vector<std::vector<float>> policy_actions;
        int h = (horizon > 0) ? horizon : config_.cf_horizon;
        for (int step = 0; step < h; ++step) {
            std::vector<float> action(12, 0.0f);
            action[std::uniform_int_distribution<int>(0, 11)(rng_)] = 1.0f;
            policy_actions.push_back(action);
        }
        
        auto traj = simulateCounterfactual(state, interventions, policy_actions, horizon);
        trajectories.push_back(std::move(traj));
    }
    
    return trajectories;
}

void CausalWorldModel::learnStructure(const std::vector<brain::inference::BeliefState>& states,
                                       const std::vector<std::vector<float>>& actions,
                                       const std::vector<brain::inference::BeliefState>& next_states) {
    // Simplified structure learning using conditional independence tests
    // In practice, would use PC algorithm or similar
    
    if (states.size() < 100) return; // Need sufficient data
    
    // Test pairwise correlations with lag
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = 0; j < nodes.size(); ++j) {
            if (i == j) continue;
            
            // Compute correlation between node i at t and node j at t+1
            float corr = 0.0f;
            int count = 0;
            
            for (size_t t = 0; t + 1 < states.size(); ++t) {
                if (i < 8 && j < 8) {
                    corr += states[t].q_intent[i] * next_states[t + 1].q_intent[j];
                    count++;
                }
            }
            
            if (count > 0) {
                corr /= count;
                
                // If correlation is strong and no edge exists, add it
                if (std::abs(corr) > config_.edge_confidence_threshold) {
                    bool has_edge = false;
                    for (uint32_t child : nodes[i].children) {
                        if (child == static_cast<uint32_t>(j)) {
                            has_edge = true;
                            break;
                        }
                    }
                    if (!has_edge) {
                        addEdge(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
                        // Initialize transition
                        auto dims = getTransitionDims(i, j);
                        initTransition(i, j, dims.first, dims.second);
                    }
                }
            }
        }
    }
}

void CausalWorldModel::discoverEdgesFromSurprise(const brain::inference::BeliefState& state,
                                                  const std::vector<float>& action,
                                                  const brain::inference::BeliefState& next_state,
                                                  float surprise) {
    if (surprise < config_.edge_confidence_threshold) return;
    
    // Find dimensions that changed most
    std::vector<std::pair<int, float>> changes;
    for (int i = 0; i < 8; ++i) {
        float change = std::abs(next_state.q_intent[i] - state.q_intent[i]);
        if (change > 0.1f) {
            changes.emplace_back(i, change);
        }
    }
    
    // Sort by change magnitude
    std::sort(changes.begin(), changes.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Add edges from action to changed dimensions
    int action_idx = -1;
    for (int i = 0; i < 12 && i < (int)action.size(); ++i) {
        if (action[i] > 0.5f) { action_idx = i; break; }
    }
    
    if (action_idx >= 0 && !changes.empty()) {
        // Add edges from action to top changed dimensions
        for (size_t i = 0; i < std::min<size_t>(changes.size(), 3); ++i) {
            uint32_t from = 8 + action_idx; // Action nodes offset
            uint32_t to = changes[i].first;
            
            bool has_edge = false;
            for (uint32_t child : nodes[from].children) {
                if (child == to) { has_edge = true; break; }
            }
            if (!has_edge) {
                addEdge(from, to);
                auto dims = getTransitionDims(from, to);
                initTransition(from, to, dims.first, dims.second);
            }
        }
    }
}

void CausalWorldModel::pruneEdges(float confidence_threshold) {
    if (confidence_threshold < 0) confidence_threshold = config_.edge_confidence_threshold;
    
    std::vector<uint64_t> to_remove;
    for (const auto& [key, trans] : transitions_) {
        if (trans.confidence < confidence_threshold && trans.observation_count > 10) {
            to_remove.push_back(key);
        }
    }
    
    for (uint64_t key : to_remove) {
        transitions_.erase(key);
        transition_momentum_.erase(key);
        
        // Also remove from graph
        uint32_t from = key >> 32;
        uint32_t to = key & 0xFFFFFFFF;
        
        auto& from_node = nodes[from];
        from_node.children.erase(
            std::remove(from_node.children.begin(), from_node.children.end(), to),
            from_node.children.end()
        );
        
        auto& to_node = nodes[to];
        to_node.parents.erase(
            std::remove(to_node.parents.begin(), to_node.parents.end(), from),
            to_node.parents.end()
        );
    }
}

float CausalWorldModel::computeCausalEFE(const brain::inference::BeliefState& belief,
                                          const std::vector<int>& policy,
                                          int horizon) const {
    float efe = 0.0f;
    brain::inference::BeliefState current = belief;
    
    for (int step = 0; step < horizon && step < (int)policy.size(); ++step) {
        int action = policy[step];
        
        // Predict using causal transitions
        current = predictTransition(current, std::vector<float>(12, 0.0f));
        
        // Pragmatic cost: distance from preferred state
        float pragmatic = 0.0f;
        auto map_state = current.getMAP();
        int map_idx = static_cast<int>(map_state.intent);
        for (int i = 0; i < 8; ++i) {
            float preferred = (i == map_idx) ? 1.0f : 0.0f;
            pragmatic += (preferred - current.q_intent[i]) * (preferred - current.q_intent[i]);
        }
        efe += pragmatic;
        
        // Epistemic value: uncertainty reduction
        float entropy = current.belief_entropy;
        efe -= 0.1f * entropy; // Lower entropy is better
    }
    
    return efe;
}

std::vector<InterventionPolicy> CausalWorldModel::getInterventionPolicies(
    const brain::inference::BeliefState& belief) const {
    
    std::vector<InterventionPolicy> policies;
    
    // Generate from base CausalGraph
    auto base_policies = CausalGraph::generateInterventionPolicies(belief);
    policies.insert(policies.end(), base_policies.begin(), base_policies.end());
    
    // Add learned intervention effects
    for (const auto& [key, effect] : intervention_effects_) {
        if (effect.confidence > 0.3f) {
            InterventionPolicy ip;
            ip.target_variable = "node_" + std::to_string(effect.target_node);
            ip.intervention_variable = "do(node_" + std::to_string(effect.intervention_node) + "=" + (effect.intervention_value ? "true" : "false") + ")";
            ip.expected_effect = effect.confidence;
            ip.policy.description = "Learned: " + ip.intervention_variable + " -> " + ip.target_variable;
            policies.push_back(ip);
        }
    }
    
    return policies;
}

std::string CausalWorldModel::serialize() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    
    // Base CausalGraph
    oss << nodes.size() << ';';
    for (const auto& node : nodes) {
        oss << node.id << ';' << node.name << ';';
        oss << node.parents.size() << ';';
        for (uint32_t p : node.parents) oss << p << ';';
        oss << node.children.size() << ';';
        for (uint32_t c : node.children) oss << c << ';';
    }
    
    // Transitions
    oss << transitions_.size() << ';';
    for (const auto& [key, trans] : transitions_) {
        oss << trans.serialize() << ';';
    }
    
    // Intervention effects
    oss << intervention_effects_.size() << ';';
    for (const auto& [key, effect] : intervention_effects_) {
        oss << effect.target_node << ';' << effect.intervention_node << ';' 
            << (effect.intervention_value ? 1 : 0) << ';'
            << effect.confidence << ';' << effect.sample_count << ';';
        for (float v : effect.effect_mean) oss << v << ';';
        for (float v : effect.effect_cov) oss << v << ';';
    }
    
    return oss.str();
}

bool CausalWorldModel::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    
    // Base CausalGraph
    size_t n_nodes;
    if (!std::getline(iss, token, ';')) return false;
    n_nodes = std::stoul(token);
    
    nodes.clear();
    nodes.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i) {
        yuki::causality::CausalNode node;
        if (!std::getline(iss, token, ';')) return false; node.id = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; node.name = token;
        
        size_t n_parents;
        if (!std::getline(iss, token, ';')) return false; n_parents = std::stoul(token);
        node.parents.resize(n_parents);
        for (size_t j = 0; j < n_parents; ++j) {
            if (!std::getline(iss, token, ';')) return false; node.parents[j] = std::stoul(token);
        }
        
        size_t n_children;
        if (!std::getline(iss, token, ';')) return false; n_children = std::stoul(token);
        node.children.resize(n_children);
        for (size_t j = 0; j < n_children; ++j) {
            if (!std::getline(iss, token, ';')) return false; node.children[j] = std::stoul(token);
        }
        
        nodes.push_back(node);
    }
    
    // Transitions
    size_t n_transitions;
    if (!std::getline(iss, token, ';')) return false;
    n_transitions = std::stoul(token);
    
    for (size_t i = 0; i < n_transitions; ++i) {
        std::string trans_data;
        std::getline(iss, trans_data, ';');
        CausalTransition trans;
        trans.deserialize(trans_data);
        transitions_[edgeKey(trans.from_node, trans.to_node)] = trans;
    }
    
    // Intervention effects
    size_t n_effects;
    if (!std::getline(iss, token, ';')) return false;
    n_effects = std::stoul(token);
    
    for (size_t i = 0; i < n_effects; ++i) {
        InterventionEffect effect;
        if (!std::getline(iss, token, ';')) return false; effect.target_node = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; effect.intervention_node = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; effect.intervention_value = std::stoi(token);
        if (!std::getline(iss, token, ';')) return false; effect.confidence = std::stof(token);
        if (!std::getline(iss, token, ';')) return false; effect.sample_count = std::stoi(token);
        
        size_t dim = 8; // Default
        effect.effect_mean.resize(dim);
        effect.effect_cov.resize(dim * dim);
        for (size_t j = 0; j < dim; ++j) {
            if (!std::getline(iss, token, ';')) return false; effect.effect_mean[j] = std::stof(token);
        }
        for (size_t j = 0; j < dim * dim; ++j) {
            if (!std::getline(iss, token, ';')) return false; effect.effect_cov[j] = std::stof(token);
        }
        
        intervention_effects_[interventionKey(effect.target_node, effect.intervention_node, effect.intervention_value)] = effect;
    }
    
    return true;
}

} // namespace causality
} // namespace brain
} // namespace yuki