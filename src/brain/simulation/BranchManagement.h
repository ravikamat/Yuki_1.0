#pragma once
// BranchManagement — Hypothesis tracking and evidence accumulation
// Manages competing hypotheses, tracks evidence, computes posterior probabilities

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <deque>
#include <functional>
#include "brain/simulation/VirtualSimulation.h"
#include "brain/inference/BeliefState.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace simulation {

using VirtualSimulation = simulation::VirtualSimulation;
using inference::BeliefState;
using yuki::memory::HdcSemanticGraph;

struct HypothesisConfig {
    float prior_weight = 0.5f;
    float evidence_decay = 0.99f;  // Per-turn decay
    float min_confidence = 0.05f;
    float max_confidence = 0.95f;
    float contradiction_penalty = 0.3f;
    int max_hypotheses = 32;
    int evidence_window = 100;
};

enum class HypothesisStatus : uint8_t {
    ACTIVE = 0,        // Currently being evaluated
    SUPPORTED = 1,     // Strong evidence in favor
    REFUTED = 2,       // Strong evidence against
    MERGED = 3,        // Merged into another hypothesis
    ARCHIVED = 4       // Historical, not active
};

struct Evidence {
    enum class Type {
        OBSERVATION,       // Direct observation supports/refutes
        SIMULATION,        // VirtualSimulation result
        CAUSAL_INTERVENTION, // do(X) intervention result
        LOGICAL_PROOF,     // Logical derivation
        CONTRADICTION,     // Contradiction detected
        EXPERT_INPUT       // Human/external input
    };

    Type type;
    std::string description;
    float weight = 1.0f;           // Strength of evidence [-1, 1]
    float reliability = 1.0f;      // Source reliability [0, 1]
    uint64_t timestamp = 0;
    uint64_t hypothesis_id = 0;
    std::string source;            // Source identifier
};

struct Hypothesis {
    uint64_t id = 0;
    std::string statement;          // Natural language description
    std::string formal_representation; // Logical/causal representation
    HypothesisStatus status = HypothesisStatus::ACTIVE;
    float prior = 0.5f;
    float posterior = 0.5f;
    float confidence = 0.5f;

    std::vector<Evidence> evidence;
    std::deque<float> confidence_history;  // Track confidence over time
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_updated;

    uint64_t parent_hypothesis = 0;   // If derived from another
    std::vector<uint64_t> child_hypotheses;
    std::vector<uint64_t> contradictory_hypotheses;
    std::vector<uint64_t> supporting_hypotheses;

    uint64_t simulation_branch_id = 0; // Link to VirtualSimulation branch

    // Compute current posterior from evidence
    void updatePosterior(float decay = 0.99f);
    float computeEvidenceWeight() const;
};

struct HypothesisSet {
    std::string domain;               // Domain/topic of hypotheses
    std::unordered_map<uint64_t, Hypothesis> hypotheses;
    uint64_t best_hypothesis = 0;     // Highest posterior
    std::chrono::steady_clock::time_point last_eval;
};

class BranchManagement {
public:
    explicit BranchManagement(const HypothesisConfig& cfg = HypothesisConfig{});
    ~BranchManagement() = default;

    void setVirtualSimulation(VirtualSimulation* vs) { virtual_simulation_ = vs; }
    void setHdcGraph(HdcSemanticGraph* hdc) { hdc_graph_ = hdc; }
    void setConfig(const HypothesisConfig& cfg) { config_ = cfg; }

    // ─── Hypothesis Lifecycle ────────────────────────────────────────────────

    // Create a new hypothesis from natural language
    uint64_t proposeHypothesis(const std::string& statement,
                               const std::string& formal_rep = "",
                               float prior = -1.0f,
                               uint64_t parent_id = 0);

    // Create hypothesis from VirtualSimulation branch
    uint64_t createFromSimulation(uint64_t branch_id, const std::string& statement);

    // Update hypothesis with new evidence
    void addEvidence(uint64_t hypothesis_id, const Evidence& evidence);
    void addEvidenceBatch(uint64_t hypothesis_id, const std::vector<Evidence>& evidence);

    // Evaluate all hypotheses (Bayesian update)
    void evaluateAll();

    // Get current best hypothesis
    uint64_t getBestHypothesis() const;
    const Hypothesis* getHypothesis(uint64_t id) const;
    std::vector<const Hypothesis*> getActiveHypotheses() const;
    std::vector<const Hypothesis*> getHypothesesByStatus(HypothesisStatus status) const;

    // ─── Hypothesis Relations ────────────────────────────────────────────────

    // Mark two hypotheses as contradictory
    void markContradiction(uint64_t hyp_a, uint64_t hyp_b, float severity = 1.0f);

    // Mark hypothesis as supporting another
    void markSupport(uint64_t supporter, uint64_t supported, float strength = 1.0f);

    // Merge two hypotheses (combine evidence)
    uint64_t mergeHypotheses(uint64_t hyp_a, uint64_t hyp_b,
                             const std::string& merged_statement);

    // ─── Contradiction Resolution ────────────────────────────────────────────

    // Detect contradictions between hypotheses
    std::vector<std::pair<uint64_t, uint64_t>> detectContradictions() const;

    // Auto-resolve contradictions using evidence
    void resolveContradictions();

    // ─── Query & Analysis ────────────────────────────────────────────────────

    // Get posterior distribution over all hypotheses
    std::unordered_map<uint64_t, float> getPosteriorDistribution() const;

    // Get confidence interval for hypothesis
    std::pair<float, float> getConfidenceInterval(uint64_t id, float alpha = 0.05f) const;

    // Get hypothesis lineage (ancestors + descendants)
    std::vector<uint64_t> getLineage(uint64_t id) const;

    // Statistics
    struct Stats {
        size_t total_hypotheses = 0;
        size_t active_hypotheses = 0;
        size_t supported = 0;
        size_t refuted = 0;
        float avg_confidence = 0.0f;
    };
    const Stats& getStats() const { return stats_; }

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    HypothesisConfig config_;
    VirtualSimulation* virtual_simulation_ = nullptr;
    HdcSemanticGraph* hdc_graph_ = nullptr;

    std::unordered_map<uint64_t, Hypothesis> hypotheses_;
    uint64_t next_hypothesis_id_ = 1;
    Stats stats_;

    // Helper: compute posterior from evidence (Bayesian update)
    float computePosterior(const Hypothesis& hyp) const;

    // Helper: apply contradiction penalty
    void applyContradictionPenalty(uint64_t hyp_a, uint64_t hyp_b, float severity);

    // Helper: update stats
    void updateStats();

    // Helper: semantic similarity via HDC
    float semanticSimilarity(const std::string& a, const std::string& b) const;
};

} // namespace simulation
} // namespace brain
} // namespace yuki