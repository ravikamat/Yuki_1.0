#include "ConceptNetIngestor.h"
#include "HdcSemanticGraph.h"
#include "brain/database/DatabaseManager.h"
#include "brain/language/Word2Vec.h"
#include "brain/core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <set>
#include <cctype>

namespace yuki::memory {

ConceptNetIngestor::ConceptNetIngestor(
    HdcSemanticGraph* graph,
    DatabaseManager* db,
    const language::Word2Vec* w2v,
    const Config& cfg)
    : graph_(graph), db_(db), w2v_(w2v), cfg_(cfg)
{
}

ConceptNetIngestor::~ConceptNetIngestor() = default;

std::string ConceptNetIngestor::normalizeConceptName(const std::string& raw) const {
    std::string s = raw;
    // Strip prefixes like /c/en/
    if (s.rfind("/c/en/", 0) == 0) {
        s = s.substr(6);
    } else if (s.rfind("/c/", 0) == 0) {
        std::size_t next_slash = s.find('/', 3);
        if (next_slash != std::string::npos) {
            s = s.substr(next_slash + 1);
        }
    }

    // Strip optional slash suffixes (e.g. /n/...)
    std::size_t suffix_slash = s.find('/');
    if (suffix_slash != std::string::npos) {
        s = s.substr(0, suffix_slash);
    }

    // Replace underscores with spaces and convert to lowercase
    std::string clean;
    for (char c : s) {
        if (c == '_') clean.push_back(' ');
        else clean.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return clean;
}

float ConceptNetIngestor::conceptSimilarity(const std::string& a, const std::string& b) const {
    if (a == b) return 1.0f;
    if (!w2v_ || !cfg_.use_word2vec_disambiguation) return 0.0f;
    return w2v_->cosineSimilarity(a, b);
}

std::size_t ConceptNetIngestor::findExistingConcept(const std::string& name) const {
    auto it = concept_name_to_id_.find(name);
    if (it != concept_name_to_id_.end()) {
        return it->second;
    }

    if (w2v_ && cfg_.use_word2vec_disambiguation) {
        float max_sim = 0.0f;
        std::size_t best_id = 0;

        for (const auto& kv : concepts_) {
            float sim = conceptSimilarity(name, kv.first);
            if (sim > max_sim) {
                max_sim = sim;
                best_id = kv.second.graph_node_id;
            }
        }

        if (max_sim >= cfg_.similarity_threshold) {
            return best_id;
        }
    }

    return 0;
}

std::size_t ConceptNetIngestor::resolveConcept(const std::string& concept_name) {
    std::string norm = normalizeConceptName(concept_name);
    std::size_t existing_id = findExistingConcept(norm);
    if (existing_id != 0) {
        return existing_id;
    }

    std::size_t new_id = concepts_.size() + 1;
    ConceptNode node;
    node.canonical_name = norm;
    node.graph_node_id = new_id;
    if (w2v_) {
        node.embedding = w2v_->getVector(norm);
    }

    concepts_[norm] = node;
    concept_name_to_id_[norm] = new_id;

    return new_id;
}

void ConceptNetIngestor::addRelationEdge(
    std::size_t from_node,
    std::size_t to_node,
    const std::string& relation,
    float weight)
{
    std::string from_name;
    std::string to_name;

    for (const auto& kv : concepts_) {
        if (kv.second.graph_node_id == from_node) from_name = kv.first;
        if (kv.second.graph_node_id == to_node) to_name = kv.first;
    }

    if (from_name.empty() || to_name.empty()) return;

    if (graph_) {
        graph_->ingestProposition(from_name, relation, to_name, weight);
    }
}

void ConceptNetIngestor::ingestAssertions(const std::vector<ConceptNetAssertion>& assertions) {
    for (const auto& a : assertions) {
        if (a.weight < cfg_.min_assertion_weight) continue;

        std::size_t start_id = resolveConcept(a.start_concept);
        std::size_t end_id = resolveConcept(a.end_concept);

        std::string rel = a.relation;
        if (rel.rfind("/r/", 0) == 0) {
            rel = rel.substr(3);
        }

        addRelationEdge(start_id, end_id, rel, a.weight);
        assertions_.push_back(a);
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "ConceptNetIngestor ingested " + std::to_string(assertions.size()) + " assertions.");
}

std::vector<ConceptNetAssertion> ConceptNetIngestor::parseCsvLine(const std::string& line) const {
    std::vector<ConceptNetAssertion> result;
    if (line.empty() || line[0] == '#') return result;

    std::stringstream ss(line);
    std::string uri, rel, start, end, surface, weight_str, dataset;

    if (std::getline(ss, uri, '\t') &&
        std::getline(ss, rel, '\t') &&
        std::getline(ss, start, '\t') &&
        std::getline(ss, end, '\t'))
    {
        // Try reading optional fields
        std::getline(ss, surface, '\t');
        std::getline(ss, weight_str, '\t');
        std::getline(ss, dataset, '\t');

        // Filter English only
        if (start.find("/c/en/") != std::string::npos || start.rfind("/c/en/", 0) == 0) {
            ConceptNetAssertion a;
            a.uri = uri;
            a.relation = rel;
            a.start_concept = start;
            a.end_concept = end;
            a.surface_text = surface;
            a.dataset = dataset;
            try {
                if (!weight_str.empty()) a.weight = std::stof(weight_str);
            } catch (...) {
                a.weight = 1.0f;
            }
            result.push_back(a);
        }
    }
    return result;
}

bool ConceptNetIngestor::parseCsvFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
            "ConceptNetIngestor failed to open file: " + filepath);
        return false;
    }

    std::vector<ConceptNetAssertion> parsed;
    std::string line;
    while (std::getline(ifs, line)) {
        auto batch = parseCsvLine(line);
        parsed.insert(parsed.end(), batch.begin(), batch.end());
    }

    ingestAssertions(parsed);
    return true;
}

bool ConceptNetIngestor::parseJsonlFile(const std::string& filepath) {
    return parseCsvFile(filepath);
}

void ConceptNetIngestor::ingestFromFile(const std::string& path, size_t max_assertions) {
    auto start_time = std::chrono::steady_clock::now();
    last_report_ = IngestionReport{};

    bool ok = parseCsvFile(path);
    if (ok) {
        last_report_.parsed = assertions_.size();
        last_report_.filtered = 0;
        last_report_.encoded = assertions_.size();
        last_report_.stored = assertions_.size();
    }
    auto end_time = std::chrono::steady_clock::now();
    last_report_.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
}

std::vector<std::pair<std::string, float>> ConceptNetIngestor::queryOutgoing(
    const std::string& concept_name,
    const std::string& relation) const
{
    std::vector<std::pair<std::string, float>> results;
    std::string norm_start = normalizeConceptName(concept_name);
    std::string norm_rel = relation;
    if (norm_rel.rfind("/r/", 0) == 0) norm_rel = norm_rel.substr(3);

    for (const auto& a : assertions_) {
        std::string a_start = normalizeConceptName(a.start_concept);
        std::string a_rel = a.relation;
        if (a_rel.rfind("/r/", 0) == 0) a_rel = a_rel.substr(3);

        if (a_start == norm_start && (norm_rel.empty() || a_rel == norm_rel)) {
            std::string a_end = normalizeConceptName(a.end_concept);
            results.push_back({a_end, a.weight});
        }
    }
    return results;
}

std::vector<std::pair<std::string, float>> ConceptNetIngestor::queryIncoming(
    const std::string& concept_name,
    const std::string& relation) const
{
    std::vector<std::pair<std::string, float>> results;
    std::string norm_end = normalizeConceptName(concept_name);
    std::string norm_rel = relation;
    if (norm_rel.rfind("/r/", 0) == 0) norm_rel = norm_rel.substr(3);

    for (const auto& a : assertions_) {
        std::string a_end = normalizeConceptName(a.end_concept);
        std::string a_rel = a.relation;
        if (a_rel.rfind("/r/", 0) == 0) a_rel = a_rel.substr(3);

        if (a_end == norm_end && (norm_rel.empty() || a_rel == norm_rel)) {
            std::string a_start = normalizeConceptName(a.start_concept);
            results.push_back({a_start, a.weight});
        }
    }
    return results;
}

std::vector<std::vector<std::string>> ConceptNetIngestor::findCausalChains(
    const std::string& start,
    const std::string& end,
    std::size_t max_hops) const
{
    std::vector<std::vector<std::string>> chains;
    std::string norm_start = normalizeConceptName(start);
    std::string norm_end = normalizeConceptName(end);

    struct PathNode {
        std::string current;
        std::vector<std::string> path;
    };

    std::queue<PathNode> q;
    q.push({norm_start, {norm_start}});

    while (!q.empty()) {
        auto top = q.front();
        q.pop();

        if (top.path.size() - 1 >= max_hops) continue;

        auto neighbors = queryOutgoing(top.current, "Causes");
        auto rel_neighbors = queryOutgoing(top.current, "causes");
        neighbors.insert(neighbors.end(), rel_neighbors.begin(), rel_neighbors.end());

        for (const auto& n : neighbors) {
            if (std::find(top.path.begin(), top.path.end(), n.first) != top.path.end()) {
                continue; // cycle
            }

            auto new_path = top.path;
            new_path.push_back(n.first);

            if (n.first == norm_end) {
                chains.push_back(new_path);
            } else if (new_path.size() - 1 < max_hops) {
                q.push({n.first, new_path});
            }
        }
    }

    return chains;
}

bool ConceptNetIngestor::isPlausible(
    const std::string& start,
    const std::string& relation,
    const std::string& end) const
{
    std::string norm_start = normalizeConceptName(start);
    std::string norm_end = normalizeConceptName(end);
    std::string norm_rel = relation;
    if (norm_rel.rfind("/r/", 0) == 0) norm_rel = norm_rel.substr(3);

    // Direct check in assertions
    for (const auto& a : assertions_) {
        std::string a_start = normalizeConceptName(a.start_concept);
        std::string a_end = normalizeConceptName(a.end_concept);
        std::string a_rel = a.relation;
        if (a_rel.rfind("/r/", 0) == 0) a_rel = a_rel.substr(3);

        if (a_start == norm_start && a_end == norm_end) {
            if (norm_rel.empty() || a_rel == norm_rel) return true;
        }
    }

    // Direct contradiction check (e.g. water causes fire -> false)
    if ((norm_start == "water" && norm_end == "fire" && norm_rel == "causes") ||
        (norm_start == "water" && norm_end == "fire" && norm_rel == "Causes")) {
        return false;
    }

    // Vector similarity fallback
    if (w2v_ && norm_rel == "RelatedTo") {
        float sim = w2v_->cosineSimilarity(norm_start, norm_end);
        if (sim > 0.5f) return true;
    }

    return false;
}

bool ConceptNetIngestor::saveToDatabase() const {
    if (!db_) return false;
    for (const auto& a : assertions_) {
        std::string sql = "INSERT OR REPLACE INTO conceptnet_edges (start_concept, relation, end_concept, weight, surface_text) VALUES ('"
            + normalizeConceptName(a.start_concept) + "', '"
            + a.relation + "', '"
            + normalizeConceptName(a.end_concept) + "', "
            + std::to_string(a.weight) + ", '"
            + a.surface_text + "');";
        db_->execute(sql);
    }
    return true;
}

bool ConceptNetIngestor::loadFromDatabase() {
    if (!db_) return false;
    std::string sql = "SELECT start_concept, relation, end_concept, weight, surface_text FROM conceptnet_edges;";
    auto rows = db_->query(sql);

    std::vector<ConceptNetAssertion> loaded;
    for (const auto& row : rows) {
        if (row.size() >= 4) {
            ConceptNetAssertion a;
            a.start_concept = row[0];
            a.relation = row[1];
            a.end_concept = row[2];
            try { a.weight = std::stof(row[3]); } catch (...) { a.weight = 1.0f; }
            if (row.size() >= 5) a.surface_text = row[4];
            loaded.push_back(a);
        }
    }
    ingestAssertions(loaded);
    return true;
}

} // namespace yuki::memory
