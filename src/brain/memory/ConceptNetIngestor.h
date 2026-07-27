#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace yuki {
namespace language {
class Word2Vec;
}
}

class DatabaseManager;

namespace yuki {
namespace memory {


class HdcSemanticGraph;

struct ConceptNetAssertion {
    std::string uri;
    std::string relation;       // e.g., "/r/Causes"
    std::string start_concept;  // e.g., "/c/en/fire"
    std::string end_concept;    // e.g., "/c/en/burn"
    std::string surface_text;   // optional English sentence
    float weight = 1.0f;
    std::string dataset;        // e.g., "/d/conceptnet/4/en"
};

struct ConceptNode {
    std::string canonical_name;  // e.g., "fire"
    std::vector<std::string> aliases;
    std::vector<float> embedding;  // cached Word2Vec vector
    std::size_t graph_node_id = 0;
};

class ConceptNetIngestor {
public:
    struct Config {
        float similarity_threshold = 0.75f;  // for concept disambiguation
        float min_assertion_weight = 1.0f;   // filter low-weight edges
        std::size_t max_edges_per_concept = 100;
        bool use_word2vec_disambiguation = true;
    };

    ConceptNetIngestor(
        HdcSemanticGraph* graph,
        DatabaseManager* db,
        const language::Word2Vec* w2v,
        const Config& cfg = Config{});
    ~ConceptNetIngestor();  // Rule #26: out-of-line destructor

    // --- Streaming & KIP Ingestion ---
    struct IngestionReport {
        uint64_t parsed = 0;
        uint64_t filtered = 0;
        uint64_t encoded = 0;
        uint64_t stored = 0;
        uint64_t duration_ms = 0;
    };

    void ingestFromFile(const std::string& path, size_t max_assertions = 0);
    IngestionReport getLastReport() const { return last_report_; }

    // --- Parsing ---
    bool parseCsvFile(const std::string& filepath);
    bool parseJsonlFile(const std::string& filepath);

    // Ingest from raw assertion list
    void ingestAssertions(const std::vector<ConceptNetAssertion>& assertions);

    // --- Graph Operations ---
    std::size_t resolveConcept(const std::string& concept_name);

    void addRelationEdge(
        std::size_t from_node,
        std::size_t to_node,
        const std::string& relation,
        float weight);

    // --- Query API ---
    std::vector<std::pair<std::string, float>> queryOutgoing(
        const std::string& concept_name,
        const std::string& relation = "") const;

    std::vector<std::pair<std::string, float>> queryIncoming(
        const std::string& concept_name,
        const std::string& relation = "") const;

    std::vector<std::vector<std::string>> findCausalChains(
        const std::string& start,
        const std::string& end,
        std::size_t max_hops = 3) const;

    bool isPlausible(const std::string& start, const std::string& relation, const std::string& end) const;

    // --- Persistence ---
    bool saveToDatabase() const;
    bool loadFromDatabase();

    std::size_t assertionCount() const { return assertions_.size(); }
    std::size_t conceptCount() const { return concepts_.size(); }

private:
    HdcSemanticGraph* graph_;
    DatabaseManager* db_;
    const language::Word2Vec* w2v_;
    Config cfg_;

    std::vector<ConceptNetAssertion> assertions_;
    std::unordered_map<std::string, ConceptNode> concepts_;
    std::unordered_map<std::string, std::size_t> concept_name_to_id_;
    IngestionReport last_report_;

    // Internal methods
    std::string normalizeConceptName(const std::string& raw) const;
    std::vector<ConceptNetAssertion> parseCsvLine(const std::string& line) const;
    std::vector<ConceptNetAssertion> parseJsonLine(const std::string& line) const;
    float conceptSimilarity(const std::string& a, const std::string& b) const;
    std::size_t findExistingConcept(const std::string& name) const;
};

} // namespace memory
} // namespace yuki

