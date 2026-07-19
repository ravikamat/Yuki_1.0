// =============================================================================
// yuki/core/memory_store.h
// Abstract interface + InMemoryStore for MemoryStore.
// =============================================================================
#pragma once
#include "predictive_turn_engine.h"
#include "turn_trace.h"
#include <unordered_map>
#include <string>

namespace yuki {

// NullMemoryStore — all operations are no-ops; used by unit tests
class NullMemoryStore final : public MemoryStore {
public:
    void store_trace(const PredictionState&, const BeliefPool&,
                     const ResolutionDecision&, const TurnResult&) override {}
    std::vector<ContradictionEvent> check_contradictions(
        const PredictionState&, const BeliefPool&) override { return {}; }
    void update(const std::string&, float) override {}
    void archive_contradiction(const ContradictionEvent&) override {}
    void distill(const std::vector<TurnTrace>&) override {}
};

// InMemoryStore — lightweight map-based store for integration tests
class InMemoryStore final : public MemoryStore {
public:
    void store_trace(const PredictionState& state, const BeliefPool&,
                     const ResolutionDecision&, const TurnResult&) override {
        turn_count_++;
        (void)state;
    }

    std::vector<ContradictionEvent> check_contradictions(
        const PredictionState&, const BeliefPool&) override {
        return {};
    }

    void update(const std::string& key, float value) override {
        entries_[key] = value;
    }

    void archive_contradiction(const ContradictionEvent& c) override {
        archived_contradictions_.push_back(c);
    }

    void distill(const std::vector<TurnTrace>& traces) override {
        distill_count_ += static_cast<int>(traces.size());
    }

    int  turn_count()    const { return turn_count_; }
    int  distill_count() const { return distill_count_; }
    float get(const std::string& k) const {
        auto it = entries_.find(k);
        return it != entries_.end() ? it->second : 0.0f;
    }

private:
    std::unordered_map<std::string, float> entries_;
    std::vector<ContradictionEvent>        archived_contradictions_;
    int turn_count_    = 0;
    int distill_count_ = 0;
};

} // namespace yuki
