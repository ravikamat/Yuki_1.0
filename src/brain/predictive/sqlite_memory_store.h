#pragma once

#include "memory_store.h"
#include "predictive_turn_engine.h"
#include "turn_trace.h"
#include "../database/DatabaseManager.h"
#include "../memory/UserMemory.h"
#include <memory>
#include <vector>
#include <string>

namespace yuki {

class SqliteMemoryStore : public MemoryStore {
public:
    SqliteMemoryStore(DatabaseManager& dbManager, std::shared_ptr<UserMemory> userMemory);
    virtual ~SqliteMemoryStore() = default;

    void store_trace(const PredictionState& state,
                     const BeliefPool& pool,
                     const ResolutionDecision& decision,
                     const TurnResult& result) override;

    std::vector<ContradictionEvent> check_contradictions(
        const PredictionState& state,
        const BeliefPool& pool) override;

    void update(const std::string& key, float value) override;
    void archive_contradiction(const ContradictionEvent& c) override;
    void distill(const std::vector<TurnTrace>& recent_traces) override;

private:
    void init_tables();

    DatabaseManager& dbManager_;
    std::shared_ptr<UserMemory> userMemory_;
};

} // namespace yuki
