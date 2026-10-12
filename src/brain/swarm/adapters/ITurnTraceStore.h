#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <ctime>
#include "brain/predictive/turn_trace.h"
#include "brain/swarm/CognitiveStore.h"
#include "brain/core/SystemConfig.h"

namespace yuki {
namespace swarm {

struct Contradiction {
    std::string key;
    std::string description;
    float severity = 0.0f;
    uint64_t timestamp = 0;
};

class ITurnTraceStore {
public:
    virtual ~ITurnTraceStore() = default;
    virtual bool recordTurn(const TurnTrace& trace) = 0;
    virtual std::vector<TurnTrace> queryRecent(size_t n) = 0;
    virtual bool archiveContradiction(const Contradiction& c) = 0;
    virtual bool updatePredictive(const std::string& key, float value) = 0;
    // --- Heuristic value extensions (HeuristicMemory layer) ---
    /// Read a predictive scalar; returns defaultValue if key not found.
    virtual float getPredictive(const std::string& key, float defaultValue) = 0;
    /// Alias for updatePredictive — write a predictive scalar.
    virtual void setPredictive(const std::string& key, float value) = 0;
};

class LegacyTurnTraceStore : public ITurnTraceStore {
private:
    std::vector<TurnTrace> inMemoryTraces_;
public:
    LegacyTurnTraceStore() = default;
    ~LegacyTurnTraceStore() override = default;

    bool recordTurn(const TurnTrace& trace) override {
        inMemoryTraces_.push_back(trace);
        return true;
    }

    std::vector<TurnTrace> queryRecent(size_t n) override {
        if (inMemoryTraces_.size() <= n) return inMemoryTraces_;
        return std::vector<TurnTrace>(inMemoryTraces_.end() - n, inMemoryTraces_.end());
    }

    bool archiveContradiction(const Contradiction& /*c*/) override {
        return true;
    }

    bool updatePredictive(const std::string& /*key*/, float /*value*/) override {
        return true;
    }
    float getPredictive(const std::string& /*key*/, float defaultValue) override {
        return defaultValue;
    }
    void setPredictive(const std::string& key, float value) override {
        updatePredictive(key, value);
    }
};

class SwarmTurnTraceStore : public ITurnTraceStore {
private:
    StorePtr store_;
    std::string agent_id_;
public:
    explicit SwarmTurnTraceStore(const std::string& agent_id) : agent_id_(agent_id) {
        auto& factory = CognitiveStoreFactory::instance();
        store_ = factory.getStore(StoreScope::AGENT_TRACES, agent_id);
    }
    ~SwarmTurnTraceStore() override = default;

    bool recordTurn(const TurnTrace& trace) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT INTO turn_traces (timestamp, raw_input, normalized_input, final_intent, final_entity, final_confidence, action_taken, was_clarification) VALUES ("
                          + std::to_string(std::time(nullptr) * 1000) + ", '"
                          + store_->sanitize(trace.raw_input) + "', '"
                          + store_->sanitize(trace.normalized_input.empty() ? trace.raw_input : trace.normalized_input) + "', '"
                          + store_->sanitize(trace.final_intent) + "', '"
                          + store_->sanitize(trace.final_entity) + "', "
                          + std::to_string(trace.final_confidence) + ", '"
                          + store_->sanitize(trace.action_taken) + "', "
                          + std::to_string(trace.was_clarification ? 1 : 0) + ");";
        return store_->execute(sql);
    }

    std::vector<TurnTrace> queryRecent(size_t n) override {
        std::vector<TurnTrace> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query("SELECT raw_input, normalized_input, final_intent, final_entity, final_confidence, action_taken, was_clarification FROM turn_traces ORDER BY id DESC LIMIT " + std::to_string(n) + ";");
        for (const auto& row : rows) {
            if (row.size() < 7) continue;
            TurnTrace t;
            t.raw_input = row[0];
            t.normalized_input = row[1];
            t.final_intent = row[2];
            t.final_entity = row[3];
            t.final_confidence = std::stof(row[4]);
            t.action_taken = row[5];
            t.was_clarification = (std::stoi(row[6]) != 0);
            results.push_back(t);
        }
        return results;
    }

    bool archiveContradiction(const Contradiction& c) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO contradictions_archive (timestamp, memory_key, memory_value, current_evidence, prediction_error, turns_unresolved) VALUES ("
                          + std::to_string(c.timestamp < 1 ? std::time(nullptr) * 1000 : c.timestamp) + ", '"
                          + store_->sanitize(c.key) + "', "
                          + std::to_string(c.severity) + ", 0.0, 0.0, 1);";
        return store_->execute(sql);
    }

    bool updatePredictive(const std::string& key, float value) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO predictive_memory_entries (key, value, last_updated) VALUES ('"
                          + store_->sanitize(key) + "', "
                          + std::to_string(value) + ", strftime('%s','now'));";
        return store_->execute(sql);
    }
    float getPredictive(const std::string& key, float defaultValue) override {
        if (!store_ || !store_->raw()) return defaultValue;
        auto rows = store_->query(
            "SELECT value FROM predictive_memory_entries WHERE key = '" + store_->sanitize(key) + "' LIMIT 1;");
        if (!rows.empty() && !rows[0].empty()) {
            try { return std::stof(rows[0][0]); } catch (...) {}
        }
        return defaultValue;
    }
    void setPredictive(const std::string& key, float value) override {
        updatePredictive(key, value);
    }
};

class TurnTraceStoreFactory {
public:
    static std::unique_ptr<ITurnTraceStore> create(const std::string& agent_id = "default") {
        if (SystemConfig::use_swarm_fabric) {
            CognitiveStoreFactory::instance().init(SystemConfig::brain_root);
            CognitiveStoreFactory::instance().provisionAgent(agent_id);
            return std::make_unique<SwarmTurnTraceStore>(agent_id);
        } else {
            return std::make_unique<LegacyTurnTraceStore>();
        }
    }
};

} // namespace swarm
} // namespace yuki
