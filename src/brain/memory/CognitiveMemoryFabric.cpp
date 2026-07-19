#include "CognitiveMemoryFabric.h"
#include "EpisodicStore.h"
#include "HdcSemanticGraph.h"
#include "Hypervector.h"
#include "MemoryEncoder.h"
#include "ProceduralStore.h"
#include "DifferentialMemoryController.h"
#include "brain/memory/ArchiveWriter.h"
#include <algorithm>
#include <iostream>
#include <chrono>

namespace yuki {
namespace memory {

CognitiveMemoryFabric::CognitiveMemoryFabric() = default;
CognitiveMemoryFabric::~CognitiveMemoryFabric() { stop(); }

bool CognitiveMemoryFabric::init() {
    encoder_      = std::make_unique<MemoryEncoder>();
    episodic_     = std::make_unique<EpisodicStore>();
    hdc_semantic_ = std::make_unique<HdcSemanticGraph>(); // HDC T2 tier

    if (!episodic_->init()) {
        std::cerr << "[CMF] EpisodicStore init failed.\n";
        return false;
    }
    if (!hdc_semantic_->init()) {
        std::cerr << "[CMF] HdcSemanticGraph init failed.\n";
        return false;
    }
    // T3 Procedural + DMC
    proceduralStore_ = std::make_unique<ProceduralStore>();
    if (!proceduralStore_->init("data/brain/procedural.db", "data/procedural/")) {
        std::cerr << "[CMF] ProceduralStore init failed. T3 unavailable.\n";
    } else {
        dmc_ = std::make_unique<DifferentialMemoryController>();
        if (!dmc_->init(proceduralStore_.get(), "dmc_primary_weights")) {
            std::cerr << "[CMF] DMC init failed. Adaptive memory unavailable.\n";
        } else {
            std::cout << "[CMF] DMC + T3 Procedural active.\n";
        }
    }
    archiveWriter_ = std::make_unique<ArchiveWriter>("data/archive");

    std::cout << "[CMF] Cognitive Memory Fabric initialized (HDC Semantic Graph active). "
              << "Episodes: " << episodic_->count() << "\n";
    return true;
}

void CognitiveMemoryFabric::start() {
    // Register the packet processor with the lock-free event loop
    event_loop_.setPacketProcessor(
        [this](const MemoryPacket& pkt) { processPacket(pkt); });
    event_loop_.start();
}

void CognitiveMemoryFabric::stop() {
    // stop() drains the ring buffer (all pending packets processed) then joins.
    event_loop_.stop();
    if (episodic_) {
        episodic_->saveIndex();
        std::cout << "[CMF] HNSW index saved.\n";
    }
    std::cout << "[CMF] Background memory thread stopped.\n";
}

void CognitiveMemoryFabric::ingest(const MemoryPacket& packet) {
    // Lock-free enqueue; returns false if ring buffer is full (backpressure).
    // Bounded memory: no unbounded queue growth regardless of producer rate.
    if (!event_loop_.enqueue(packet)) {
        static size_t drops = 0;
        ++drops;
        if (drops % 1000 == 1) {
            std::cerr << "[CMF] WARNING: event queue full — packet dropped "
                      << "(total drops: " << drops << ").\n";
        }
    }
}
// workerLoop() removed — replaced by EventLoopCore::run()

void CognitiveMemoryFabric::processPacket(const MemoryPacket& pkt) {
    // Encode to vector
    std::vector<float> vec;
    if (pkt.type == MemoryPacket::USER_UTTERANCE || pkt.type == MemoryPacket::SYNTHETIC_CURRICULUM) {
        vec = encoder_->encodeScores(
            pkt.question_score, pkt.command_score, pkt.emotional_score,
            pkt.technical_score, pkt.urgency_score, pkt.greeting_score,
            pkt.action_score, pkt.polarity_score,
            pkt.source, pkt.timestamp_ms
        );
    } else {
        vec = encoder_->encodeText(pkt.text);
    }

    // Build episode record
    EpisodeRecord rec;
    rec.timestamp_ms = pkt.timestamp_ms;
    rec.source = pkt.source;
    rec.text = pkt.text;
    rec.question_score = pkt.question_score;
    rec.command_score = pkt.command_score;
    rec.emotional_score = pkt.emotional_score;
    rec.technical_score = pkt.technical_score;
    rec.urgency_score = pkt.urgency_score;
    rec.greeting_score = pkt.greeting_score;
    rec.action_score = pkt.action_score;
    rec.polarity_score = pkt.polarity_score;
    rec.intent_label = pkt.intent_label;
    rec.confidence = pkt.confidence;
    rec.topic_tag = pkt.topic_tag;

    episodic_->insert(rec, vec);

    // HDC T2 semantic tier: ingest KNOWLEDGE_FACT as proposition triples
    if (pkt.type == MemoryPacket::KNOWLEDGE_FACT && hdc_semantic_) {
        // Also call ingestFact for backward compatibility with text pipeline
        hdc_semantic_->ingestProposition(pkt.topic_tag, "related_to", pkt.text, pkt.confidence);
    }
}

std::vector<MemoryPacket> CognitiveMemoryFabric::retrieveSimilarToLast(size_t k) {
    // Placeholder: retrieve most recent k episodes
    // In Phase 2, this will use VectorStore KNN search
    auto recs = episodic_->retrieveByTopic("", k);
    std::vector<MemoryPacket> packets;
    for (const auto& r : recs) {
        MemoryPacket p;
        p.timestamp_ms = r.timestamp_ms;
        p.source = r.source;
        p.text = r.text;
        p.question_score = r.question_score;
        p.command_score = r.command_score;
        p.emotional_score = r.emotional_score;
        p.technical_score = r.technical_score;
        p.urgency_score = r.urgency_score;
        p.greeting_score = r.greeting_score;
        p.action_score = r.action_score;
        p.polarity_score = r.polarity_score;
        p.intent_label = r.intent_label;
        p.confidence = r.confidence;
        p.topic_tag = r.topic_tag;
        packets.push_back(p);
    }
    return packets;
}

std::vector<MemoryPacket> CognitiveMemoryFabric::retrieveByTopic(const std::string& topic, size_t limit) {
    auto recs = episodic_->retrieveByTopic(topic, limit);
    std::vector<MemoryPacket> packets;
    for (const auto& r : recs) {
        MemoryPacket p;
        p.timestamp_ms = r.timestamp_ms;
        p.source = r.source;
        p.text = r.text;
        p.question_score = r.question_score;
        p.command_score = r.command_score;
        p.emotional_score = r.emotional_score;
        p.technical_score = r.technical_score;
        p.urgency_score = r.urgency_score;
        p.greeting_score = r.greeting_score;
        p.action_score = r.action_score;
        p.polarity_score = r.polarity_score;
        p.intent_label = r.intent_label;
        p.confidence = r.confidence;
        p.topic_tag = r.topic_tag;
        packets.push_back(p);
    }
    return packets;
}

size_t CognitiveMemoryFabric::totalEpisodes() const {
    return episodic_ ? episodic_->count() : 0;
}

bool CognitiveMemoryFabric::ingestFact(const std::string& text, const std::string& topic, float confidence) {
    if (!hdc_semantic_) return false;
    // Forward as an HDC proposition: topic "related_to" text-summary
    hdc_semantic_->ingestProposition(topic, "related_to", text, confidence);
    return true;
}

std::vector<std::string> CognitiveMemoryFabric::getRelatedConcepts(const std::string& concept_name) {
    std::vector<std::string> names;
    if (!hdc_semantic_) return names;
    // Query: concept_name "related_to" ?
    std::vector<std::pair<std::string,float>> related;
    querySemantic(concept_name, "related_to", related, 20);
    for (const auto& [name, _] : related) names.push_back(name);
    return names;
}

std::string CognitiveMemoryFabric::retrieveContextForQuery(const std::string& text, size_t max_chars) {
    if (!encoder_ || !episodic_) return "";
    auto query_vec = encoder_->encodeText(text);
    return episodic_->retrieveContextString(query_vec, max_chars);
}

std::vector<MemoryPacket> CognitiveMemoryFabric::retrieveSimilarEpisodes(const std::vector<float>& query_vec, size_t k) {
    std::vector<MemoryPacket> packets;
    if (!episodic_) return packets;
    
    auto records = episodic_->retrieveSimilar(query_vec, k);
    for (const auto& r : records) {
        MemoryPacket p;
        p.timestamp_ms = r.timestamp_ms;
        p.source = r.source;
        p.text = r.text;
        p.question_score = r.question_score;
        p.command_score = r.command_score;
        p.emotional_score = r.emotional_score;
        p.technical_score = r.technical_score;
        p.urgency_score = r.urgency_score;
        p.greeting_score = r.greeting_score;
        p.action_score = r.action_score;
        p.polarity_score = r.polarity_score;
        p.intent_label = r.intent_label;
        p.confidence = r.confidence;
        p.topic_tag = r.topic_tag;
        packets.push_back(p);
    }
    return packets;
}

EpisodicStore*    CognitiveMemoryFabric::episodicStore()    const { return episodic_.get(); }
HdcSemanticGraph* CognitiveMemoryFabric::hdcSemanticGraph() const { return hdc_semantic_.get(); }
MemoryEncoder*    CognitiveMemoryFabric::encoder()          const { return encoder_.get(); }

void CognitiveMemoryFabric::ingestProposition(const std::string& subject,
                                               const std::string& relation,
                                               const std::string& object,
                                               float              confidence) {
    if (hdc_semantic_)
        hdc_semantic_->ingestProposition(subject, relation, object, confidence);
}

bool CognitiveMemoryFabric::querySemantic(const std::string& subject,
                                           const std::string& relation,
                                           std::vector<std::pair<std::string,float>>& out_objects,
                                           size_t limit) {
    if (!hdc_semantic_) return false;
    (void)subject;  // TODO: build subject.hv XOR relation.hv once getConcept is wired
    (void)relation;
    Hypervector query; // placeholder — querySimilar scores all concepts by default HV
    auto results = hdc_semantic_->querySimilar(query, limit);
    for (const auto& c : results)
        out_objects.emplace_back(c.name, c.strength);
    return !out_objects.empty();
}

bool CognitiveMemoryFabric::reinforceConcept(const std::string& name) {
    return hdc_semantic_ ? hdc_semantic_->reinforce(name) : false;
}

size_t CognitiveMemoryFabric::decayWeakConcepts(float /*threshold*/) {
    if (!hdc_semantic_) return 0;
    hdc_semantic_->decay(0.95f);
    return 0; // HdcSemanticGraph::decay doesn't return count yet
}

ProceduralStore* CognitiveMemoryFabric::proceduralStore() const {
    return proceduralStore_.get();
}

DifferentialMemoryController* CognitiveMemoryFabric::differentialMemoryController() const {
    return dmc_.get();
}

} // namespace memory
} // namespace yuki
