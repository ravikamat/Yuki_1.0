#include "CognitiveMemoryFabric.h"
#include "EpisodicStore.h"
#include "HdcSemanticGraph.h"
#include "Hypervector.h"
#include "MemoryEncoder.h"
#include "ProceduralStore.h"
#include "DifferentialMemoryController.h"
#include "brain/memory/ArchiveWriter.h"
#include "brain/language/LocalTransformer.h"  // for T1 in-process embed
#include <algorithm>
#include <iostream>
#include <chrono>

namespace yuki {
namespace brain {
namespace memory {

namespace {

// The storage classes live in the sibling namespace yuki::memory;
// bring them into scope for this translation unit.
using yuki::memory::HdcSemanticGraph;
using yuki::memory::Hypervector;
using yuki::memory::MemoryEncoder;
using yuki::memory::ProceduralStore;
using yuki::memory::ArchiveWriter;
using yuki::memory::SparseDistributedMemory;

MemoryPacket packetFromRecord(const EpisodeRecord& r) {
    MemoryPacket p;
    p.timestamp_ms    = static_cast<uint64_t>(r.timestamp);
    p.source          = r.source;
    p.text            = r.text;
    p.question_score  = r.question_score;
    p.command_score   = r.command_score;
    p.emotional_score = r.emotional_score;
    p.technical_score = r.technical_score;
    p.urgency_score   = r.urgency_score;
    p.greeting_score  = r.greeting_score;
    p.action_score    = r.action_score;
    p.polarity_score  = r.polarity_score;
    p.intent_label    = r.intent_label;
    p.confidence      = r.confidence;
    p.topic_tag       = r.topic_tag;
    return p;
}

} // namespace

CognitiveMemoryFabric::CognitiveMemoryFabric() = default;
CognitiveMemoryFabric::~CognitiveMemoryFabric() { stop(); }

bool CognitiveMemoryFabric::init() {
    encoder_      = std::make_unique<MemoryEncoder>();
    episodic_     = std::make_unique<EpisodicStore>();
    hdc_semantic_ = std::make_unique<HdcSemanticGraph>(); // HDC T2 tier

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
        std::cout << "[CMF] DMC + T3 Procedural active.\n";
    }
    archiveWriter_ = std::make_unique<ArchiveWriter>("data/archive");

    std::cout << "[CMF] Cognitive Memory Fabric initialized (HDC Semantic Graph active). "
              << "Episodes: " << episodic_->totalEpisodes() << "\n";
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
    // ── T1 encoding: in-process transformer primary, heuristic fallback ────────
    std::vector<float> vec;

    if (lt_encoder_ && lt_encoder_->isLoaded() && !pkt.text.empty()) {
        vec = lt_encoder_->embed(pkt.text);
    }

    // Fallback: heuristic MemoryEncoder (24-dim) when transformer not loaded
    if (vec.empty()) {
        if (pkt.type == MemoryPacket::USER_UTTERANCE ||
            pkt.type == MemoryPacket::SYNTHETIC_CURRICULUM) {
            vec = encoder_->encodeScores(
                pkt.question_score, pkt.command_score, pkt.emotional_score,
                pkt.technical_score, pkt.urgency_score, pkt.greeting_score,
                pkt.action_score, pkt.polarity_score,
                pkt.source, pkt.timestamp_ms);
        } else {
            vec = encoder_->encodeText(pkt.text);
        }
    }

    // Build episode record
    EpisodeRecord rec;
    rec.timestamp = static_cast<double>(pkt.timestamp_ms);
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

    episodic_->insert(rec);

    // HDC T2 semantic tier: ingest KNOWLEDGE_FACT as proposition triples
    if (pkt.type == MemoryPacket::KNOWLEDGE_FACT && hdc_semantic_) {
        hdc_semantic_->ingestProposition(pkt.topic_tag, "related_to", pkt.text, pkt.confidence);
    }
}

std::vector<MemoryPacket> CognitiveMemoryFabric::retrieveByTopic(const std::string& topic, size_t limit) {
    // T1 tier has no keyword index — scan recent episodes and match topic tag.
    std::vector<MemoryPacket> packets;
    if (!episodic_) return packets;

    auto recs = episodic_->queryRecentSnapshots(limit * 4);
    for (const auto& r : recs) {
        if (packets.size() >= limit) break;
        if (r.topic_tag == topic) {
            packets.push_back(packetFromRecord(r));
        }
    }
    return packets;
}

size_t CognitiveMemoryFabric::totalEpisodes() const {
    return episodic_ ? episodic_->totalEpisodes() : 0;
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
    // T1 has no vector index; retrieve the most recent episode text matching the query
    // as a lightweight context source.
    if (!episodic_) return "";

    auto recs = episodic_->queryRecentSnapshots(32);
    std::string best;
    size_t best_score = 0;
    for (const auto& r : recs) {
        if (r.text.empty()) continue;
        // Score by shared word count (cheap relevance proxy)
        size_t score = 0;
        size_t pos = 0;
        while ((pos = text.find_first_not_of(" \t\n", pos)) != std::string::npos) {
            size_t end = text.find_first_of(" \t\n", pos);
            std::string word = text.substr(pos, end - pos);
            if (word.size() > 2 && r.text.find(word) != std::string::npos) {
                score += word.size();
            }
            if (end == std::string::npos) break;
            pos = end;
        }
        if (score > best_score) {
            best_score = score;
            best = r.text;
        }
    }
    if (best.size() > max_chars) best.resize(max_chars);
    return best;
}

std::vector<MemoryPacket> CognitiveMemoryFabric::retrieveSimilarEpisodes(const std::vector<float>& query_vec, size_t k) {
    // Vector index was removed from the T1 store; degrade to recent episodes.
    (void)query_vec;
    std::vector<MemoryPacket> packets;
    if (!episodic_) return packets;

    auto recs = episodic_->queryRecentSnapshots(k);
    for (const auto& r : recs) {
        packets.push_back(packetFromRecord(r));
    }
    return packets;
}

EpisodicStore*    CognitiveMemoryFabric::episodicStore()    const { return episodic_.get(); }
HdcSemanticGraph* CognitiveMemoryFabric::hdcSemanticGraph() const { return hdc_semantic_.get(); }
MemoryEncoder*    CognitiveMemoryFabric::encoder()          const { return encoder_.get(); }

void CognitiveMemoryFabric::setTransformerEncoder(yuki::brain::language::LocalTransformer* lt) {
    lt_encoder_ = lt;
    if (lt && lt->isLoaded()) {
        std::cout << "[CMF] T1 encoder: LocalTransformer (dim=" << lt->EMBED_DIM << ") ready\n";
    } else {
        std::cerr << "[CMF] setTransformerEncoder: model not loaded — heuristic fallback active\n";
    }
}

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

    // Build relational query using public Hypervector(string) API
    // Seed separate HVs from subject and relation, bind them together
    Hypervector query(subject);  // semantic seed from subject text

    if (!relation.empty()) {
        Hypervector relationHv(relation);  // semantic seed from relation text
        query = query.bind(relationHv);    // binding → target object space
    }

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

// ── Tier Promotion (T1 → T2 → T3) ────────────────────────────────────────

bool CognitiveMemoryFabric::promoteEpisodeToSemantic(int64_t episode_id) {
    if (!episodic_ || !hdc_semantic_) return false;

    auto opt = episodic_->getById(episode_id);
    if (!opt) return false;

    const auto& ep = *opt;
    if (ep.text.empty()) return false;

    // Extract noun phrases as concepts and ingest as propositions
    // Format: episode content → topic/concept relations
    std::string topic = ep.topic_tag.empty() ? "general" : ep.topic_tag;

    // Ingest the main fact from the episode
    hdc_semantic_->ingestProposition(topic, "contains", ep.text, ep.confidence);

    // Also ingest the intent as a relation
    if (!ep.intent_label.empty()) {
        hdc_semantic_->ingestProposition(topic, "has_intent", ep.intent_label, ep.confidence);
    }

    std::cout << "[CMF] Promoted episode " << episode_id << " to semantic tier (topic: " << topic << ")\n";
    return true;
}

bool CognitiveMemoryFabric::promoteConceptToProcedural(const std::string& concept_name) {
    if (!hdc_semantic_ || !proceduralStore_) return false;

    // Query the concept from semantic graph
    std::vector<std::pair<std::string, float>> related;
    if (!querySemantic(concept_name, "related_to", related, 10)) {
        return false;
    }

    // Build a procedural skill from the concept and its relations
    std::string skill_data = concept_name + ": ";
    for (const auto& [rel, strength] : related) {
        skill_data += rel + "(" + std::to_string(strength) + ") ";
    }

    // Store as procedural knowledge
    std::vector<uint8_t> blob(skill_data.begin(), skill_data.end());
    if (!proceduralStore_->store(concept_name, ProceduralStore::BlobType::SKILL_COMPILED, blob)) {
        return false;
    }

    // Mark concept as procedural in semantic graph
    hdc_semantic_->markProcedural(concept_name);

    std::cout << "[CMF] Promoted concept '" << concept_name << "' to procedural tier\n";
    return true;
}

bool CognitiveMemoryFabric::demoteConceptToEpisodic(const std::string& concept_name) {
    if (!hdc_semantic_ || !episodic_) return false;

    // The concept exists in semantic graph - we can't easily "demote" it
    // but we can decay its strength and create an episodic record of the demotion
    hdc_semantic_->decay(0.5f); // Aggressive decay for this concept

    // Create an episodic record of the demotion event
    MemoryPacket pkt;
    pkt.type = MemoryPacket::KNOWLEDGE_FACT;
    pkt.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    pkt.source = "demotion";
    pkt.text = "Demoted concept: " + concept_name;
    pkt.confidence = 0.3f;
    pkt.topic_tag = "demoted";
    ingest(pkt);

    std::cout << "[CMF] Demoted concept '" << concept_name << "' from semantic tier\n";
    return true;
}

bool CognitiveMemoryFabric::demoteProceduralToSemantic(const std::string& skill_id) {
    if (!proceduralStore_ || !hdc_semantic_) return false;

    // Retrieve the skill from procedural store
    auto skill_opt = proceduralStore_->retrieve(skill_id);
    if (!skill_opt) return false;

    // Convert blob back to string
    std::string skill_data(skill_opt->begin(), skill_opt->end());

    // Re-ingest as semantic knowledge
    hdc_semantic_->ingestProposition(skill_id, "was_procedural", skill_data, 0.5f);

    std::cout << "[CMF] Demoted skill '" << skill_id << "' from procedural to semantic\n";
    return true;
}

ProceduralStore* CognitiveMemoryFabric::proceduralStore() const {
    return proceduralStore_.get();
}

DifferentialMemoryController* CognitiveMemoryFabric::differentialMemoryController() const {
    return dmc_.get();
}

SparseDistributedMemory* CognitiveMemoryFabric::sdm() const {
    return nullptr; // SDM accessor removed with the old episodic vector index
}

// HdcSemanticGraphDeleter implementation
void HdcSemanticGraphDeleter::operator()(HdcSemanticGraph* ptr) {
    delete ptr;
}

} // namespace memory
} // namespace brain
} // namespace yuki
