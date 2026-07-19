#pragma once

#include <array>
#include <string>
#include <vector>
#include "brain/memory/CognitiveMemoryFabric.h"

namespace yuki {
struct TurnResult;
namespace self {

enum class CompetenceDomain {
    CPP_PROGRAMMING = 0,
    CMAKE_BUILD_SYSTEM,
    ACTIVE_INFERENCE,
    MEMORY_SYSTEMS,
    LLM_INTEGRATION,
    SYSTEM_ARCHITECTURE,
    DOMAIN_COUNT
};

enum class CuriosityTopic {
    PREDICTIVE_CODING = 0,
    FREE_ENERGY_PRINCIPLE,
    HDC_COMPUTING,
    SPARSE_DISTRIBUTED_MEMORY,
    DYNAMICS_MODELS,
    PROACTIVE_BEHAVIOR,
    TOPIC_COUNT
};

struct CompetenceState {
    float level = 0.1f;        // 0.0=novice, 1.0=expert
    float confidence = 0.1f;   // reliability of self-assessment
    uint32_t successes = 0;
    uint32_t failures = 0;
    double last_exercised = 0.0; // epoch seconds
};

struct CuriosityState {
    float intensity = 0.5f;     // 0.0=indifferent, 1.0=obsessed
    float epistemic_value = 0.5f; // expected information gain
    double last_satisfied = 0.0;
    uint32_t times_pursued = 0;
};

struct RelationshipState {
    float depth = 0.0f;        // 0.0=stranger, 1.0=trusted partner
    float alignment = 0.5f;    // judgment agreement rate
    uint32_t turns_together = 0;
    uint32_t corrections_given = 0;
    uint32_t corrections_accepted = 0;
};

struct CorrectionRecord {
    CompetenceDomain domain;
    std::string what_i_said;
    std::string what_was_right;
    double timestamp = 0.0;
};

struct SelfSnapshot {
    double timestamp = 0.0;
    std::array<CompetenceState, static_cast<size_t>(CompetenceDomain::DOMAIN_COUNT)> competence;
    std::array<CuriosityState, static_cast<size_t>(CuriosityTopic::TOPIC_COUNT)> curiosity;
    RelationshipState relationship;
    float overall_uncertainty = 1.0f;
    float growth_rate = 0.0f;
    std::vector<CorrectionRecord> correction_history;
};

class SelfModel {
public:
    SelfModel();

    // Called at end of every turn
    void updateFromTurn(const yuki::TurnResult& result, 
                        const std::string& user_input,
                        float vse_entropy);

    // Called during SleepThread dreamEpoch
    void consolidate();

    // Called when user gives explicit feedback
    void recordCorrection(CompetenceDomain domain, 
                          const std::string& what_i_said,
                          const std::string& what_was_right);

    // Query interface
    CompetenceState getCompetence(CompetenceDomain d) const;
    CuriosityState getCuriosity(CuriosityTopic t) const;
    RelationshipState getRelationship() const;

    // Generate learning goals from curiosity gaps
    std::vector<std::string> generateLearningGoals() const;

    // Persistence to CMF T1 Episodic as "self" category
    void saveToCMF(yuki::memory::CognitiveMemoryFabric* cmf);
    void loadFromCMF(yuki::memory::CognitiveMemoryFabric* cmf);

    // Diagnostic log
    std::string toString() const;

private:
    SelfSnapshot current_;
    std::vector<SelfSnapshot> history_;

    void updateCompetence(const yuki::TurnResult& result, const std::string& input);
    void updateCuriosity(float vse_entropy);
    void updateRelationship(const yuki::TurnResult& result, const std::string& input);
    float estimateEpistemicValue(CuriosityTopic t) const;
    CompetenceDomain inferDomain(const std::string& input) const;
};

} // namespace self
} // namespace yuki
