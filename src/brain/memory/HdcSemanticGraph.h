#pragma once
#include "Hypervector.h"
#include "NeuralPopulation.h"  // PACL Phase 1: dual representation layer
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace yuki::memory {

// HDC-encoded concept: name + type + unique random hypervector identity
struct HdcConcept {
    int64_t     id               = -1;
    std::string name;
    std::string type;            // "entity", "relation", "topic", "skill"
    Hypervector identity;        // unique random HV assigned at concept creation
    float       strength         = 0.5f;
    uint64_t    first_seen_ms    = 0;
    uint64_t    last_accessed_ms = 0;
    int         access_count     = 0;

    // PACL Phase 1: population-coded dual representation.
    // Coexists with identity — does NOT replace it.
    // population is lazily initialized on first excite() call.
    mutable PopulationNode population;

    // HdcConcept is copyable/movable despite PopulationNode having deleted copy.
    // Copy: manually transfer atomic activation values.
    HdcConcept() = default;

    HdcConcept(const HdcConcept& o)
        : id(o.id), name(o.name), type(o.type)
        , identity(o.identity), strength(o.strength)
        , first_seen_ms(o.first_seen_ms)
        , last_accessed_ms(o.last_accessed_ms)
        , access_count(o.access_count)
    {
        population.concept_id = o.population.concept_id;
        population.vectors    = o.population.vectors;
        for (size_t i = 0; i < kPopulationSize; ++i) {
            population.activations_raw[i].store(
                o.population.activations_raw[i].load(std::memory_order_relaxed),
                std::memory_order_relaxed);
        }
    }

    HdcConcept& operator=(const HdcConcept& o) {
        if (this != &o) {
            id = o.id; name = o.name; type = o.type;
            identity = o.identity; strength = o.strength;
            first_seen_ms = o.first_seen_ms;
            last_accessed_ms = o.last_accessed_ms;
            access_count = o.access_count;
            population.concept_id = o.population.concept_id;
            population.vectors    = o.population.vectors;
            for (size_t i = 0; i < kPopulationSize; ++i) {
                population.activations_raw[i].store(
                    o.population.activations_raw[i].load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
            }
        }
        return *this;
    }

    HdcConcept(HdcConcept&&) = default;
    HdcConcept& operator=(HdcConcept&&) = default;

    // Backward-compat accessor: returns population consensus if the population
    // is active (firing rate > silence threshold), otherwise returns identity.
    Hypervector getPopulationVector() const {
        return (population.firingRate() > kSilenceThreshold)
            ? population.consensus()
            : identity;
    }
};

// HDC-encoded edge: bound hypervector = subj.identity XOR rel_hv XOR obj.identity
struct HdcEdge {
    int64_t     from_id       = -1;
    int64_t     to_id         = -1;
    std::string relation_type;
    Hypervector bound_vector;   // subject ⊗ relation ⊗ object (XOR bind)
    float       weight         = 0.0f;
};

// HdcSemanticGraph — proposition-level knowledge graph using HDC binding.
//
// Key idea:
//   Each concept gets a unique random HV ("identity").
//   A proposition (subject, relation, object) is stored as a SINGLE bound HV:
//       bound = subject.identity XOR relation_hv XOR object.identity
//   Querying: given subject + relation, decode object via
//       query = subject.identity XOR relation_hv
//       then find nearest concepts to query.
//
// Storage: SQLite (schema matches existing cmf_episodes.db file).
// Thread-safety: per-method mutex on relation_memory_ cache.
class HdcSemanticGraph {
public:
    explicit HdcSemanticGraph(
        const std::string& db_path = "data/brain/cmf_episodes.db");
    bool init();

    // Ingest one proposition into the graph.
    // Creates concepts if they don't exist; stores bound HV as an edge.
    bool ingestProposition(const std::string& subject,
                           const std::string& relation,
                           const std::string& object,
                           float              confidence);

    // Decode: given a query HV, find the top-k most similar concepts.
    // Used to answer "subject ⊗ relation → ?" queries.
    std::vector<HdcConcept> querySimilar(const Hypervector& query,
                                          size_t             limit = 10);

    // Hebbian reinforcement: boost concept.strength + update access metadata.
    bool reinforce(const std::string& concept_name);

    // Decay all concept strengths by decay_rate; prune edges below 0.01.
    bool decay(float decay_rate = 0.95f);

    // ── SleepThread interface ─────────────────────────────────────────────────
    // Return up to `limit` concepts ordered by access_count DESC
    std::vector<HdcConcept> getAllConcepts(size_t limit = 100) const;
    // Promote concept type to 'procedural' (T2→T3 marker)
    bool markProcedural(const std::string& concept_name);

    // ── DMC interface ───────────────────────────────────────────────────────────────
    struct ConceptStats {
        std::string id;                      // concept name
        size_t      accessCount        = 0;
        size_t      reinforcementCount = 0;
        double      ageHours           = 0.0;
        float       heuristicStrength  = 0.0f;
    };
    // Return stats for up to 500 most-accessed concepts
    std::vector<ConceptStats> getAllConceptStats() const;
    // Return concept HV as 64-dim float embedding (first 64 bytes of hv_hex / 255.0)
    std::vector<float> getConceptEmbedding(const std::string& name) const;
    // Phase B: Reset reinforcement counter for a concept (prevent re-promotion spam).
    void resetReinforcement(const std::string& concept_name);
    // Phase B: 2-arg overload — create/update concept with source_tag as type annotation.
    bool ingestProposition(const std::string& id, const std::string& source_tag);

private:
    std::string db_path_;

    // In-memory cache: relation_name → deterministic Hypervector
    std::unordered_map<std::string, Hypervector> relation_memory_;
    mutable std::mutex                            relation_mtx_;

    Hypervector getRelationVector(const std::string& relation);
    bool        ensureSchema();
    int64_t     getOrCreateConcept(const std::string& name,
                                   const std::string& type);
    bool        createEdge(int64_t from, int64_t to,
                           const std::string& relation_type,
                           const Hypervector& bound,
                           float weight);
};

} // namespace yuki::memory
