#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "brain/memory/MerkleDAG.h"

// Forward: existing VectorStore from retrieval/
class VectorStore;
struct sqlite3;

// HDC memory subsystem
#include "brain/memory/Hypervector.h"
#include "brain/memory/SparseDistributedMemory.h"
#include "brain/memory/LocalitySensitiveHash.h"

namespace yuki {
namespace memory {

struct EpisodeRecord {
    int64_t id = -1;
    uint64_t timestamp_ms = 0;
    std::string source;
    std::string text;
    float question_score = 0.0f;
    float command_score = 0.0f;
    float emotional_score = 0.0f;
    float technical_score = 0.0f;
    float urgency_score = 0.0f;
    float greeting_score = 0.0f;
    float action_score = 0.0f;
    float polarity_score = 0.0f;
    std::string intent_label;
    float confidence = 0.0f;
    std::string topic_tag;
};

class EpisodicStore {
public:
    // Tamper-evidence result
    struct ChainVerification {
        bool        valid            = true;
        int64_t     first_broken_id  = -1;
        std::string expected_hash;
        std::string stored_hash;
    };

    explicit EpisodicStore(const std::string& db_path = "data/brain/cmf_episodes.db",
                           const std::string& index_path = "data/brain/cmf_vectors.index");
    ~EpisodicStore();

    bool init();
    bool insert(const EpisodeRecord& record, const std::vector<float>& vector);
    std::vector<EpisodeRecord> retrieveSimilar(const std::vector<float>& query_vec, size_t k = 5);
    std::vector<EpisodeRecord> retrieveByTopic(const std::string& topic, size_t limit = 50);
    size_t count() const;
    // Fetch a single episode by its integer ID (searches HDC map first, then DB)
    std::optional<EpisodeRecord> getById(int64_t id) const;

    // Context retrieval for TurnCoordinator — returns concatenated relevant text
    std::string retrieveContextString(const std::vector<float>& query_vec, size_t max_chars = 800);

    // ── Merkle chain API ──────────────────────────────────────────────────────
    // Verify tamper-evidence of all episodes for session_id (0 = default)
    ChainVerification verifyChain(int64_t session_id = 0);
    // Latest merkle_hash for session (empty if none)
    std::string getMerkleRoot(int64_t session_id = 0) const;

    // ── HDC/SDM interface (parallel to HNSW, non-breaking) ──────────────────
    // Insert episode with a precomputed 10K-bit hypervector (from HypervectorEncoder)
    bool insertHDC(const EpisodeRecord& record,
                   const yuki::memory::Hypervector& hv);
    // Retrieve episodes similar to query hypervector via LSH + SDM
    std::vector<EpisodeRecord> retrieveSimilarHDC(
        const yuki::memory::Hypervector& query, size_t k = 5);

    // Persist HNSW index + metadata
    bool saveIndex();
    bool loadIndex();

    // ── SleepThread query interface ────────────────────────────────────────
    // Lightweight episode_chain view for SleepThread (no HDC vectors)
    struct EpisodeSnapshot {
        int64_t episode_id   = -1;
        int64_t session_id   = 0;
        double  timestamp    = 0.0;   // seconds
        int64_t vector_slot  = -1;
        bool    consolidated = false;
        int     access_count = 0;
    };
    // Query episode_chain rows (consolidated_only=false → unconsolidated first)
    std::vector<EpisodeSnapshot> queryRecentSnapshots(size_t limit, bool consolidated_only) const;
    // Mark episode_chain row as consolidated by SleepThread
    void  markConsolidated(int64_t episode_id);
    // Return fraction of T1 episodes that co-occur within window_ms milliseconds
    float computeCooccurrence(const std::string& label_a, const std::string& label_b,
                              int64_t window_ms) const;
    // LSH diagnostic: collision rate 0–1 (high = tables need rebuild)
    float getLshCollisionRate() const;
    // Rebuild all LSH tables from the current in-memory SDM
    void  rebuildLshTables();

    bool   lshRehashing_needed() const { return getLshCollisionRate() > 0.5f; }

    // ── DMC interface ─────────────────────────────────────────────────────────
    struct MemoryStats {
        std::string id;                      // "ep_{episode_id}"
        size_t      accessCount        = 0;
        size_t      reinforcementCount = 0;  // proxy: same as accessCount
        double      ageHours           = 0.0;
        float       lastFreeEnergy     = 0.0f;
    };
    // Return stats for up to 500 most-accessed episodes (thread-safe)
    std::vector<MemoryStats> getAllMemoryStats() const;
    // Phase B: Reset reinforcement counter for an episode (prevent re-promotion spam).
    // id format: "ep_{episode_id}".
    void resetReinforcement(const std::string& id);

private:
    std::string db_path_;
    std::string index_path_ = "data/brain/cmf_vectors";
    bool index_loaded_ = false;
    std::unique_ptr<::VectorStore>               vector_store_;
    mutable std::mutex                            mtx_;
    int64_t                                       next_id_ = 0;
    
    ::sqlite3* db_ = nullptr;  // persistent connection
    bool openDb();           // open with WAL mode pragmas

    // HDC subsystem — parallel to HNSW
    std::unique_ptr<yuki::memory::SparseDistributedMemory> sdm_;
    std::unique_ptr<yuki::memory::LocalitySensitiveHash>   lsh_;
    std::unordered_map<uint64_t, EpisodeRecord>            id_to_hdc_record_;
    uint64_t                                               next_hdc_id_ = 1;

    bool ensureSchema();
    bool insertMetadata(const EpisodeRecord& record, int64_t vector_label);
    bool idExists(int64_t id) const;
    std::vector<EpisodeRecord> queryByIds(const std::vector<int64_t>& ids);

    // ── Merkle chain internals ────────────────────────────────────────────────
    MerkleDAG   merkle_dag_;
    bool        initChainSchema();
    std::string computeContentHash(int64_t session_id, double timestamp,
                                   int64_t vector_slot,
                                   const std::string& meta_json) const;
    // Returns last episode_id for session, sets out_merkle (64-zero string for root)
    int64_t     getLastEpisodeId(int64_t session_id, std::string& out_merkle) const;
};

} // namespace memory
} // namespace yuki
