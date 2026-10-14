#pragma once
#include "CompetenceRecord.h"
#include "Hypothesis.h"
#include <array>
#include <vector>
#include <string>
#include <mutex>
#include <queue>

#include "brain/inference/PrecisionPredictor.h"
namespace yuki::organism {
    class DriveSystem;
}
namespace yuki::system {
    class BackgroundJobEngine;
}
namespace yuki::research {
    class ResearchPlanner;
}
namespace yuki::synthesis {
    class CodeSynthesisAgent;
}
namespace yuki::brain::memory {
    class EpisodicStore;
}

namespace yuki::metacognition {

class ImprovementGraph;

struct CurriculumTopic {
    std::string topic;
    float priority = 0.0f;
    float competence = 0.0f;
    uint64_t timestamp = 0;

    bool operator<(const CurriculumTopic& other) const {
        return priority < other.priority; // max-heap by priority
    }
};

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
    void observePrecisionPredictor(const yuki::brain::inference::PrecisionPredictor* predictor);

    // M9 DriveSystem Advisory Setter
    void setDriveSystem(yuki::organism::DriveSystem* ptr);
    void setBackgroundJobEngine(yuki::system::BackgroundJobEngine* ptr);
    void setImprovementGraph(ImprovementGraph* graph);
    ImprovementGraph* getImprovementGraph() const;

    void setResearchPlanner(yuki::research::ResearchPlanner* planner);
    yuki::research::ResearchPlanner* getResearchPlanner() const;

    void setCodeSynthesisAgent(yuki::synthesis::CodeSynthesisAgent* agent);
    yuki::synthesis::CodeSynthesisAgent* getCodeSynthesisAgent() const;

    // ── Task 4.2: Research & Code Synthesis Triggers ─────────────────────────
    bool triggerResearch(const std::string& gap_description);
    void buildLearningCurriculum(const std::vector<std::string>& gaps);
    bool requestCodeSynthesis(const std::string& gap);
    std::vector<CurriculumTopic> getCurriculumTopics() const;

    // ── Task 6.1: Metacognitive Inner Monologue Observation ───────────────────
    void processSelfReflections(yuki::brain::memory::EpisodicStore* store);
    void processSelfReflectionText(const std::string& reflection_text);

    // Observe Free Energy & detect ignorance gap stagnation
    void observeFreeEnergy(float free_energy,
                           const yuki::brain::inference::PrecisionPredictor* predictor = nullptr,
                           ImprovementGraph* graph = nullptr);

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
    ImprovementGraph* improvement_graph_{nullptr};
    yuki::research::ResearchPlanner* research_planner_{nullptr};
    yuki::synthesis::CodeSynthesisAgent* code_synthesis_agent_{nullptr};
    std::priority_queue<CurriculumTopic> learning_curriculum_;

    // Internal logic
    bool evaluateSuccess(const TurnOutcome& outcome) const;
    void generateHypothesis(CompetenceDomain domain, const TurnOutcome& outcome);
    float computePriority(CompetenceDomain domain, const TurnOutcome& outcome) const;
    float computeConfidence(CompetenceDomain domain) const;
    bool detectStagnation(const std::string& serializedWeights) const;
};

} // namespace yuki::metacognition
