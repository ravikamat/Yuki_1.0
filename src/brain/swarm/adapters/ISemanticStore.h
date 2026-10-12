#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <ctime>
#include "brain/database/DatabaseManager.h"
#include "brain/swarm/CognitiveStore.h"
#include "brain/swarm/MergeEngine.h"
#include "brain/core/SystemConfig.h"

namespace yuki {
namespace swarm {

struct Concept {
    std::string name;
    std::string category;
    float confidence = 1.0f;
};

struct Relation {
    std::string start_concept;
    std::string relation_type;
    std::string end_concept;
    float weight = 1.0f;
};

/// A learned semantic rule entry (word → canonical_tag/weight).
struct SemanticRule {
    std::string rule_type;
    std::string word;
    std::string canonical_tag;   // stores float weight as string
    float weight = 1.0f;
};

class ISemanticStore {
public:
    virtual ~ISemanticStore() = default;
    virtual bool ingestConcept(const Concept& c) = 0;
    virtual bool ingestRelation(const Relation& r) = 0;
    virtual std::vector<Concept> queryConceptByName(const std::string& name) = 0;
    virtual std::vector<Relation> queryRelationsFrom(const std::string& name) = 0;
    virtual bool learnFact(const std::string& topic, const std::string& fact, const std::string& source, float confidence) = 0;
    virtual std::vector<std::string> queryFacts(const std::string& topic) = 0;
    virtual bool learnSemanticRule(const std::string& rule_type, const std::string& word, const std::string& canonical_tag) = 0;
    virtual bool ingestConceptNetEdge(const std::string& start, const std::string& relation, const std::string& end, float weight) = 0;

    // --- Heuristic value extensions (HeuristicMemory layer) ---
    /// Query all stored SemanticRules for the given rule_type.
    virtual std::vector<SemanticRule> querySemanticRules(const std::string& rule_type) = 0;
    /// Load a scalar float from the fact store by key (returns defaultValue if missing).
    virtual float getParameter(const std::string& key, float defaultValue) = 0;
    /// Persist a scalar float as a learned fact.
    virtual void setParameter(const std::string& key, float value) = 0;
};

class LegacySemanticStore : public ISemanticStore {
public:
    LegacySemanticStore() {
        DatabaseManager::instance().init("yuki_global.db");
    }
    ~LegacySemanticStore() override = default;

    bool ingestConcept(const Concept& c) override {
        return DatabaseManager::instance().storeLearned(c.name, c.category, "legacy", c.confidence, std::time(nullptr));
    }
    bool ingestRelation(const Relation& r) override {
        return DatabaseManager::instance().execute("INSERT OR REPLACE INTO conceptnet_edges (start_node, relation, end_node, weight) VALUES ('"
            + r.start_concept + "', '" + r.relation_type + "', '" + r.end_concept + "', " + std::to_string(r.weight) + ");");
    }
    std::vector<Concept> queryConceptByName(const std::string& name) override {
        std::vector<Concept> results;
        auto facts = DatabaseManager::instance().queryAllFacts(name);
        for (const auto& f : facts) {
            results.push_back(Concept{name, f, 1.0f});
        }
        return results;
    }
    std::vector<Relation> queryRelationsFrom(const std::string& name) override {
        std::vector<Relation> results;
        auto rows = DatabaseManager::instance().query("SELECT start_node, relation, end_node, weight FROM conceptnet_edges WHERE start_node = '" + name + "';");
        for (const auto& row : rows) {
            if (row.size() >= 4) {
                results.push_back(Relation{row[0], row[1], row[2], std::stof(row[3])});
            }
        }
        return results;
    }
    bool learnFact(const std::string& topic, const std::string& fact, const std::string& source, float confidence) override {
        return DatabaseManager::instance().storeLearned(topic, fact, source, confidence, std::time(nullptr));
    }
    std::vector<std::string> queryFacts(const std::string& topic) override {
        return DatabaseManager::instance().queryAllFacts(topic);
    }
    bool learnSemanticRule(const std::string& rule_type, const std::string& word, const std::string& canonical_tag) override {
        return DatabaseManager::instance().saveSemanticRule(rule_type, word, canonical_tag);
    }
    std::vector<SemanticRule> querySemanticRules(const std::string& rule_type) override {
        std::vector<SemanticRule> results;
        auto rows = DatabaseManager::instance().query(
            "SELECT rule_type, word, canonical_tag FROM learned_semantic_rules WHERE rule_type = '" + rule_type + "';");
        for (const auto& row : rows) {
            if (row.size() < 3) continue;
            float w = 0.5f;
            try { w = std::stof(row[2]); } catch (...) {}
            results.push_back({row[0], row[1], row[2], w});
        }
        return results;
    }
    float getParameter(const std::string& key, float defaultValue) override {
        auto facts = queryFacts(key);
        if (!facts.empty()) {
            try { return std::stof(facts.front()); } catch (...) {}
        }
        return defaultValue;
    }
    void setParameter(const std::string& key, float value) override {
        learnFact(key, std::to_string(value), "heuristic", 1.0f);
    }
    bool ingestConceptNetEdge(const std::string& start, const std::string& relation, const std::string& end, float weight) override {
        return DatabaseManager::instance().execute("INSERT OR REPLACE INTO conceptnet_edges (start_node, relation, end_node, weight) VALUES ('"
            + start + "', '" + relation + "', '" + end + "', " + std::to_string(weight) + ");");
    }
};

class SwarmSemanticStore : public ISemanticStore {
private:
    StorePtr store_;
    std::unique_ptr<MergeEngine> merge_engine_;
public:
    SwarmSemanticStore() {
        auto& factory = CognitiveStoreFactory::instance();
        store_ = factory.getStore(StoreScope::SHARED_SEMANTIC);
        auto procStore = factory.getStore(StoreScope::SHARED_PROCEDURAL);
        merge_engine_ = std::make_unique<MergeEngine>(store_, procStore);
    }
    ~SwarmSemanticStore() override = default;

    bool ingestConcept(const Concept& c) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO hdc_concepts (agent_id, name, type, hv_hex, strength, vector_clock) VALUES ('shared', '"
                          + store_->sanitize(c.name) + "', '"
                          + store_->sanitize(c.category) + "', '', "
                          + std::to_string(c.confidence) + ", '0');";
        return store_->execute(sql);
    }

    bool ingestRelation(const Relation& r) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = std::string("INSERT OR REPLACE INTO hdc_edges (agent_id, from_id, to_id, relation_type, bound_hv_hex, weight, vector_clock) VALUES ('shared', ")
                          + "(SELECT id FROM hdc_concepts WHERE agent_id='shared' AND name='" + store_->sanitize(r.start_concept) + "'), "
                          + "(SELECT id FROM hdc_concepts WHERE agent_id='shared' AND name='" + store_->sanitize(r.end_concept) + "'), '"
                          + store_->sanitize(r.relation_type) + "', '', "
                          + std::to_string(r.weight) + ", '0');";
        return store_->execute(sql);
    }

    std::vector<Concept> queryConceptByName(const std::string& name) override {
        std::vector<Concept> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query("SELECT name, type, strength FROM hdc_concepts WHERE agent_id='shared' AND name = '" + store_->sanitize(name) + "';");
        for (const auto& row : rows) {
            if (row.size() < 3) continue;
            results.push_back(Concept{row[0], row[1], std::stof(row[2])});
        }
        return results;
    }

    std::vector<Relation> queryRelationsFrom(const std::string& name) override {
        std::vector<Relation> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query("SELECT c1.name, e.relation_type, c2.name, e.weight FROM hdc_edges e "
                                  "JOIN hdc_concepts c1 ON e.from_id = c1.id AND e.agent_id = c1.agent_id "
                                  "JOIN hdc_concepts c2 ON e.to_id = c2.id AND e.agent_id = c2.agent_id "
                                  "WHERE e.agent_id = 'shared' AND c1.name = '" + store_->sanitize(name) + "';");
        for (const auto& row : rows) {
            if (row.size() < 4) continue;
            results.push_back(Relation{row[0], row[1], row[2], std::stof(row[3])});
        }
        return results;
    }

    bool learnFact(const std::string& topic, const std::string& fact, const std::string& source, float confidence) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO learned_knowledge (agent_id, topic, fact, source, confidence, timestamp) VALUES ('shared', '"
                          + store_->sanitize(topic) + "', '"
                          + store_->sanitize(fact) + "', '"
                          + store_->sanitize(source) + "', "
                          + std::to_string(confidence) + ", "
                          + std::to_string(std::time(nullptr)) + ");";
        return store_->execute(sql);
    }

    std::vector<std::string> queryFacts(const std::string& topic) override {
        std::vector<std::string> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query("SELECT fact FROM learned_knowledge WHERE topic = '" + store_->sanitize(topic) + "';");
        for (const auto& row : rows) {
            if (!row.empty()) results.push_back(row[0]);
        }
        return results;
    }

    std::vector<SemanticRule> querySemanticRules(const std::string& rule_type) override {
        std::vector<SemanticRule> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query(
            "SELECT rule_type, word, canonical_tag FROM learned_semantic_rules WHERE rule_type = '" + store_->sanitize(rule_type) + "';");
        for (const auto& row : rows) {
            if (row.size() < 3) continue;
            float w = 0.5f;
            try { w = std::stof(row[2]); } catch (...) {}
            results.push_back({row[0], row[1], row[2], w});
        }
        return results;
    }
    float getParameter(const std::string& key, float defaultValue) override {
        if (!store_ || !store_->raw()) return defaultValue;
        auto rows = store_->query(
            "SELECT fact FROM learned_knowledge WHERE topic = '" + store_->sanitize(key) + "' LIMIT 1;");
        if (!rows.empty() && !rows[0].empty()) {
            try { return std::stof(rows[0][0]); } catch (...) {}
        }
        return defaultValue;
    }
    void setParameter(const std::string& key, float value) override {
        learnFact(key, std::to_string(value), "heuristic", 1.0f);
    }
    bool learnSemanticRule(const std::string& rule_type, const std::string& word, const std::string& canonical_tag) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO learned_semantic_rules (rule_type, word, canonical_tag) VALUES ('"
                          + store_->sanitize(rule_type) + "', '"
                          + store_->sanitize(word) + "', '"
                          + store_->sanitize(canonical_tag) + "');";
        return store_->execute(sql);
    }

    bool ingestConceptNetEdge(const std::string& start, const std::string& relation, const std::string& end, float weight) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO conceptnet_edges (start_node, relation, end_node, weight) VALUES ('"
                          + store_->sanitize(start) + "', '"
                          + store_->sanitize(relation) + "', '"
                          + store_->sanitize(end) + "', "
                          + std::to_string(weight) + ");";
        return store_->execute(sql);
    }
};

class SemanticStoreFactory {
public:
    static std::unique_ptr<ISemanticStore> create() {
        if (SystemConfig::use_swarm_fabric) {
            CognitiveStoreFactory::instance().init(SystemConfig::brain_root);
            return std::make_unique<SwarmSemanticStore>();
        } else {
            return std::make_unique<LegacySemanticStore>();
        }
    }
};

} // namespace swarm
} // namespace yuki
