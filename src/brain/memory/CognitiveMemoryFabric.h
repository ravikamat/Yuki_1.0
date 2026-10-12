#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include "brain/core/EventLoopCore.h"
#include "brain/language/LocalTransformer.h"

// Forward declarations for types not included above
namespace yuki {
namespace brain {
namespace language { class LocalTransformer; }
} // namespace brain
} // namespace yuki

namespace yuki::memory { class HdcSemanticGraph; }  // real namespace — defined in HdcSemanticGraph.h
namespace yuki::memory { class Hypervector; }       // real namespace — defined in Hypervector.h
namespace yuki::memory { class MemoryEncoder; }
namespace yuki::memory { class ProceduralStore; }
namespace yuki::memory { class ArchiveWriter; }
namespace yuki::memory { class SparseDistributedMemory; }

namespace yuki {
namespace brain {
namespace memory {

struct HdcSemanticGraphDeleter {
    void operator()(yuki::memory::HdcSemanticGraph* ptr);
};

class EpisodicStore;
class DifferentialMemoryController; // DMC

// These live in the real yuki::memory namespace (declared above):
//   HdcSemanticGraph, Hypervector, MemoryEncoder, ProceduralStore,
//   ArchiveWriter, SparseDistributedMemory.

struct MemoryPacket {
    enum Type { USER_UTTERANCE, KNOWLEDGE_FACT, SYNTHETIC_CURRICULUM, SELF_MODEL };
    Type type = USER_UTTERANCE;
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

class CognitiveMemoryFabric {
public:
    CognitiveMemoryFabric();
    ~CognitiveMemoryFabric();

    bool init();
    void start();
    void stop();

    // Fire-and-forget ingest (thread-safe)
    void ingest(const MemoryPacket& packet);

    // Query interfaces
    std::vector<MemoryPacket> retrieveByTopic(const std::string& topic, size_t limit = 50);
    size_t totalEpisodes() const;

    // Retrieve relevant context for TurnCoordinator / response generation
    std::string retrieveContextForQuery(const std::string& text, size_t max_chars = 800);

    // Retrieve similar episodes by vector query
    std::vector<MemoryPacket> retrieveSimilarEpisodes(const std::vector<float>& query_vec, size_t k = 5);

    // Semantic graph — HDC T2 tier (replaces old keyword SemanticGraph)
    bool ingestFact(const std::string& text, const std::string& topic, float confidence);
    std::vector<std::string> getRelatedConcepts(const std::string& concept_name);

    // HDC proposition API (new Week 2 interface)
    void ingestProposition(const std::string& subject,
                           const std::string& relation,
                           const std::string& object,
                           float              confidence = 0.8f);
    bool querySemantic(const std::string& subject,
                       const std::string& relation,
                       std::vector<std::pair<std::string,float>>& out_objects,
                       size_t limit = 10);

    // Hebbian-style concept management (delegated to HdcSemanticGraph)
    bool reinforceConcept(const std::string& name);
    size_t decayWeakConcepts(float threshold = 0.1f);

    // Accessors for wiring
    EpisodicStore*              episodicStore()              const;
    yuki::memory::HdcSemanticGraph* hdcSemanticGraph()           const;
    ::yuki::memory::MemoryEncoder* encoder()                    const;
    ::yuki::memory::ProceduralStore* proceduralStore()              const;
    DifferentialMemoryController* differentialMemoryController() const;
    ::yuki::memory::SparseDistributedMemory* sdm()                  const;
    ::yuki::memory::ArchiveWriter* archiveWriter()                const { return archiveWriter_.get(); }

    // ── Tier Promotion (T1 → T2 → T3) ──────────────────────────────────────
    // Promote an episode from EpisodicStore (T1) to HdcSemanticGraph (T2)
    // by extracting propositions from the episode text and ingesting them.
    bool promoteEpisodeToSemantic(int64_t episode_id);
    
    // Promote a concept from HdcSemanticGraph (T2) to ProceduralStore (T3)
    // by extracting executable patterns/skills.
    bool promoteConceptToProcedural(const std::string& concept_name);
    
    // Demote a concept from T2 back to T1 (archive)
    bool demoteConceptToEpisodic(const std::string& concept_name);
    
    // Demote a procedural skill from T3 back to T2
    bool demoteProceduralToSemantic(const std::string& skill_id);

    /// Wire in-process LocalTransformer for T1 dense retrieval.
    /// Call before start(). If not called, falls back to heuristic MemoryEncoder.
    /// Does NOT take ownership — BabyMode owns the LocalTransformer instance.
    void setTransformerEncoder(::yuki::brain::language::LocalTransformer* lt);
    /// Returns the wired LocalTransformer (nullptr if not set).
    ::yuki::brain::language::LocalTransformer* getTransformerEncoder() const { return lt_encoder_; }

private:
    std::unique_ptr<EpisodicStore>    episodic_;
    std::unique_ptr<yuki::memory::HdcSemanticGraph> hdc_semantic_;
    std::unique_ptr<::yuki::memory::MemoryEncoder>    encoder_;          // heuristic fallback (kept)
    ::yuki::brain::language::LocalTransformer* lt_encoder_ = nullptr;  // non-owning; primary when set
    std::unique_ptr<::yuki::memory::ProceduralStore>              proceduralStore_;
    std::unique_ptr<DifferentialMemoryController> dmc_;
    std::unique_ptr<::yuki::memory::ArchiveWriter>                archiveWriter_;   // T4

    // ── Lock-free event loop (replaces mutex + cv + std::queue) ──────────
    ::yuki::core::EventLoopCore event_loop_;

    void processPacket(const MemoryPacket& pkt);
};

} // namespace memory
} // namespace brain
} // namespace yuki