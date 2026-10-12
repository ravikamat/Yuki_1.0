#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <ctime>
#include "brain/self/SelfModel.h"
#include "brain/swarm/CognitiveStore.h"
#include "brain/core/SystemConfig.h"

namespace yuki {
namespace swarm {

struct SwarmIdentityForkResult {
    uint64_t new_snapshot_id = 0;
    uint64_t parent_snapshot_id = 0;
    std::string parent_agent_id;
    std::string mission;
};

class IIdentityStore {
public:
    virtual ~IIdentityStore() = default;
    virtual bool saveSnapshot(const self::IdentitySnapshot& snap) = 0;
    virtual std::optional<self::IdentitySnapshot> loadLatest() = 0;
    virtual std::optional<self::IdentitySnapshot> loadById(uint64_t id) = 0;
    virtual bool verifyChain() = 0;
    virtual std::optional<SwarmIdentityForkResult> forkFrom(
        const std::string& parent_agent_id, const std::string& new_mission) = 0;
    // --- Heuristic value extensions (HeuristicMemory layer) ---
    /// Load a named scalar float from the identity parameter store.
    virtual float getParameter(const std::string& key, float defaultValue) = 0;
    /// Persist a named scalar float to the identity parameter store.
    virtual void setParameter(const std::string& key, float value) = 0;
};

class LegacyIdentityStore : public IIdentityStore {
private:
    std::unique_ptr<self::IdentityPersistence> legacy_;
public:
    explicit LegacyIdentityStore(const std::string& dbPath = "data/identity.db") {
        legacy_ = std::make_unique<self::IdentityPersistence>(dbPath);
        legacy_->initializeSchema();
    }
    ~LegacyIdentityStore() override = default;

    bool saveSnapshot(const self::IdentitySnapshot& snap) override {
        return legacy_->saveSnapshot(snap);
    }

    std::optional<self::IdentitySnapshot> loadLatest() override {
        auto snaps = legacy_->getSnapshotHistory(1);
        if (snaps.empty()) return std::nullopt;
        return snaps[0];
    }

    std::optional<self::IdentitySnapshot> loadById(uint64_t id) override {
        auto snaps = legacy_->getSnapshotHistory(100);
        for (const auto& s : snaps) {
            if (s.id == id) return s;
        }
        return std::nullopt;
    }

    bool verifyChain() override {
        return legacy_->verifyHashChain();
    }

    std::optional<SwarmIdentityForkResult> forkFrom(
        const std::string& parent_agent_id, const std::string& new_mission) override {
        SwarmIdentityForkResult res;
        res.new_snapshot_id = std::time(nullptr);
        res.parent_snapshot_id = 1;
        res.parent_agent_id = parent_agent_id;
        res.mission = new_mission;
        return res;
    }
    float getParameter(const std::string& /*key*/, float defaultValue) override {
        return defaultValue;
    }
    void setParameter(const std::string& /*key*/, float /*value*/) override {
        // Legacy: no-op (no identity_parameters table available)
    }
};

class SwarmIdentityStore : public IIdentityStore {
private:
    StorePtr store_;
    std::string agent_id_;
public:
    explicit SwarmIdentityStore(const std::string& agent_id) : agent_id_(agent_id) {
        auto& factory = CognitiveStoreFactory::instance();
        store_ = factory.getStore(StoreScope::AGENT_IDENTITY, agent_id);
    }
    ~SwarmIdentityStore() override = default;

    bool saveSnapshot(const self::IdentitySnapshot& snap) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO identity_snapshots (id, timestamp, version, self_model_blob, theory_of_mind_blob, valence_arousal_blob, confidence_calibrator_blob, previous_hash, current_hash, identity_drift) VALUES ("
                          + std::to_string(snap.id < 1 ? std::time(nullptr) : snap.id) + ", "
                          + std::to_string(snap.timestamp < 1 ? std::time(nullptr) * 1000 : snap.timestamp) + ", '"
                          + store_->sanitize(snap.version.empty() ? "1.0.0" : snap.version) + "', "
                          + "'', '', '', '', "
                          + std::to_string(snap.previousHash) + ", "
                          + std::to_string(snap.currentHash) + ", "
                          + std::to_string(snap.identityDrift) + ");";
        return store_->execute(sql);
    }

    std::optional<self::IdentitySnapshot> loadLatest() override {
        if (!store_ || !store_->raw()) return std::nullopt;
        auto rows = store_->query("SELECT id, timestamp, version, previous_hash, current_hash, identity_drift FROM identity_snapshots ORDER BY timestamp DESC LIMIT 1;");
        if (rows.empty() || rows[0].size() < 6) return std::nullopt;

        self::IdentitySnapshot snap;
        snap.id = std::stoull(rows[0][0]);
        snap.timestamp = std::stoull(rows[0][1]);
        snap.version = rows[0][2];
        snap.previousHash = std::stoull(rows[0][3]);
        snap.currentHash = std::stoull(rows[0][4]);
        snap.identityDrift = std::stod(rows[0][5]);
        return snap;
    }

    std::optional<self::IdentitySnapshot> loadById(uint64_t id) override {
        if (!store_ || !store_->raw()) return std::nullopt;
        auto rows = store_->query("SELECT id, timestamp, version, previous_hash, current_hash, identity_drift FROM identity_snapshots WHERE id = " + std::to_string(id) + ";");
        if (rows.empty() || rows[0].size() < 6) return std::nullopt;

        self::IdentitySnapshot snap;
        snap.id = std::stoull(rows[0][0]);
        snap.timestamp = std::stoull(rows[0][1]);
        snap.version = rows[0][2];
        snap.previousHash = std::stoull(rows[0][3]);
        snap.currentHash = std::stoull(rows[0][4]);
        snap.identityDrift = std::stod(rows[0][5]);
        return snap;
    }

    bool verifyChain() override {
        if (!store_ || !store_->raw()) return true;
        auto rows = store_->query("SELECT id, previous_hash, current_hash FROM identity_snapshots ORDER BY id ASC;");
        uint64_t prev = 0;
        for (const auto& row : rows) {
            if (row.size() < 3) continue;
            uint64_t pHash = std::stoull(row[1]);
            if (pHash != prev && prev != 0) return false;
            prev = std::stoull(row[2]);
        }
        return true;
    }

    std::optional<SwarmIdentityForkResult> forkFrom(
        const std::string& parent_agent_id, const std::string& new_mission) override {
        auto& factory = CognitiveStoreFactory::instance();
        auto parentStore = factory.getStore(StoreScope::AGENT_IDENTITY, parent_agent_id);
        if (!parentStore || !parentStore->raw()) return std::nullopt;

        auto parentRows = parentStore->query("SELECT id, current_hash FROM identity_snapshots ORDER BY timestamp DESC LIMIT 1;");
        uint64_t parentSnapId = parentRows.empty() ? 1 : std::stoull(parentRows[0][0]);
        uint64_t parentHash = parentRows.empty() ? 10001 : std::stoull(parentRows[0][1]);

        self::IdentitySnapshot childSnap;
        childSnap.id = std::time(nullptr);
        childSnap.timestamp = std::time(nullptr) * 1000;
        childSnap.version = "1.0.0-fork";
        childSnap.previousHash = parentHash;
        childSnap.currentHash = parentHash ^ 0xABCD1234;
        childSnap.identityDrift = 0.0;

        if (!saveSnapshot(childSnap)) return std::nullopt;

        SwarmIdentityForkResult res;
        res.new_snapshot_id = childSnap.id;
        res.parent_snapshot_id = parentSnapId;
        res.parent_agent_id = parent_agent_id;
        res.mission = new_mission;
        return res;
    }
    float getParameter(const std::string& key, float defaultValue) override {
        if (!store_ || !store_->raw()) return defaultValue;
        auto rows = store_->query(
            "SELECT value FROM identity_parameters WHERE key = '" + store_->sanitize(key) + "' LIMIT 1;");
        if (!rows.empty() && !rows[0].empty()) {
            try { return std::stof(rows[0][0]); } catch (...) {}
        }
        return defaultValue;
    }
    void setParameter(const std::string& key, float value) override {
        if (!store_ || !store_->raw()) return;
        std::string sql = "INSERT OR REPLACE INTO identity_parameters (key, value, updated_at) VALUES ('"
                          + store_->sanitize(key) + "', "
                          + std::to_string(value) + ", "
                          + std::to_string(std::time(nullptr)) + ");";
        store_->execute(sql);
    }
};

class IdentityStoreFactory {
public:
    static std::unique_ptr<IIdentityStore> create(const std::string& agent_id = "default") {
        if (SystemConfig::use_swarm_fabric) {
            CognitiveStoreFactory::instance().init(SystemConfig::brain_root);
            CognitiveStoreFactory::instance().provisionAgent(agent_id);
            return std::make_unique<SwarmIdentityStore>(agent_id);
        } else {
            return std::make_unique<LegacyIdentityStore>();
        }
    }
};

} // namespace swarm
} // namespace yuki
