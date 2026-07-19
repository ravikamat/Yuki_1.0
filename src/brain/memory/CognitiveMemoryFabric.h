#pragma once
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include "HdcSemanticGraph.h"
#include "brain/core/EventLoopCore.h"

namespace yuki {
namespace memory {

class EpisodicStore;
class MemoryEncoder;
class MemoryEncoder;
class ProceduralStore;              // T3
class DifferentialMemoryController; // DMC
class ArchiveWriter;                // T4

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
    std::vector<MemoryPacket> retrieveSimilarToLast(size_t k = 5);
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
    HdcSemanticGraph*           hdcSemanticGraph()           const;
    MemoryEncoder*              encoder()                    const;
    ProceduralStore*              proceduralStore()              const;
    DifferentialMemoryController* differentialMemoryController() const;
    ArchiveWriter*                archiveWriter()                const { return archiveWriter_.get(); }

private:
    std::unique_ptr<EpisodicStore>    episodic_;
    std::unique_ptr<HdcSemanticGraph> hdc_semantic_;
    std::unique_ptr<MemoryEncoder>    encoder_;
    std::unique_ptr<ProceduralStore>              proceduralStore_;
    std::unique_ptr<DifferentialMemoryController> dmc_;
    std::unique_ptr<ArchiveWriter>                archiveWriter_;   // T4

    // ── Lock-free event loop (replaces mutex + cv + std::queue) ──────────
    yuki::core::EventLoopCore event_loop_;

    void processPacket(const MemoryPacket& pkt);
};

} // namespace memory
} // namespace yuki
