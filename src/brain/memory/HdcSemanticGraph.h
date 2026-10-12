#pragma once
#include "Hypervector.h"
#include "NeuralPopulation.h"  // PACL Phase 1: dual representation layer
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <array>
#include <random>
#include <chrono>
#include <tuple>

struct sqlite3;      // forward declaration (vendor/sqlite/sqlite3.h included in .cpp)
struct sqlite3_stmt; // forward declaration

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

// Provenance-aware weight structure
struct TripleWeight {
    float base_weight = 0.0f;              // Initial weight from source (ConceptNet, web, user, etc.)
    uint32_t confirmation_count = 0;       // Times this triple was confirmed correct
    uint32_t refutation_count = 0;         // Times this triple was corrected/refuted
    uint64_t last_confirmed_ts = 0;        // Unix timestamp of last confirmation
    std::vector<std::string> sources;      // Source tags: {"conceptnet", "web:2026-09", "user_confirm"}

    // Computed effective weight with provenance decay
    float effectiveWeight(float time_decay_rate = 0.001f) const {
        if (confirmation_count == 0 && refutation_count == 0) return base_weight;
        
        float pos = static_cast<float>(confirmation_count);
        float neg = static_cast<float>(refutation_count);
        float ratio = (pos + 1.0f) / (pos + neg + 2.0f);  // Laplace smoothing
        
        // Time decay on base weight
        uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        float time_factor = 1.0f;
        if (last_confirmed_ts > 0 && now > last_confirmed_ts) {
            float days = static_cast<float>(now - last_confirmed_ts) / 86400.0f;
            time_factor = std::exp(-time_decay_rate * days);
        }
        
        return base_weight * ratio * time_factor;
    }
    
    void confirm(const std::string& source_tag) {
        confirmation_count++;
        last_confirmed_ts = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        sources.push_back(source_tag);
    }
    
    void refute(const std::string& source_tag) {
        refutation_count++;
        sources.push_back(source_tag + ":refuted");
    }
};

// HDC-encoded edge: bound hypervector = subj.identity XOR rel_hv XOR obj.identity
struct HdcEdge {
    int64_t        from_id       = -1;
    int64_t        to_id         = -1;
    std::string    relation_type;
    Hypervector    bound_vector;   // subject ⊗ relation ⊗ object (XOR bind)
    TripleWeight   weight;         // provenance-aware weight with BKT/ELO calibration
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
    ~HdcSemanticGraph();
    bool init();

    // Ingest one proposition into the graph.
    // Creates concepts if they don't exist; stores bound HV as an edge.
    bool ingestProposition(const std::string& subject,
                           const std::string& relation,
                           const std::string& object,
                           float              confidence);

    // Provenance-aware ingest with full TripleWeight
    bool ingestProposition(const std::string& subject,
                           const std::string& relation,
                           const std::string& object,
                           const TripleWeight& weight);

    // Bulk ingest: (subject, relation, object, confidence) propositions in a
    // single transaction — bootstrap-scale loads (ConceptNet-sized corpora)
    // must not pay a lock round-trip per proposition. All-or-nothing: any
    // failed proposition rolls back the whole batch.
    bool ingestPropositions(
        const std::vector<std::tuple<std::string, std::string, std::string, float>>& props);

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

    // Provenance tracking
    bool confirmTriple(const std::string& subject, const std::string& relation, const std::string& object, const std::string& source_tag);
    bool refuteTriple(const std::string& subject, const std::string& relation, const std::string& object, const std::string& source_tag);
    std::vector<std::pair<std::string, float>> getNearestNeighborsWeighted(const std::string& concept_name, size_t k = 10);

    // A full (subject, relation, object) proposition retrieved from the graph,
    // as opposed to getNearestNeighborsWeighted's bare concept neighbour.
    // Used by the per-turn learning hook to credit/debit the facts a turn
    // actually relied on.
    struct RetrievedTriple {
        std::string subject;
        std::string relation;
        std::string object;
        float       weight = 0.0f;
    };
    // Read-only: does not create concepts. Incoming and outgoing edges of
    // `concept` are merged and ordered by effective weight, then capped at k.
    std::vector<RetrievedTriple> getTriplesFor(const std::string& concept_name, size_t k = 8);

    // Stats & counts
    size_t getConceptCount() const;
    size_t getEdgeCount() const;

    // Transaction management for high-throughput batch ingestion
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

private:
    std::string db_path_;

    // Persistent SQLite connection, opened lazily by conn() and closed in the
    // destructor. Every public/private method must hold db_mtx_ while using
    // the handle. (Per-call sqlite3_open/close made each ingest cost ~6 file
    // open cycles — 60K ingests were ~350K filesystem operations.)
    mutable std::recursive_mutex db_mtx_;
    mutable sqlite3*             db_ = nullptr;

    // Returns the cached connection (caller must hold db_mtx_). Opens and
    // configures it (WAL + synchronous=NORMAL) on first use.
    sqlite3* conn() const;

    // In-memory cache: relation_name → deterministic Hypervector
    std::unordered_map<std::string, Hypervector> relation_memory_;
    mutable std::mutex                            relation_mtx_;

    // Concept identity cache: name → (id, identity HV). Concepts are
    // insert-only with name UNIQUE and an immutable HV, so entries never go
    // stale. Cuts two SELECTs plus two ~2.5 KB hex decodes per ingest.
    mutable std::mutex concept_cache_mtx_;
    std::unordered_map<std::string, std::pair<int64_t, Hypervector>> concept_cache_;

    // Cache-aware lookup-or-create; returns {id, identity}, id < 0 on failure.
    std::pair<int64_t, Hypervector> conceptIdentity(const std::string& name,
                                                    const std::string& type);

    // Prepared-statement cache for the hot ingest path (parse once, reuse).
    // Returned statements are owned by the cache — callers reset/clear them,
    // never finalize. Requires db_mtx_ held and the connection open.
    sqlite3_stmt* cachedStmt(const char* sql);
    std::unordered_map<std::string, sqlite3_stmt*> stmt_cache_;

    Hypervector getRelationVector(const std::string& relation);
    bool        ensureSchema();
    int64_t     getOrCreateConcept(const std::string& name,
                                   const std::string& type);
    bool        createEdge(int64_t from, int64_t to,
                           const std::string& relation_type,
                           const Hypervector& bound,
                           const TripleWeight& weight);
    TripleWeight getTripleWeight(const std::string& subject,
                                 const std::string& relation,
                                 const std::string& object);
};

} // namespace yuki::memory
