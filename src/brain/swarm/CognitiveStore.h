#pragma once
// CognitiveStore — the swarm's per-agent and shared SQLite stores.
// One ICognitiveStore implementation per StoreScope (shared semantic /
// procedural / user cortex, plus per-agent episodic, identity and turn-trace
// databases). Each store exposes raw SQLite access, transactions, a vector
// clock for cross-agent ordering, and a SQL sanitizer; SQLiteStoreFactory
// resolves a scope + agent id into the concrete store.
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <sqlite3.h>

namespace yuki {

enum class StoreScope {
    SHARED_SEMANTIC,     // shared/semantic.db (T2)
    SHARED_PROCEDURAL,   // shared/procedural.db (T3)
    SHARED_USER,         // shared/user_cortex.db (Global User)
    AGENT_EPISODIC,      // agents/{id}/episodic.db (T1)
    AGENT_IDENTITY,      // agents/{id}/identity.db (Self-Model)
    AGENT_TRACES,        // agents/{id}/turn_traces.db (Predictive)
    SYSTEM_ORCHESTRATOR  // system/orchestrator.db (Registry)
};

class ICognitiveStore {
public:
    virtual ~ICognitiveStore() = default;

    virtual sqlite3* raw() = 0;
    virtual std::string path() const = 0;
    virtual StoreScope scope() const = 0;
    virtual std::string agentId() const = 0;

    virtual bool execute(const std::string& sql) = 0;
    virtual std::vector<std::vector<std::string>> query(const std::string& sql) = 0;

    virtual bool beginTransaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    virtual std::string getVectorClock() = 0;
    virtual void incrementVectorClock(const std::string& nodeId) = 0;
    virtual bool isHealthy() const = 0;
    virtual std::string sanitize(const std::string& input) const = 0;
};

using StorePtr = std::shared_ptr<ICognitiveStore>;

class CognitiveStore : public ICognitiveStore, public std::enable_shared_from_this<CognitiveStore> {
public:
    CognitiveStore(StoreScope scope, const std::string& dbPath, const std::string& agentId = "");
    ~CognitiveStore() override;

    bool init();

    sqlite3* raw() override { return db_; }
    std::string path() const override { return dbPath_; }
    StoreScope scope() const override { return scope_; }
    std::string agentId() const override { return agentId_; }

    bool execute(const std::string& sql) override;
    std::vector<std::vector<std::string>> query(const std::string& sql) override;

    bool beginTransaction() override;
    bool commit() override;
    bool rollback() override;

    std::string getVectorClock() override;
    void incrementVectorClock(const std::string& nodeId) override;
    bool isHealthy() const override;
    std::string sanitize(const std::string& input) const override;

    static std::vector<uint8_t> hexToBlob(const std::string& hex);
    static std::string blobToHex(const std::vector<uint8_t>& blob);

private:
    bool applyPragmas();
    bool ensureSchema();

    StoreScope scope_;
    std::string dbPath_;
    std::string agentId_;
    sqlite3* db_ = nullptr;
    mutable std::mutex mtx_;
    uint64_t localClock_ = 0;
};

struct StoreHandleEntry {
    std::weak_ptr<ICognitiveStore> weakStore;
    StorePtr strongStore;
    std::chrono::steady_clock::time_point lastAccessed;
    bool inUse = false;
};

class CognitiveStoreFactory {
public:
    static CognitiveStoreFactory& instance();

    void init(const std::string& brainRoot = "YUKI_BRAIN");
    void reset();

    StorePtr getStore(StoreScope scope, const std::string& agentId = "");
    void releaseStore(StoreScope scope, const std::string& agentId = "");
    bool provisionAgent(const std::string& agentId, const std::string& parentAgentId = "");
    bool hibernateAgent(const std::string& agentId);
    void closeAgentStores(const std::string& agentId);

    void setMaxHandles(size_t maxHandles) { maxHandles_ = maxHandles; }
    size_t handleCount() const;
    std::string brainRoot() const { return brainRoot_; }

private:
    CognitiveStoreFactory() = default;
    ~CognitiveStoreFactory() = default;

    void evictLruIfNecessary();

    std::string brainRoot_ = "YUKI_BRAIN";
    size_t maxHandles_ = 128;
    mutable std::recursive_mutex mtx_;
    std::unordered_map<std::string, StoreHandleEntry> activeStores_;
};

} // namespace yuki
