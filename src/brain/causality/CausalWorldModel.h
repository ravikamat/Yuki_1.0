#pragma once
// CausalWorldModel — Extends CausalGraph with learned interventional transitions
// Integrates with LatentWorldModel for counterfactual simulation in latent space

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>
#include <random>
#include "CausalGraph.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/BeliefState.h"

namespace yuki {
namespace brain {
namespace causality {

struct CausalWorldModelConfig {
    // Transition learning
    float transition_lr = 0.01f;
    float transition_momentum = 0.9f;
    int max_transition_history = 10000;
    
    // Intervention effect estimation
    int n_intervention_samples = 100;
    float intervention_noise = 0.1f;
    
    // Latent space integration
    bool use_latent_transitions = true;
    float latent_transition_weight = 0.5f;
    
    // Counterfactual simulation
    int cf_horizon = 20;
    int cf_samples = 512;
    float cf_temperature = 1.0f;
    
    // Structure learning
    float edge_confidence_threshold = 0.7f;
    float structure_lr = 0.001f;
    int max_structure_iterations = 100;
};

// Learned transition model for a causal edge
struct CausalTransition {
    uint32_t from_node;
    uint32_t to_node;
    
    // Linear transition: next = W * current + b + noise
    std::vector<float> W;      // to_dim x from_dim
    std::vector<float> b;      // to_dim
    
    // Non-linear transition (MLP)
    std::vector<float> W1;     // hidden x from_dim
    std::vector<float> b1;
    std::vector<float> W2;     // to_dim x hidden
    std::vector<float> b2;
    
    // Statistics
    float confidence = 0.0f;
    int observation_count = 0;
    float running_loss = 0.0f;
    
    CausalTransition() = default;
    CausalTransition(uint32_t from, uint32_t to, size_t from_dim, size_t to_dim, size_t hidden_dim = 32);
    
    std::vector<float> predict(const std::vector<float>& state) const;
    void update(const std::vector<float>& from_state, const std::vector<float>& to_state, float lr);
    void updateMomentum(const CausalTransition& other, float momentum);
    
    std::string serialize() const;
    bool deserialize(const std::string& data);
};

// Intervention effect estimator
struct InterventionEffect {
    uint32_t target_node;
    uint32_t intervention_node;
    bool intervention_value;
    
    std::vector<float> effect_mean;     // Expected change in target
    std::vector<float> effect_cov;      // Covariance of effect
    float confidence = 0.0f;
    int sample_count = 0;
    
    InterventionEffect() = default;
    InterventionEffect(uint32_t target, uint32_t interv, bool value, size_t target_dim);
    
    void update(const std::vector<float>& observed_effect);
    std::vector<float> predictEffect(const std::vector<float>& current_state) const;
};

// Counterfactual trajectory
struct CounterfactualTrajectory {
    std::vector<std::vector<float>> states;      // State at each step
    std::vector<std::vector<float>> actions;     // Actions taken
    std::vector<float> rewards;                  // Reward at each step
    float total_reward = 0.0f;
    float probability = 1.0f;
};

class CausalWorldModel : public yuki::causality::CausalGraph {
public:
    explicit CausalWorldModel(const CausalWorldModelConfig& cfg = CausalWorldModelConfig{});
    ~CausalWorldModel() = default;
    
    using Intervention = yuki::causality::Intervention;
    using InterventionPolicy = yuki::causality::InterventionPolicy;
    
    // Set dependencies
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setGenerativeModel(inference::GenerativeModel* gm) { generative_model_ = gm; }
    
    // ─── Transition Learning ─────────────────────────────────────────────────
    
    // Learn transition from experience (called after each turn)
    void learnTransition(const brain::inference::BeliefState& current,
                         const std::vector<float>& action,
                         const brain::inference::BeliefState& next);
    
    // Learn transition from latent states
    void learnLatentTransition(const predictive::RSSMState& current,
                               const std::vector<float>& action,
                               const predictive::RSSMState& next,
                               int level);
    
    // Predict next belief state using learned causal transitions
    brain::inference::BeliefState predictTransition(
        const brain::inference::BeliefState& current,
        const std::vector<float>& action) const;
    
    // Predict next latent state
    predictive::RSSMState predictLatentTransition(
        const predictive::RSSMState& current,
        const std::vector<float>& action,
        int level) const;
    
    // ─── Intervention & Counterfactuals ──────────────────────────────────────
    
    // Estimate effect of intervention do(X=x) on target Y
    InterventionEffect estimateInterventionEffect(
        uint32_t target_node, uint32_t intervention_node, bool value,
        const brain::inference::BeliefState& current_state) const;
    
    // Simulate counterfactual: what would happen if we intervened?
    CounterfactualTrajectory simulateCounterfactual(
        const brain::inference::BeliefState& initial_state,
        const std::vector<yuki::causality::Intervention>& interventions,
        const std::vector<std::vector<float>>& policy_actions,
        int horizon = -1) const;
    
    // Simulate counterfactual in latent space (more efficient)
    predictive::LatentWorldModel::ImaginationResult simulateLatentCounterfactual(
        const predictive::LatentWorldModel::ImaginationResult& factual_trajectory,
        const std::vector<yuki::causality::Intervention>& interventions,
        int level) const;
    
    // Generate counterfactual trajectories for policy evaluation
    std::vector<CounterfactualTrajectory> generateCounterfactuals(
        const brain::inference::BeliefState& state,
        int n_trajectories,
        int horizon = -1) const;
    
    // ─── Structure Learning ──────────────────────────────────────────────────
    
    // Learn causal structure from observational data
    void learnStructure(const std::vector<brain::inference::BeliefState>& states,
                        const std::vector<std::vector<float>>& actions,
                        const std::vector<brain::inference::BeliefState>& next_states);
    
    // Discover new causal edges from surprise events
    void discoverEdgesFromSurprise(const brain::inference::BeliefState& state,
                                   const std::vector<float>& action,
                                   const brain::inference::BeliefState& next_state,
                                   float surprise);
    
    // Prune low-confidence edges
    void pruneEdges(float confidence_threshold = -1.0f);
    
    // ─── Integration with Policy Engine ──────────────────────────────────────
    
    // Compute Expected Free Energy using causal model
    float computeCausalEFE(const brain::inference::BeliefState& belief,
                           const std::vector<int>& policy,
                           int horizon = 5) const;
    
    // Get intervention policies for Active Inference
    std::vector<yuki::causality::InterventionPolicy> getInterventionPolicies(
        const brain::inference::BeliefState& belief) const;
    
    // ─── Serialization ───────────────────────────────────────────────────────
    
    std::string serialize() const;
    bool deserialize(const std::string& data);
    
    // Accessors
    const std::unordered_map<uint64_t, CausalTransition>& getTransitions() const { return transitions_; }
    const std::unordered_map<uint64_t, InterventionEffect>& getInterventionEffects() const { return intervention_effects_; }
    
private:
    CausalWorldModelConfig config_;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    inference::GenerativeModel* generative_model_ = nullptr;
    
    // Transition models per edge (key = from_node * 10000 + to_node)
    std::unordered_map<uint64_t, CausalTransition> transitions_;
    
    // Intervention effect cache
    std::unordered_map<uint64_t, InterventionEffect> intervention_effects_;
    
    // Momentum for transition updates
    std::unordered_map<uint64_t, CausalTransition> transition_momentum_;
    
    // Random generator
    mutable std::mt19937 rng_;
    
    // Helpers
    uint64_t edgeKey(uint32_t from, uint32_t to) const { return (static_cast<uint64_t>(from) << 32) | to; }
    uint64_t interventionKey(uint32_t target, uint32_t interv, bool value) const { 
        return (static_cast<uint64_t>(target) << 33) | (static_cast<uint64_t>(interv) << 1) | (value ? 1 : 0); 
    }
    
    // Initialize transition for edge
    void initTransition(uint32_t from, uint32_t to, size_t from_dim, size_t to_dim);
    
    // Compute transition dimensions from nodes
    std::pair<size_t, size_t> getTransitionDims(uint32_t from, uint32_t to) const;
    
    // Latent dimension per level
    static constexpr size_t latent_dim_per_level[5] = {8, 16, 32, 32, 64};
    static constexpr size_t deterministic_dim_per_level[5] = {32, 64, 128, 256, 512};
};

} // namespace causality
} // namespace brain
} // namespace yuki