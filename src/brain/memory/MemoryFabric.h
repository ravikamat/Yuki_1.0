#ifndef YUKI_MEMORY_FABRIC_H
#define YUKI_MEMORY_FABRIC_H

#include "brain/memory/KnowledgeTag.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/ActionPlan.h"
#include "src/brain/learning/LearningEpisode.h"
#include <cstdint>

#include <vector>
#include <string>
#include <memory>

namespace yuki {
namespace memory {

enum class MemoryTier : uint8_t {
    T0_WORKING = 0,
    T1_EPISODIC,
    T2_SEMANTIC_HDC,
    T3_PROCEDURAL,
    T4_ARCHIVE_MERKLE
};

enum class RetrieveMode : uint8_t {
    EXACT = 0,
    SEMANTIC,
    CHAIN,
    FUZZY,
    TEMPORAL
};

struct MemoryItem {
    uint64_t             itemId = 0;
    MemoryTier           tier = MemoryTier::T0_WORKING;
    std::string          key;
    std::vector<uint8_t> payload;
    float                confidence = 0.0f;
    uint64_t             timestamp = 0;
    std::vector<KnowledgeTag> tags;
};

class MemoryFabric {
public:
    void store(const MemoryItem& item);
    std::vector<MemoryItem> retrieve(const std::string& query, RetrieveMode mode = RetrieveMode::FUZZY);
    
    void consolidateT0toT1();
    void consolidateT1toT2();
    void archiveToT4();

    size_t getItemCount(MemoryTier tier) const;
    void clear();
    void warmConnection();

    // M4: Action plan storage
    void storeActionPlan(const action::ActionPlan& plan, MemoryTier tier);
    void storeExecutionReport(const action::ExecutionReport& report, MemoryTier tier);
    std::vector<action::ActionPlan> retrieveActionPlans(const std::string& query, RetrieveMode mode, float minConfidence);

    // Autonomy & Organism store helpers
    void storeBelief(const std::string& beliefId, const std::vector<uint8_t>& payload);
    void storeExperiment(const std::string& expId, const std::vector<uint8_t>& payload);

    void storeLearningEpisode(const yuki::brain::learning::LearningEpisode& episode);
    std::vector<yuki::brain::learning::LearningEpisode> loadLearningEpisodes() const;



private:
    std::vector<MemoryItem> t0Working_;
    std::vector<MemoryItem> t1Episodic_;
    std::vector<MemoryItem> t2Semantic_;
    std::vector<MemoryItem> t3Procedural_;
    std::vector<MemoryItem> t4Archive_;
};

} // namespace memory
} // namespace yuki

#endif
