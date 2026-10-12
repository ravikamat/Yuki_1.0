#include "brain/simulation/VirtualSimulation.h"
#include "brain/core/Logger.h"
#include "brain/inference/GenerativeModel.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <cmath>

namespace yuki {
namespace brain {
namespace simulation {

VirtualSimulation::VirtualSimulation(const SimulationConfig& cfg)
    : config_(cfg), root_branch_id_(0) {
    // Create root branch
    SimulationBranch root;
    root.id = next_branch_id_++;
    root.description = "Root (factual timeline)";
    root.created = std::chrono::steady_clock::now();
    root.confidence = 1.0f;
    root_branch_id_ = root.id;
    branches_[root.id] = std::move(root);
    stats_.total_branches = 1;
    stats_.active_branches = 1;
}

SimulationResult VirtualSimulation::runWhatIf(const WhatIfQuery& query) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "VirtualSimulation",
        "Running what-if query: " + query.natural_language);

    uint64_t branch_id = createBranch(query.natural_language, query.interventions, root_branch_id_);
    SimulationResult result = runBranch(branch_id, query.horizon);
    stats_.total_simulations++;
    stats_.avg_confidence = (stats_.avg_confidence * (stats_.total_simulations - 1) + result.confidence) / stats_.total_simulations;
    return result;
}

std::vector<SimulationResult> VirtualSimulation::compareInterventions(
    const BeliefState& initial_state,
    const std::vector<std::vector<Intervention>>& intervention_sets,
    int horizon) {

    std::vector<SimulationResult> results;
    results.reserve(intervention_sets.size());

    for (size_t i = 0; i < intervention_sets.size(); ++i) {
        WhatIfQuery query;
        query.initial_state = initial_state;
        query.interventions = intervention_sets[i];
        query.horizon = horizon;
        query.natural_language = "Intervention set " + std::to_string(i);
        query.num_samples = config_.max_branches;

        uint64_t branch_id = createBranch(query.natural_language, intervention_sets[i], root_branch_id_);
        SimulationResult result = runBranch(branch_id, horizon);
        results.push_back(std::move(result));
    }

    stats_.total_simulations += intervention_sets.size();
    return results;
}

uint64_t VirtualSimulation::createBranch(const std::string& description,
                                         const std::vector<Intervention>& interventions,
                                         uint64_t parent_id) {
    SimulationBranch branch;
    branch.id = next_branch_id_++;
    branch.parent_id = parent_id ? parent_id : root_branch_id_;
    branch.description = description;
    branch.interventions = interventions;
    branch.created = std::chrono::steady_clock::now();
    branch.confidence = 0.0f;
    branch.is_active = true;

    if (parent_id != 0 && branches_.count(parent_id)) {
        branches_[parent_id].child_ids.push_back(branch.id);
        branch.divergence_from_parent = computeDivergence(branches_[parent_id], branch);
    }

    branches_[branch.id] = std::move(branch);
    stats_.total_branches++;
    stats_.active_branches++;

    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "VirtualSimulation",
        "Created branch " + std::to_string(branch.id) + ": " + description);

    return branch.id;
}

bool VirtualSimulation::activateBranch(uint64_t branch_id) {
    auto it = branches_.find(branch_id);
    if (it == branches_.end()) return false;
    if (it->second.is_active) return true;

    it->second.is_active = true;
    stats_.active_branches++;
    return true;
}

bool VirtualSimulation::pruneBranch(uint64_t branch_id) {
    auto it = branches_.find(branch_id);
    if (it == branches_.end()) return false;
    if (!it->second.is_active) return true;

    it->second.is_active = false;
    stats_.active_branches--;

    // Recursively prune children
    for (uint64_t child_id : it->second.child_ids) {
        pruneBranch(child_id);
    }

    return true;
}

const SimulationBranch* VirtualSimulation::getBranch(uint64_t branch_id) const {
    auto it = branches_.find(branch_id);
    return (it != branches_.end()) ? &it->second : nullptr;
}

std::vector<const SimulationBranch*> VirtualSimulation::getActiveBranches() const {
    std::vector<const SimulationBranch*> active;
    for (const auto& [id, branch] : branches_) {
        if (branch.is_active) active.push_back(&branch);
    }
    return active;
}

std::vector<const SimulationBranch*> VirtualSimulation::getBranchHistory(uint64_t branch_id) const {
    std::vector<const SimulationBranch*> history;
    uint64_t current = branch_id;
    while (current != 0 && branches_.count(current)) {
        history.push_back(&branches_.at(current));
        current = branches_.at(current).parent_id;
    }
    std::reverse(history.begin(), history.end());
    return history;
}

SimulationResult VirtualSimulation::runBranch(uint64_t branch_id, int horizon) {
    auto branch_ptr = getBranch(branch_id);
    if (!branch_ptr) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "VirtualSimulation",
            "Branch not found: " + std::to_string(branch_id));
        return SimulationResult{};
    }

    SimulationResult result;
    result.branch_id = branch_id;

    // Build query from branch
    WhatIfQuery query;
    query.interventions = branch_ptr->interventions;
    query.horizon = (horizon > 0) ? horizon : config_.default_horizon;
    query.num_samples = config_.max_branches;

    // Run causal simulation if available
    if (causal_world_model_ && config_.track_counterfactuals) {
        result = runCausalSimulation(query, branch_id);
    }

    // Run latent simulation if available and enabled
    if (latent_world_model_ && config_.use_latent_space) {
        SimulationResult latent_result = runLatentSimulation(query, branch_id);
        // Fuse results: prefer latent for efficiency, causal for accuracy
        if (result.confidence < latent_result.confidence) {
            result.latent_trajectory = std::move(latent_result.latent_trajectory);
            result.expected_free_energy = latent_result.expected_free_energy;
            result.expected_reward = latent_result.expected_reward;
            result.confidence = latent_result.confidence;
        }
    }

    // Update branch confidence
    auto& branch = branches_[branch_id];
    branch.confidence = result.confidence;

    result.converged = result.confidence >= config_.confidence_threshold;

    std::ostringstream oss;
    oss << "Branch " << branch_id << " (EFE=" << result.expected_free_energy
        << ", reward=" << result.expected_reward << ", conf=" << result.confidence << ")";
    result.summary = oss.str();

    return result;
}

uint64_t VirtualSimulation::mergeBranches(uint64_t branch_a, uint64_t branch_b,
                                          const std::string& merge_description) {
    auto* ba = getBranch(branch_a);
    auto* bb = getBranch(branch_b);
    if (!ba || !bb) return 0;

    float divergence = computeDivergence(*ba, *bb);
    if (divergence > config_.branch_divergence_threshold) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "VirtualSimulation",
            "Branches too divergent to merge: " + std::to_string(divergence));
        return 0;
    }

    // Create merged branch
    std::vector<Intervention> merged_interventions = ba->interventions;
    merged_interventions.insert(merged_interventions.end(), bb->interventions.begin(), bb->interventions.end());

    uint64_t merged_id = createBranch(merge_description, merged_interventions, root_branch_id_);
    branches_[merged_id].parent_id = branch_a;
    branches_[branch_a].child_ids.push_back(merged_id);
    branches_[branch_b].child_ids.push_back(merged_id);

    return merged_id;
}

std::vector<Intervention> VirtualSimulation::parseInterventions(const std::string& nl) const {
    // Placeholder: In production, this would use an LLM or semantic parser
    // to convert natural language to structured interventions
    std::vector<Intervention> interventions;
    // For now, return empty - caller must provide structured interventions
    return interventions;
}

SimulationResult VirtualSimulation::runCausalSimulation(const WhatIfQuery& query, uint64_t branch_id) const {
    SimulationResult result;
    result.branch_id = branch_id;

    if (!causal_world_model_) return result;

    // Convert belief state to initial belief for causal model
    BeliefState initial = query.initial_state;

    // Generate policy actions (simplified - would use policy engine in production)
    std::vector<std::vector<float>> policy_actions;
    int h = (query.horizon > 0) ? query.horizon : config_.default_horizon;
    policy_actions.reserve(h);
    for (int i = 0; i < h; ++i) {
        policy_actions.push_back({}); // Empty action - placeholder
    }

    // Run counterfactual simulation
    causality::CounterfactualTrajectory trajectory =
        causal_world_model_->simulateCounterfactual(
            initial, query.interventions, policy_actions, h);

    result.causal_trajectory = std::move(trajectory);

    // Compute expected free energy and reward
    float total_reward = 0.0f;
    for (float r : result.causal_trajectory.rewards) {
        total_reward += r;
    }
    result.expected_reward = total_reward;
    result.expected_free_energy = -total_reward; // Simplified: negative reward as free energy
    result.confidence = causal_world_model_->getTransitions().empty() ? 0.3f : 0.7f;

    return result;
}

SimulationResult VirtualSimulation::runLatentSimulation(const WhatIfQuery& query, uint64_t branch_id) const {
    SimulationResult result;
    result.branch_id = branch_id;

    if (!latent_world_model_) return result;

    // Get config through public accessor (assuming we add one or use public member)
    const auto& lwm_config = latent_world_model_->getConfig();

    // Convert belief state to RSSM states (simplified)
    std::array<predictive::RSSMState, 5> initial_rssm{};
    for (int level = 0; level < 5; ++level) {
        size_t det_dim = lwm_config.levels[level].deterministic_dim;
        size_t stoch_dim = lwm_config.levels[level].stochastic_dim;
        size_t disc_cat = lwm_config.levels[level].discrete_categories;

        initial_rssm[level].deterministic.assign(det_dim, 0.0f);
        initial_rssm[level].stochastic.assign(stoch_dim, 0.0f);
        initial_rssm[level].discrete_logits.assign(disc_cat, 0.0f);
        initial_rssm[level].discrete_probs.assign(disc_cat, 1.0f / disc_cat);
    }

    // Generate policy actions in latent space
    std::vector<std::vector<float>> policy_actions;
    int h = (query.horizon > 0) ? query.horizon : config_.default_horizon;
    int action_dim = 16; // Default action dimension
    policy_actions.reserve(h);
    for (int i = 0; i < h; ++i) {
        policy_actions.push_back(std::vector<float>(action_dim, 0.0f));
    }

    // Run imagination in latent space
    LatentWorldModel::ImaginationResult imagination =
        latent_world_model_->imagine(initial_rssm, policy_actions, h);

    result.latent_trajectory = std::move(imagination);
    result.expected_reward = result.latent_trajectory.cumulative_reward;
    result.expected_free_energy = -result.latent_trajectory.cumulative_reward;
    result.confidence = 0.6f; // Latent simulation is approximate

    return result;
}

float VirtualSimulation::computeDivergence(const SimulationBranch& a, const SimulationBranch& b) const {
    // Simple divergence metric: intervention set difference
    if (a.interventions.empty() && b.interventions.empty()) return 0.0f;
    if (a.interventions.empty() || b.interventions.empty()) return 1.0f;

    // Jaccard distance on intervention targets
    std::unordered_map<uint64_t, int> set_a, set_b;
    for (const auto& iv : a.interventions) {
        uint64_t key = (static_cast<uint64_t>(iv.node_id) << 32) | 0;
        set_a[key] = iv.value ? 1 : 0;
    }
    for (const auto& iv : b.interventions) {
        uint64_t key = (static_cast<uint64_t>(iv.node_id) << 32) | 0;
        set_b[key] = iv.value ? 1 : 0;
    }

    int intersection = 0, union_count = 0;
    for (const auto& [key, val] : set_a) {
        union_count++;
        if (set_b.count(key) && set_b.at(key) == val) intersection++;
    }
    for (const auto& [key, val] : set_b) {
        if (!set_a.count(key)) union_count++;
    }

    return (union_count > 0) ? (1.0f - static_cast<float>(intersection) / union_count) : 0.0f;
}

causality::CounterfactualTrajectory VirtualSimulation::mergeTrajectories(
    const causality::CounterfactualTrajectory& a,
    const causality::CounterfactualTrajectory& b) const {
    // Simple merge: concatenate unique states
    causality::CounterfactualTrajectory merged = a;
    for (const auto& state : b.states) {
        bool found = false;
        for (const auto& existing : merged.states) {
            if (existing == state) { found = true; break; }
        }
        if (!found) merged.states.push_back(state);
    }
    merged.total_reward = std::max(a.total_reward, b.total_reward);
    return merged;
}

std::string VirtualSimulation::serialize() const {
    std::ostringstream oss;
    oss << "{\"branches\":[";
    bool first = true;
    for (const auto& [id, branch] : branches_) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":" << branch.id
            << ",\"parent\":" << branch.parent_id
            << ",\"desc\":\"" << branch.description << "\""
            << ",\"conf\":" << branch.confidence
            << ",\"active\":" << (branch.is_active ? "true" : "false")
            << ",\"div\":" << branch.divergence_from_parent << "}";
    }
    oss << "],\"root\":" << root_branch_id_ << ",\"next_id\":" << next_branch_id_ << "}";
    return oss.str();
}

bool VirtualSimulation::deserialize(const std::string& data) {
    // Simplified deserialization - in production use JSON parser
    branches_.clear();
    next_branch_id_ = 1;
    root_branch_id_ = 0;
    stats_ = Stats{};
    return true;
}

} // namespace simulation
} // namespace brain
} // namespace yuki