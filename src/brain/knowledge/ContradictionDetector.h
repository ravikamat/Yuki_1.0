#pragma once
// ContradictionDetector — Vector + Logic based contradiction detection
// Detects contradictions in HDC semantic graph and latent space

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/causality/CausalWorldModel.h"

namespace yuki {
namespace brain {
namespace knowledge {

using yuki::memory::HdcSemanticGraph;
using yuki::brain::predictive::LatentWorldModel;
using yuki::brain::causality::CausalWorldModel;

struct ContradictionConfig {
    // Vector-based detection
    float vector_similarity_threshold = 0.85f;  // Cosine similarity for near-duplicate detection
    float opposition_threshold = -0.7f;         // Negative similarity = contradiction
    int vector_check_knn = 10;                  // K-NN for contradiction search
    
    // Logic-based detection
    bool enable_logic_check = true;
    int max_proof_depth = 5;
    float logic_confidence_threshold = 0.8f;
    
    // Latent space detection
    float latent_contradiction_threshold = 0.8f;
    int latent_check_knn = 5;
    
    // Resolution
    bool auto_resolve = true;
    float resolution_confidence_threshold = 0.7f;
};

struct Contradiction {
    enum class Type {
        VECTOR_OPPOSITION,      // HDC vectors point in opposite directions
        LOGIC_INCONSISTENCY,    // Logical proof of inconsistency
        LATENT_OPPOSITION,      // Latent states encode opposite predictions
        CAUSAL_CYCLE,           // Causal graph has contradictory cycles
        TEMPORAL_INCONSISTENCY  // Same entity has contradictory states over time
    };
    
    Type type;
    std::string entity_a;
    std::string relation;
    std::string entity_b;
    std::string statement_a;
    std::string statement_b;
    float confidence = 0.0f;
    float severity = 0.0f;  // 0-1, how damaging is this contradiction
    uint64_t timestamp = 0;
    
    // Evidence
    std::vector<std::string> sources_a;
    std::vector<std::string> sources_b;
    std::vector<std::string> reasoning_chain;
};

struct Resolution {
    enum class Strategy {
        KEEP_BOTH,           // Both true in different contexts
        PREFER_RECENT,       // Prefer more recent evidence
        PREFER_RELIABLE,     // Prefer higher-reliability source
        MERGE_CONTEXT,       // Merge with context qualification
        DEFER_HUMAN          // Require human arbitration
    };
    
    Contradiction contradiction;
    Strategy strategy;
    std::string resolved_statement;
    float confidence = 0.0f;
    bool applied = false;
};

class ContradictionDetector {
public:
    explicit ContradictionDetector(const ContradictionConfig& cfg = ContradictionConfig{});
    ~ContradictionDetector() = default;
    
    void setHdcGraph(HdcSemanticGraph* hdc) { hdc_ = hdc; }
    void setLatentWorldModel(LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setCausalWorldModel(CausalWorldModel* cwm) { causal_world_model_ = cwm; }
    
    // ─── Detection ───────────────────────────────────────────────────────────
    
    // Detect contradictions for a new fact being added
    std::vector<Contradiction> detectForFact(
        const std::string& subject,
        const std::string& relation,
        const std::string& object,
        float confidence = 1.0f);
    
    // Batch scan for contradictions in entire graph
    std::vector<Contradiction> scanAll(float min_severity = 0.3f);
    
    // Detect contradictions in latent space predictions
    std::vector<Contradiction> detectLatentContradictions(
        const std::array<predictive::RSSMState, 5>& latent_states);
    
    // Detect causal contradictions (cycles, inconsistent interventions)
    std::vector<Contradiction> detectCausalContradictions();
    
    // ─── Resolution ──────────────────────────────────────────────────────────
    
    // Auto-resolve contradictions based on strategy
    std::vector<Resolution> resolve(const std::vector<Contradiction>& contradictions);
    
    // Apply resolution to HDC graph / latent model
    bool applyResolution(const Resolution& resolution);
    
    // ─── Query ───────────────────────────────────────────────────────────────
    
    const std::vector<Contradiction>& getActiveContradictions() const { return active_contradictions_; }
    const std::vector<Resolution>& getResolutions() const { return resolutions_; }
    
    // Statistics
    struct Stats {
        size_t total_detected = 0;
        size_t total_resolved = 0;
        size_t by_type[5] = {0};
    };
    const Stats& getStats() const { return stats_; }

private:
    ContradictionConfig config_;
    HdcSemanticGraph* hdc_ = nullptr;
    LatentWorldModel* latent_world_model_ = nullptr;
    CausalWorldModel* causal_world_model_ = nullptr;
    
    std::vector<Contradiction> active_contradictions_;
    std::vector<Resolution> resolutions_;
    Stats stats_;
    
    // Vector-based detection
    std::vector<Contradiction> detectVectorContradictions(
        const std::string& subject, const std::string& relation, const std::string& object);
    
    // Logic-based detection
    std::vector<Contradiction> detectLogicContradictions(
        const std::string& subject, const std::string& relation, const std::string& object);
    
    // Latent-based detection
    std::vector<Contradiction> detectLatentSpaceContradictions(
        const std::array<predictive::RSSMState, 5>& latent_states);
    
    // Resolution strategies
    Resolution resolvePreferRecent(const Contradiction& c);
    Resolution resolvePreferReliable(const Contradiction& c);
    Resolution resolveMergeContext(const Contradiction& c);
    
    // Helpers
    float computeVectorOpposition(const std::string& a, const std::string& b) const;
    bool checkLogicConsistency(const std::string& a, const std::string& b, std::vector<std::string>& chain) const;
    std::string getOppositeRelation(const std::string& relation) const;
    void applyToHdc(const Resolution& r);
    void applyToLatent(const Resolution& r);
};

} // namespace knowledge
} // namespace brain
} // namespace yuki