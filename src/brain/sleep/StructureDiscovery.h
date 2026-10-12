#pragma once
// StructureDiscovery — Sleep-driven causal structure learning and knowledge distillation
// Discovers causal structure from episodic memory, exports distillation corpora

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include "brain/memory/EpisodicStore.h"
#include "brain/causality/CausalWorldModel.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/knowledge/ContradictionDetector.h"
#include "brain/simulation/VirtualSimulation.h"
#include "brain/simulation/BranchManagement.h"

namespace yuki {
namespace brain {
namespace sleep {

struct StructureDiscoveryConfig {
    // Structure learning
    float independence_threshold = 0.05f;
    int max_conditioning_set = 3;
    float edge_confidence_threshold = 0.7f;
    int min_interventions_per_edge = 3;
    int max_nodes = 1000;
    float prune_threshold = 0.1f;
    int structure_learning_iterations = 100;

    // Discovery scheduling
    std::chrono::hours discovery_interval{24};
    int max_discoveries_per_epoch = 10;
    float min_surprise_for_discovery = 0.5f;

    // Distillation export
    bool export_sft = true;
    bool export_dpo = true;
    bool export_structure = true;
    std::string export_directory = "data/distillation";
    size_t max_sft_samples = 50000;
    size_t max_dpo_pairs = 10000;

    // Quality thresholds
    float min_sft_quality = 0.7f;
    float min_dpo_chosen = 0.8f;
    float max_dpo_rejected = 0.6f;
};

struct DiscoveredStructure {
    struct Edge {
        uint32_t from;
        uint32_t to;
        float confidence = 0.0f;
        float strength = 0.0f;
        std::string relation_type;  // "causes", "enables", "prevents", "correlates"
        int evidence_count = 0;
        std::vector<uint64_t> supporting_episodes;
    };

    struct Node {
        uint32_t id = 0;
        std::string label;
        std::string concept_type;  // "entity", "event", "property", "action"
        float centrality = 0.0f;
    };

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    float overall_confidence = 0.0f;
    std::chrono::steady_clock::time_point discovered_at;
    uint64_t source_episodes = 0;
};

struct DistillationExport {
    std::string sft_path;
    std::string dpo_path;
    std::string structure_path;
    size_t sft_samples = 0;
    size_t dpo_pairs = 0;
    size_t structures = 0;
    std::chrono::steady_clock::time_point exported_at;
    bool success = false;
    std::string error_message;
};

class StructureDiscovery {
public:
    explicit StructureDiscovery(const StructureDiscoveryConfig& cfg = StructureDiscoveryConfig{});
    ~StructureDiscovery() = default;

    void setEpisodicStore(memory::EpisodicStore* store) { episodic_store_ = store; }
    void setCausalWorldModel(causality::CausalWorldModel* cwm) { causal_world_model_ = cwm; }
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setContradictionDetector(knowledge::ContradictionDetector* cd) { contradiction_detector_ = cd; }
    void setVirtualSimulation(simulation::VirtualSimulation* vs) { virtual_simulation_ = vs; }
    void setBranchManagement(simulation::BranchManagement* bm) { branch_management_ = bm; }
    void setConfig(const StructureDiscoveryConfig& cfg) { config_ = cfg; }

    // ─── Main Discovery Loop ────────────────────────────────────────────────

    // Run one epoch of structure discovery
    size_t runDiscoveryEpoch(float& avg_confidence_out);

    // Continuous discovery (called from SleepThread)
    void discoverStructures();

    // ─── Causal Structure Learning ──────────────────────────────────────────

    // Learn causal graph from episodic transitions
    DiscoveredStructure learnCausalStructure();

    // Discover new causal edges from surprise events
    std::vector<DiscoveredStructure::Edge> discoverEdgesFromSurprise();

    // Validate discovered structure against held-out data
    float validateStructure(const DiscoveredStructure& structure) const;

    // ─── Knowledge Distillation Export ──────────────────────────────────────

    // Export distillation corpora (SFT + DPO + structure)
    DistillationExport exportDistillation();

    // Export discovered causal structure as JSON
    bool exportStructureJson(const std::string& path, const DiscoveredStructure& structure) const;

    // Export for specific curriculum stage
    std::vector<std::string> exportCurriculumSeeds(const std::string& base_dir) const;

    // ─── Integration with VirtualSimulation & BranchManagement ──────────────

    // Generate hypotheses from discovered structures
    std::vector<uint64_t> generateHypothesesFromStructure(const DiscoveredStructure& structure);

    // Test hypotheses via simulation
    void testHypotheses(const std::vector<uint64_t>& hypothesis_ids);

    // ─── Statistics ────────────────────────────────────────────────────────

    struct Stats {
        size_t total_discoveries = 0;
        size_t successful_validations = 0;
        size_t total_exports = 0;
        float avg_structure_confidence = 0.0f;
        size_t total_sft_exported = 0;
        size_t total_dpo_exported = 0;
    };
    const Stats& getStats() const { return stats_; }

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    StructureDiscoveryConfig config_;
    memory::EpisodicStore* episodic_store_ = nullptr;
    causality::CausalWorldModel* causal_world_model_ = nullptr;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    knowledge::ContradictionDetector* contradiction_detector_ = nullptr;
    simulation::VirtualSimulation* virtual_simulation_ = nullptr;
    simulation::BranchManagement* branch_management_ = nullptr;

    Stats stats_;
    std::chrono::steady_clock::time_point last_discovery_;
    uint32_t next_node_id_ = 1;

    // Helper: extract transition data from episodic store
    struct TransitionData {
        std::vector<std::vector<float>> states;
        std::vector<std::vector<float>> actions;
        std::vector<std::vector<float>> next_states;
        std::vector<float> rewards;
        std::vector<float> surprises;
        std::vector<uint64_t> episode_ids;
    };
    TransitionData extractTransitions(size_t max_episodes) const;

    // Helper: PC algorithm for causal discovery
    DiscoveredStructure runPCAlgorithm(const TransitionData& data);

    // Helper: intervention-based edge validation
    bool validateEdge(const DiscoveredStructure::Edge& edge, const TransitionData& data) const;

    // Helper: compute node centrality
    float computeCentrality(uint32_t node_id, const DiscoveredStructure& structure) const;

    // Helper: generate SFT samples from structure
    std::vector<std::string> generateSFTFromStructure(const DiscoveredStructure& structure) const;

    // Helper: generate DPO pairs from structure
    std::vector<std::pair<std::string, std::string>> generateDPOFromStructure(const DiscoveredStructure& structure) const;

    // Helper: write SFT JSONL
    size_t writeSFTJsonl(const std::string& path, const std::vector<std::string>& samples) const;

    // Helper: write DPO JSONL
    size_t writeDPOJsonl(const std::string& path, const std::vector<std::pair<std::string, std::string>>& pairs) const;
};

} // namespace sleep
} // namespace brain
} // namespace yuki