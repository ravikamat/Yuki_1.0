#pragma once
// TaskContext.h — Task genome building + cognitive situation (merged from TaskGenomeBuilder + SituationBuilder)
#include "BrainTypes.h"
#include "../memory/ContextMemory.h"

// ── §4 Task Genome ────────────────────────────────────────────────────────────

class TaskGenomeBuilder {
public:
    TaskGenome build(const CognitiveSituation& situation,
                     int conversationTurnIndex) const;
private:
    float scoreComplexity(const PatternFrame& frame) const;
    float scoreNovelty(const PatternFrame& frame, int turnIndex) const;
    float scoreRisk(const PatternFrame& frame) const;
    SearchMode chooseSearchMode(const PatternFrame& frame) const;
    std::vector<std::string> chooseFamilies(const PatternFrame& frame, float complexity) const;
};

// ── §3.3 Cognitive Situation ──────────────────────────────────────────────────

class SituationBuilder {
public:
    SituationBuilder();
    CognitiveSituation build(const PatternFrame&       frame,
                             const ConversationMemory& memory,
                             int                       turnIndex) const;
private:
    UserStateEstimate   estimateUserState(const PatternFrame& frame,
                                         const ConversationMemory& memory,
                                         int turnIndex) const;
    std::vector<std::string> inferMemoryZones(const PatternFrame& frame) const;
    std::vector<std::string> inferToolZones(const PatternFrame& frame) const;
};
