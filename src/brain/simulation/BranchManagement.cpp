#include "brain/simulation/BranchManagement.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace yuki {
namespace brain {
namespace simulation {

BranchManagement::BranchManagement(const HypothesisConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "BranchManagement",
        "Initialized with max_hypotheses=" + std::to_string(config_.max_hypotheses));
}

uint64_t BranchManagement::proposeHypothesis(const std::string& statement,
                                             const std::string& formal_rep,
                                             float prior,
                                             uint64_t parent_id) {
    if (hypotheses_.size() >= static_cast<size_t>(config_.max_hypotheses)) {
        // Remove oldest archived hypothesis
        auto oldest = hypotheses_.begin();
        for (auto it = hypotheses_.begin(); it != hypotheses_.end(); ++it) {
            if (it->second.status == HypothesisStatus::ARCHIVED &&
                it->second.created < oldest->second.created) {
                oldest = it;
            }
        }
        if (oldest->second.status == HypothesisStatus::ARCHIVED) {
            hypotheses_.erase(oldest);
        }
    }

    Hypothesis hyp;
    hyp.id = next_hypothesis_id_++;
    hyp.statement = statement;
    hyp.formal_representation = formal_rep;
    hyp.prior = (prior > 0) ? prior : config_.prior_weight;
    hyp.posterior = hyp.prior;
    hyp.confidence = hyp.prior;
    hyp.created = std::chrono::steady_clock::now();
    hyp.last_updated = hyp.created;
    hyp.parent_hypothesis = parent_id;

    if (parent_id != 0 && hypotheses_.count(parent_id)) {
        hypotheses_[parent_id].child_hypotheses.push_back(hyp.id);
    }

    // Create corresponding simulation branch if VirtualSimulation available
    if (virtual_simulation_ && !formal_rep.empty()) {
        // Parse interventions from formal representation (simplified)
        std::vector<Intervention> interventions;
        hyp.simulation_branch_id = virtual_simulation_->createBranch(statement, interventions, 0);
    }

    hypotheses_[hyp.id] = std::move(hyp);
    stats_.total_hypotheses++;
    stats_.active_hypotheses++;
    updateStats();

    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "BranchManagement",
        "Proposed hypothesis " + std::to_string(hyp.id) + ": " + statement);

    return hyp.id;
}

uint64_t BranchManagement::createFromSimulation(uint64_t branch_id, const std::string& statement) {
    auto* branch = virtual_simulation_ ? virtual_simulation_->getBranch(branch_id) : nullptr;
    if (!branch) return 0;

    std::string formal_rep = "Branch " + std::to_string(branch_id) + ": " + branch->description;
    uint64_t hyp_id = proposeHypothesis(statement, formal_rep, 0.6f, 0);

    if (hyp_id != 0) {
        hypotheses_[hyp_id].simulation_branch_id = branch_id;
        // Add simulation result as evidence
        Evidence ev;
        ev.type = Evidence::Type::SIMULATION;
        ev.description = "VirtualSimulation branch " + std::to_string(branch_id);
        ev.weight = 0.5f;  // Neutral initial weight
        ev.reliability = 0.7f;
        ev.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        ev.hypothesis_id = hyp_id;
        ev.source = "VirtualSimulation";
        addEvidence(hyp_id, ev);
    }

    return hyp_id;
}

void BranchManagement::addEvidence(uint64_t hypothesis_id, const Evidence& evidence) {
    auto it = hypotheses_.find(hypothesis_id);
    if (it == hypotheses_.end()) return;

    Evidence ev = evidence;
    ev.hypothesis_id = hypothesis_id;
    ev.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    it->second.evidence.push_back(ev);

    // Maintain evidence window
    if (it->second.evidence.size() > static_cast<size_t>(config_.evidence_window)) {
        it->second.evidence.erase(it->second.evidence.begin(),
                                  it->second.evidence.end() - config_.evidence_window);
    }

    it->second.last_updated = std::chrono::steady_clock::now();
    it->second.updatePosterior(config_.evidence_decay);

    // Update confidence history
    it->second.confidence_history.push_back(it->second.confidence);
    if (it->second.confidence_history.size() > 1000) {
        it->second.confidence_history.pop_front();
    }

    // Update status based on confidence
    if (it->second.confidence >= 0.8f) {
        it->second.status = HypothesisStatus::SUPPORTED;
    } else if (it->second.confidence <= 0.2f) {
        it->second.status = HypothesisStatus::REFUTED;
    }

    updateStats();
}

void BranchManagement::addEvidenceBatch(uint64_t hypothesis_id, const std::vector<Evidence>& evidence) {
    for (const auto& ev : evidence) {
        addEvidence(hypothesis_id, ev);
    }
}

void BranchManagement::evaluateAll() {
    for (auto& [id, hyp] : hypotheses_) {
        if (hyp.status == HypothesisStatus::ACTIVE ||
            hyp.status == HypothesisStatus::SUPPORTED) {
            hyp.updatePosterior(config_.evidence_decay);

            // Run simulation if linked
            if (virtual_simulation_ && hyp.simulation_branch_id != 0) {
                auto result = virtual_simulation_->runBranch(hyp.simulation_branch_id);
                if (result.converged) {
                    Evidence ev;
                    ev.type = Evidence::Type::SIMULATION;
                    ev.description = "Simulation result: " + result.summary;
                    ev.weight = result.confidence * 0.5f;
                    ev.reliability = 0.8f;
                    ev.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    ev.hypothesis_id = id;
                    ev.source = "VirtualSimulation";
                    hyp.evidence.push_back(ev);
                }
            }
        }
    }
    updateStats();
}

uint64_t BranchManagement::getBestHypothesis() const {
    uint64_t best = 0;
    float best_posterior = -1.0f;

    for (const auto& [id, hyp] : hypotheses_) {
        if (hyp.status != HypothesisStatus::REFUTED &&
            hyp.status != HypothesisStatus::ARCHIVED &&
            hyp.status != HypothesisStatus::MERGED) {
            if (hyp.posterior > best_posterior) {
                best_posterior = hyp.posterior;
                best = id;
            }
        }
    }
    return best;
}

const Hypothesis* BranchManagement::getHypothesis(uint64_t id) const {
    auto it = hypotheses_.find(id);
    return (it != hypotheses_.end()) ? &it->second : nullptr;
}

std::vector<const Hypothesis*> BranchManagement::getActiveHypotheses() const {
    std::vector<const Hypothesis*> active;
    for (const auto& [id, hyp] : hypotheses_) {
        if (hyp.status == HypothesisStatus::ACTIVE ||
            hyp.status == HypothesisStatus::SUPPORTED) {
            active.push_back(&hyp);
        }
    }
    return active;
}

std::vector<const Hypothesis*> BranchManagement::getHypothesesByStatus(HypothesisStatus status) const {
    std::vector<const Hypothesis*> result;
    for (const auto& [id, hyp] : hypotheses_) {
        if (hyp.status == status) result.push_back(&hyp);
    }
    return result;
}

void BranchManagement::markContradiction(uint64_t hyp_a, uint64_t hyp_b, float severity) {
    auto it_a = hypotheses_.find(hyp_a);
    auto it_b = hypotheses_.find(hyp_b);
    if (it_a == hypotheses_.end() || it_b == hypotheses_.end()) return;

    it_a->second.contradictory_hypotheses.push_back(hyp_b);
    it_b->second.contradictory_hypotheses.push_back(hyp_a);

    applyContradictionPenalty(hyp_a, hyp_b, severity);
    updateStats();
}

void BranchManagement::markSupport(uint64_t supporter, uint64_t supported, float strength) {
    auto it_s = hypotheses_.find(supporter);
    auto it_d = hypotheses_.find(supported);
    if (it_s == hypotheses_.end() || it_d == hypotheses_.end()) return;

    it_s->second.supporting_hypotheses.push_back(supported);

    // Add supporting evidence
    Evidence ev;
    ev.type = Evidence::Type::LOGICAL_PROOF;
    ev.description = "Supports hypothesis " + std::to_string(supported);
    ev.weight = strength * 0.3f;
    ev.reliability = 0.8f;
    ev.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    ev.hypothesis_id = supported;
    ev.source = "Hypothesis " + std::to_string(supporter);
    it_d->second.evidence.push_back(ev);

    it_d->second.updatePosterior(config_.evidence_decay);
    updateStats();
}

uint64_t BranchManagement::mergeHypotheses(uint64_t hyp_a, uint64_t hyp_b,
                                           const std::string& merged_statement) {
    auto it_a = hypotheses_.find(hyp_a);
    auto it_b = hypotheses_.find(hyp_b);
    if (it_a == hypotheses_.end() || it_b == hypotheses_.end()) return 0;

    uint64_t merged_id = proposeHypothesis(merged_statement,
        it_a->second.formal_representation + " + " + it_b->second.formal_representation,
        (it_a->second.prior + it_b->second.prior) / 2.0f, 0);

    if (merged_id != 0) {
        auto& merged = hypotheses_[merged_id];
        merged.evidence.insert(merged.evidence.end(),
                               it_a->second.evidence.begin(), it_a->second.evidence.end());
        merged.evidence.insert(merged.evidence.end(),
                               it_b->second.evidence.begin(), it_b->second.evidence.end());

        // Combine simulation branches if both have them
        if (virtual_simulation_ && it_a->second.simulation_branch_id != 0 &&
            it_b->second.simulation_branch_id != 0) {
            merged.simulation_branch_id = virtual_simulation_->mergeBranches(
                it_a->second.simulation_branch_id,
                it_b->second.simulation_branch_id,
                merged_statement);
        }

        // Mark originals as merged
        it_a->second.status = HypothesisStatus::MERGED;
        it_b->second.status = HypothesisStatus::MERGED;
        stats_.active_hypotheses -= 2;
    }

    updateStats();
    return merged_id;
}

std::vector<std::pair<uint64_t, uint64_t>> BranchManagement::detectContradictions() const {
    std::vector<std::pair<uint64_t, uint64_t>> contradictions;

    std::vector<uint64_t> active_ids;
    for (const auto& [id, hyp] : hypotheses_) {
        if (hyp.status == HypothesisStatus::ACTIVE ||
            hyp.status == HypothesisStatus::SUPPORTED) {
            active_ids.push_back(id);
        }
    }

    // Check pairwise for contradictions
    for (size_t i = 0; i < active_ids.size(); ++i) {
        for (size_t j = i + 1; j < active_ids.size(); ++j) {
            const auto* h1 = getHypothesis(active_ids[i]);
            const auto* h2 = getHypothesis(active_ids[j]);

            // Check explicit contradiction links
            bool explicit_contradiction = false;
            for (uint64_t c : h1->contradictory_hypotheses) {
                if (c == active_ids[j]) { explicit_contradiction = true; break; }
            }

            // Check semantic opposition via HDC
            float sim = 1.0f;
            if (hdc_graph_) {
                sim = semanticSimilarity(h1->statement, h2->statement);
            }

            if (explicit_contradiction || sim < -0.5f) {
                contradictions.emplace_back(active_ids[i], active_ids[j]);
            }
        }
    }

    return contradictions;
}

void BranchManagement::resolveContradictions() {
    auto contradictions = detectContradictions();

    for (const auto& [a, b] : contradictions) {
        auto it_a = hypotheses_.find(a);
        auto it_b = hypotheses_.find(b);
        if (it_a == hypotheses_.end() || it_b == hypotheses_.end()) continue;

        // Keep the one with higher posterior, archive the other
        if (it_a->second.posterior >= it_b->second.posterior) {
            it_b->second.status = HypothesisStatus::REFUTED;
            stats_.refuted++;
            stats_.active_hypotheses--;
        } else {
            it_a->second.status = HypothesisStatus::REFUTED;
            stats_.refuted++;
            stats_.active_hypotheses--;
        }
    }

    updateStats();
}

std::unordered_map<uint64_t, float> BranchManagement::getPosteriorDistribution() const {
    std::unordered_map<uint64_t, float> dist;
    float total = 0.0f;

    for (const auto& [id, hyp] : hypotheses_) {
        if (hyp.status != HypothesisStatus::ARCHIVED &&
            hyp.status != HypothesisStatus::MERGED) {
            dist[id] = hyp.posterior;
            total += hyp.posterior;
        }
    }

    // Normalize
    if (total > 0.0f) {
        for (auto& [id, post] : dist) {
            post /= total;
        }
    }

    return dist;
}

std::pair<float, float> BranchManagement::getConfidenceInterval(uint64_t id, float alpha) const {
    auto it = hypotheses_.find(id);
    if (it == hypotheses_.end() || it->second.confidence_history.empty()) {
        return {0.0f, 1.0f};
    }

    const auto& history = it->second.confidence_history;
    std::vector<float> sorted(history.begin(), history.end());
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    size_t lower_idx = static_cast<size_t>(std::floor(alpha / 2.0f * n));
    size_t upper_idx = static_cast<size_t>(std::ceil((1.0f - alpha / 2.0f) * n) - 1);

    lower_idx = std::min(lower_idx, n - 1);
    upper_idx = std::min(upper_idx, n - 1);

    return {sorted[lower_idx], sorted[upper_idx]};
}

std::vector<uint64_t> BranchManagement::getLineage(uint64_t id) const {
    std::vector<uint64_t> lineage;
    auto it = hypotheses_.find(id);
    if (it == hypotheses_.end()) return lineage;

    // Ancestors
    std::vector<uint64_t> ancestors;
    uint64_t current = it->second.parent_hypothesis;
    while (current != 0 && hypotheses_.count(current)) {
        ancestors.push_back(current);
        current = hypotheses_.at(current).parent_hypothesis;
    }
    std::reverse(ancestors.begin(), ancestors.end());
    lineage.insert(lineage.end(), ancestors.begin(), ancestors.end());

    // Self
    lineage.push_back(id);

    // Descendants (BFS)
    std::deque<uint64_t> queue;
    for (uint64_t child : it->second.child_hypotheses) {
        queue.push_back(child);
    }
    while (!queue.empty()) {
        uint64_t curr = queue.front();
        queue.pop_front();
        lineage.push_back(curr);
        auto child_it = hypotheses_.find(curr);
        if (child_it != hypotheses_.end()) {
            for (uint64_t child : child_it->second.child_hypotheses) {
                queue.push_back(child);
            }
        }
    }

    return lineage;
}

float BranchManagement::computePosterior(const Hypothesis& hyp) const {
    // Bayesian update: posterior ∝ prior × likelihood(evidence)
    float log_prior = std::log(hyp.prior / (1.0f - hyp.prior + 1e-8f));
    float log_likelihood = 0.0f;

    for (const auto& ev : hyp.evidence) {
        float effective_weight = ev.weight * ev.reliability;
        // Convert weight to log-likelihood ratio
        if (effective_weight > 0) {
            log_likelihood += std::log(1.0f + effective_weight);
        } else {
            log_likelihood += std::log(1.0f - std::abs(effective_weight));
        }
    }

    float log_posterior = log_prior + log_likelihood;
    float posterior = 1.0f / (1.0f + std::exp(-log_posterior));

    return std::clamp(posterior, config_.min_confidence, config_.max_confidence);
}

void BranchManagement::applyContradictionPenalty(uint64_t hyp_a, uint64_t hyp_b, float severity) {
    auto it_a = hypotheses_.find(hyp_a);
    auto it_b = hypotheses_.find(hyp_b);
    if (it_a == hypotheses_.end() || it_b == hypotheses_.end()) return;

    float penalty = config_.contradiction_penalty * severity;

    // Add contradictory evidence to both
    Evidence ev_a;
    ev_a.type = Evidence::Type::CONTRADICTION;
    ev_a.description = "Contradicts hypothesis " + std::to_string(hyp_b);
    ev_a.weight = -penalty;
    ev_a.reliability = 0.9f;
    ev_a.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    ev_a.hypothesis_id = hyp_a;
    ev_a.source = "ContradictionDetection";
    it_a->second.evidence.push_back(ev_a);

    Evidence ev_b;
    ev_b.type = Evidence::Type::CONTRADICTION;
    ev_b.description = "Contradicts hypothesis " + std::to_string(hyp_a);
    ev_b.weight = -penalty;
    ev_b.reliability = 0.9f;
    ev_b.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    ev_b.hypothesis_id = hyp_b;
    ev_b.source = "ContradictionDetection";
    it_b->second.evidence.push_back(ev_b);

    it_a->second.updatePosterior(config_.evidence_decay);
    it_b->second.updatePosterior(config_.evidence_decay);
}

void BranchManagement::updateStats() {
    stats_.total_hypotheses = hypotheses_.size();
    stats_.active_hypotheses = 0;
    stats_.supported = 0;
    stats_.refuted = 0;
    float conf_sum = 0.0f;
    int conf_count = 0;

    for (const auto& [id, hyp] : hypotheses_) {
        switch (hyp.status) {
            case HypothesisStatus::ACTIVE: stats_.active_hypotheses++; break;
            case HypothesisStatus::SUPPORTED: stats_.supported++; break;
            case HypothesisStatus::REFUTED: stats_.refuted++; break;
            default: break;
        }
        if (hyp.status != HypothesisStatus::ARCHIVED &&
            hyp.status != HypothesisStatus::MERGED) {
            conf_sum += hyp.confidence;
            conf_count++;
        }
    }
    stats_.avg_confidence = (conf_count > 0) ? conf_sum / conf_count : 0.0f;
}

float BranchManagement::semanticSimilarity(const std::string& a, const std::string& b) const {
    if (!hdc_graph_) return 0.0f;
    // Simplified: would use HDC graph to compute semantic similarity
    // For now, return 0 (neutral)
    return 0.0f;
}

std::string BranchManagement::serialize() const {
    std::ostringstream oss;
    oss << "{\"hypotheses\":[";
    bool first = true;
    for (const auto& [id, hyp] : hypotheses_) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":" << id
            << ",\"stmt\":\"" << hyp.statement << "\""
            << ",\"prior\":" << hyp.prior
            << ",\"post\":" << hyp.posterior
            << ",\"conf\":" << hyp.confidence
            << ",\"status\":" << static_cast<int>(hyp.status) << "}";
    }
    oss << "]}";
    return oss.str();
}

bool BranchManagement::deserialize(const std::string& data) {
    hypotheses_.clear();
    next_hypothesis_id_ = 1;
    stats_ = Stats{};
    return true;
}

// Hypothesis member implementation
void Hypothesis::updatePosterior(float decay) {
    // Apply decay to old evidence
    for (auto& ev : evidence) {
        ev.weight *= decay;
    }

    // Recompute posterior (simplified - would use BranchManagement::computePosterior in practice)
    float evidence_weight = computeEvidenceWeight();
    posterior = std::clamp(prior + evidence_weight * 0.5f, 0.0f, 1.0f);
    confidence = posterior;
}

float Hypothesis::computeEvidenceWeight() const {
    float total = 0.0f;
    for (const auto& ev : evidence) {
        total += ev.weight * ev.reliability;
    }
    return std::tanh(total);  // Bounded [-1, 1]
}

} // namespace simulation
} // namespace brain
} // namespace yuki