#pragma once
// GoalModel.h — GoalModel (what user wants) + CogTaskState (live execution state)
// Phase 1 + Phase H — replaces the old thin GoalModel
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "brain/reasoning/SemanticParser.h"
#include "LanguageLayer.h"
#include "MeaningTypes.h"

// ── §GoalModel — static description of what the user wants ────────────────────
struct GoalModel {
    std::string  goal;            // human-readable goal description
    std::string  domain;          // tech / communication / food / health / creative / system
    std::string  tone;            // formal / casual / urgent / emotional
    std::string  language;        // "en" / "hi-en" / "hi"
    std::string  responseStyle;   // "english" / "hinglish" / "hindi"

    std::map<std::string, std::string> knownSlots;   // filled slots
    std::vector<std::string>           unknownSlots;  // gaps that need clarification
    std::vector<std::string>           gaps;          // capability gaps

    bool needsClarification = false;
    bool needsResearch      = false;
    bool needsExecution     = false;
    bool isEmotional        = false;
};

// ── §CogTaskState — live execution state (changes every step) ───────────────────
struct CogTaskState {
    std::string taskId;
    std::string goal;

    enum class Status {
        PROPOSED,           // plan shown, not approved yet
        RUNNING,            // executing steps
        BLOCKED,            // waiting for user input / dependency
        AWAITING_APPROVAL,  // permission required before next step
        PAUSED,             // user paused it
        FAILED,             // unrecoverable failure
        DONE                // confirmed complete
    } status = Status::PROPOSED;

    int currentStep = 0;
    int totalSteps  = 0;
    int retries     = 0;
    int maxRetries  = 3;

    std::vector<std::string> assumptions;     // what we assumed without asking
    std::vector<std::string> blockers;        // what's stopping us right now
    std::vector<std::string> completedSteps;  // steps already confirmed done

    std::string rollbackCheckpoint;   // last safe restore point description
    std::string safetyLevel;          // OBSERVE / READ / EDIT / INSTALL / SEND / DELETE / SELF_MODIFY
    bool        userApproved = false;

    uint64_t    createdAtMs  = 0;
    uint64_t    updatedAtMs  = 0;

    // Helpers
    static std::string statusLabel(Status s);
    bool isTerminal() const { return status == Status::FAILED || status == Status::DONE; }
    bool needsInput() const { return status == Status::BLOCKED || status == Status::AWAITING_APPROVAL; }
};

// ── §GoalModelBuilder — constructs GoalModel from SemanticFrame ───────────────
class GoalModelBuilder {
public:
    GoalModel buildSpec(const SemanticFrame&   frame,
                       const LanguageResult&  lang) const;
    GoalModel build(const Goal& goal, const MeaningState& meaning) const;

    CogTaskState createTaskState(const GoalModel& spec,
                              const std::string& taskId) const;
private:
    std::string deriveTone(const SemanticFrame& frame) const;
    std::string deriveGoalDescription(const SemanticFrame& frame) const;
    std::string deriveSafetyLevel(const SemanticFrame& frame) const;
};
