#pragma once
#include "CognitiveStore.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>

namespace yuki {

struct VectorClock {
    std::map<std::string, uint64_t> clocks;

    void increment(const std::string& nodeId);
    bool happensBefore(const VectorClock& other) const;
    bool isConcurrent(const VectorClock& other) const;
    VectorClock merge(const VectorClock& other) const;

    std::string serialize() const;
    static VectorClock deserialize(const std::string& s);
};

class IMergeStrategy {
public:
    virtual ~IMergeStrategy() = default;
    virtual nlohmann::json resolve(
        const std::string& table,
        const nlohmann::json& localRecord,
        const VectorClock& localVc,
        const nlohmann::json& remoteRecord,
        const VectorClock& remoteVc
    ) = 0;
};

class LastWriterWinsStrategy : public IMergeStrategy {
public:
    nlohmann::json resolve(
        const std::string& table,
        const nlohmann::json& localRecord,
        const VectorClock& localVc,
        const nlohmann::json& remoteRecord,
        const VectorClock& remoteVc
    ) override;
};

class ORSetStrategy : public IMergeStrategy {
public:
    nlohmann::json resolve(
        const std::string& table,
        const nlohmann::json& localRecord,
        const VectorClock& localVc,
        const nlohmann::json& remoteRecord,
        const VectorClock& remoteVc
    ) override;
};

class ContentAddressedStrategy : public IMergeStrategy {
public:
    nlohmann::json resolve(
        const std::string& table,
        const nlohmann::json& localRecord,
        const VectorClock& localVc,
        const nlohmann::json& remoteRecord,
        const VectorClock& remoteVc
    ) override;
};

class MergeEngine {
public:
    static MergeEngine& instance();
    void setStores(StorePtr sharedSemantic, StorePtr sharedProcedural);

    MergeEngine(StorePtr sharedSemantic = nullptr, StorePtr sharedProcedural = nullptr);
    ~MergeEngine();

    bool stageContribution(
        const std::string& agentId,
        const std::string& targetTable,
        const std::string& targetKey,
        const std::string& operation,
        const nlohmann::json& payload,
        const VectorClock& vc
    );

    bool integrateAgent(const std::string& agentId);
    size_t processMergeQueue();
    size_t pendingCount() const;

    void startBackgroundWorker(int intervalMs = 500);
    void stopBackgroundWorker();

private:
    StorePtr semantic_;
    StorePtr procedural_;
    std::unordered_map<std::string, std::unique_ptr<IMergeStrategy>> strategies_;

    std::atomic<bool> running_{false};
    std::thread workerThread_;
    std::mutex mtx_;
};

} // namespace yuki
