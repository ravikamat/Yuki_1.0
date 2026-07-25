#pragma once
#include "CompetenceRecord.h"
#include "Hypothesis.h"
#include <array>
#include <vector>
#include <string>
#include <mutex>

namespace yuki::inference {
    class PrecisionPredictor; // forward decl
}
namespace yuki::organism {
    class DriveSystem;
}
namespace yuki::system {
    class BackgroundJobEngine;
}

namespace yuki::metacognition {

struct TurnOutcome {
    uint8_t predicted_intent = 0;           // Index into IntentClass enum
    std::string actual_response_family;      // template_family from TurnResult
    float precision_used = 0.5f;
    float belief_entropy = 0.0f;
    bool clarification_triggered = false;
    bool user_corrected = false;             // true if next turn indicates correction
};

class MetacognitionEngine {
public:
    MetacognitionEngine();
    ~MetacognitionEngine();

    // Main observation entry point — called from TurnCoordinator::end_turn()
    void observeTurnOutcome(const TurnOutcome& outcome);

    // Read PrecisionPredictor weights to detect stagnation
    void observePrecisionPredictor(const yuki::inference::PrecisionPredictor* predictor);

    // M9 DriveSystem Advisory Setter
    void setDriveSystem(yuki::organism::DriveSystem* ptr);
    void setBackgroundJobEngine(yuki::system::BackgroundJobEngine* ptr);

    // Retrieve current competence state
    const CompetenceRecord& getCompetence(CompetenceDomain domain) const;

    // Retrieve active hypotheses (non-destructive read)
    std::vector<Hypothesis> getActiveHypotheses() const;

    // Clear hypotheses (call after they are consumed by planner)
    void clearHypotheses();

    // Serialize competence state to JSON (for persistence)
    std::string serializeCompetence() const;

    // Deserialize competence state
    void deserializeCompetence(const std::string& json);

private:
    std::array<CompetenceRecord, static_cast<size_t>(CompetenceDomain::COUNT)> competence_;
    std::vector<Hypothesis> activeHypotheses_;
    mutable std::mutex mutex_;
    std::unique_ptr<yuki::organism::DriveSystem> drive_system_{nullptr};
    std::unique_ptr<yuki::system::BackgroundJobEngine> job_engine_{nullptr};

    // Internal logic
    bool evaluateSuccess(const TurnOutcome& outcome) const;
    void generateHypothesis(CompetenceDomain domain, const TurnOutcome& outcome);
    float computePriority(CompetenceDomain domain, const TurnOutcome& outcome) const;
    float computeConfidence(CompetenceDomain domain) const;
    bool detectStagnation(const std::string& serializedWeights) const;
};

} // namespace yuki::metacognition
