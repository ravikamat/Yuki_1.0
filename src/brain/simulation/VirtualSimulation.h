#pragma once
// VirtualSimulation — What-if reasoning engine using causal and latent world models
// Supports branching simulations for hypothesis testing and planning

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include "brain/causality/CausalWorldModel.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/inference/BeliefState.h"

namespace yuki {
namespace brain {
namespace simulation {

using yuki::brain::causality::CausalWorldModel;
using yuki::causality::Intervention;
using yuki::brain::predictive::LatentWorldModel;
using yuki::brain::inference::BeliefState;

struct SimulationConfig {
    int max_branches = 16;
    int max_depth = 10;
    int default_horizon = 20;
    float branch_divergence_threshold = 0.1f;
    float confidence_threshold = 0.6f;
    bool use_latent_space = true;
    bool track_counterfactuals = true;
};

struct SimulationBranch {
    uint64_t id = 0;
    uint64_t parent_id = 0;
    std::string description;
    std::vector<Intervention> interventions;
    std::chrono::steady_clock::time_point created;
    float confidence = 0.0f;
    float divergence_from_parent = 0.0f;
    bool is_active = true;
    std::vector<uint64_t> child_ids;
};

struct SimulationResult {
    uint64_t branch_id = 0;
    yuki::brain::causality::CounterfactualTrajectory causal_trajectory;
    LatentWorldModel::ImaginationResult latent_trajectory;
    float expected_free_energy = 0.0f;
    float expected_reward = 0.0f;
    float confidence = 0.0f;
    bool converged = false;
    std::string summary;
};

struct WhatIfQuery {
    std::string natural_language;
    std::vector<Intervention> interventions;
    BeliefState initial_state;
    int horizon = -1;
    int num_samples = 64;
};

class VirtualSimulation {
public:
    explicit VirtualSimulation(const SimulationConfig& cfg = SimulationConfig{});
    ~VirtualSimulation() = default;

    void setCausalWorldModel(CausalWorldModel* cwm) { causal_world_model_ = cwm; }
    void setLatentWorldModel(LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setConfig(const SimulationConfig& cfg) { config_ = cfg; }

    // Main entry: run a what-if query from natural language or structured interventions
    SimulationResult runWhatIf(const WhatIfQuery& query);

    // Run multiple intervention scenarios in parallel and compare
    std::vector<SimulationResult> compareInterventions(
        const BeliefState& initial_state,
        const std::vector<std::vector<Intervention>>& intervention_sets,
        int horizon = -1);

    // Branch management
    uint64_t createBranch(const std::string& description,
                          const std::vector<Intervention>& interventions,
                          uint64_t parent_id = 0);
    bool activateBranch(uint64_t branch_id);
    bool pruneBranch(uint64_t branch_id);
    const SimulationBranch* getBranch(uint64_t branch_id) const;
    std::vector<const SimulationBranch*> getActiveBranches() const;
    std::vector<const SimulationBranch*> getBranchHistory(uint64_t branch_id) const;

    // Run simulation on existing branch
    SimulationResult runBranch(uint64_t branch_id, int horizon = -1);

    // Merge compatible branches
    uint64_t mergeBranches(uint64_t branch_a, uint64_t branch_b,
                           const std::string& merge_description);

    // Get statistics
    struct Stats {
        size_t total_branches = 0;
        size_t active_branches = 0;
        size_t total_simulations = 0;
        float avg_confidence = 0.0f;
    };
    const Stats& getStats() const { return stats_; }

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    SimulationConfig config_;
    CausalWorldModel* causal_world_model_ = nullptr;
    LatentWorldModel* latent_world_model_ = nullptr;

    std::unordered_map<uint64_t, SimulationBranch> branches_;
    uint64_t next_branch_id_ = 1;
    uint64_t root_branch_id_ = 0;

    Stats stats_;

    // Helper: convert natural language to interventions (placeholder for LLM integration)
    std::vector<Intervention> parseInterventions(const std::string& nl) const;

    // Helper: run causal simulation
    SimulationResult runCausalSimulation(const WhatIfQuery& query, uint64_t branch_id) const;

    // Helper: run latent simulation
    SimulationResult runLatentSimulation(const WhatIfQuery& query, uint64_t branch_id) const;

    // Helper: compute branch divergence
    float computeDivergence(const SimulationBranch& a, const SimulationBranch& b) const;

    // Helper: merge trajectories
    yuki::brain::causality::CounterfactualTrajectory mergeTrajectories(
        const yuki::brain::causality::CounterfactualTrajectory& a,
        const yuki::brain::causality::CounterfactualTrajectory& b) const;
};

} // namespace simulation
} // namespace brain
} // namespace yuki