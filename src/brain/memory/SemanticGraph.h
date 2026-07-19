#pragma once
// DEPRECATED: replaced by HdcSemanticGraph in CognitiveMemoryFabric (Week 2).
// File preserved for reference. Removed from CMakeLists.txt compilation targets.
// Do NOT include this file in new code.
#include <string>
#include <vector>
#include <cstdint>

namespace yuki {
namespace memory {

struct ConceptNode {
    int64_t id = -1;
    std::string name;
    std::string type; // "entity", "relation", "topic", "skill"
    float strength = 0.0f;
    uint64_t first_seen_ms = 0;
    uint64_t last_accessed_ms = 0;
    int access_count = 0;
};

struct ConceptEdge {
    int64_t from_id = -1;
    int64_t to_id = -1;
    std::string relation_type; // "causes", "requires", "similar_to", "part_of"
    float weight = 0.0f;
};

class SemanticGraph {
public:
    explicit SemanticGraph(const std::string& db_path = "data/brain/cmf_episodes.db");
    bool init();

    // Ingest a fact: extract concepts, create edges, boost existing
    bool ingestFact(const std::string& text, const std::string& topic_tag, float confidence);

    // Query concepts related to a topic
    std::vector<ConceptNode> getRelatedConcepts(const std::string& concept_name, size_t limit = 10);

    // Enhanced concept extraction: noun phrases, not single words
    bool ingestFactEnhanced(const std::string& text, const std::string& topic_tag, float confidence);

    // Hebbian reinforcement: strengthen concept on access, decay weak ones
    bool reinforceConcept(const std::string& name, float boost = 0.05f);
    bool decayConcepts(float decay_rate = 0.01f);

    // Get concept web with relation types and strengths
    struct ConceptWebNode {
        std::string name;
        std::string relation_type;
        float weight = 0.0f;
        float concept_strength = 0.0f;
    };
    std::vector<ConceptWebNode> getConceptWeb(const std::string& concept_name, size_t limit = 10);

    // Batch decay for sleep/consolidation
    size_t decayBatch(float threshold = 0.1f);

private:
    std::string db_path_;

    bool ensureSchema();
    int64_t getOrCreateConcept(const std::string& name, const std::string& type);
    bool createEdge(int64_t from, int64_t to, const std::string& rel_type, float weight);
    std::vector<std::string> extractNounPhrases(const std::string& text);
    std::vector<std::pair<std::string, std::string>> inferRelations(const std::string& text);
};

} // namespace memory
} // namespace yuki
