#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace yuki::autonomy {

enum class AutonomyMode {
    REACTIVE = 0,
    OWNER_DIRECTED,
    BACKGROUND,
    MAINTENANCE,
    SLEEP_CONSOLIDATION,
    SELF_IMPROVEMENT
};

enum class BeliefStatus {
    VERIFIED = 0,
    LIKELY,
    HYPOTHESIS,
    IMPOSSIBLE_NOW,
    DISPROVED,
    SUPERSEDED
};

enum class RequirementNodeType {
    GOAL = 0,
    CONSTRAINT,
    RESOURCE,
    EVIDENCE_GAP,
    SAFETY_RULE,
    OWNER_RULE,
    CAPABILITY_NEED,
    DELIVERABLE,
    CHECKPOINT
};

enum class OwnerDecisionMode {
    COMPLY = 0,
    SAFE_ALTERNATIVE,
    DEFER_BUILD_PATH,
    DECLINE
};

enum class ExperimentState {
    PROPOSED = 0,
    RUNNING,
    PASSED,
    FAILED,
    ROLLED_BACK,
    PROMOTED
};

enum class WatchdogAlertLevel {
    NONE = 0,
    INFO,
    WARNING,
    CRITICAL
};

struct AutonomyTask {
    std::string taskId;
    std::string source;
    std::string goalText;
    std::string domain;
    float ownerPriority = 0.0f;
    float urgency = 0.0f;
    float expectedValue = 0.0f;
    float risk = 0.0f;
    float confidence = 0.0f;
    float resourceCost = 0.0f;
    float curiosityScore = 0.0f;
    bool requiresHumanApproval = false;
    bool canRunInBackground = false;
    std::vector<std::string> tags;
};

struct RequirementNode {
    std::string nodeId;
    RequirementNodeType type = RequirementNodeType::GOAL;
    std::string label;
    std::string payload;
    float weight = 0.0f;
    bool satisfied = false;
    std::vector<std::string> dependsOn;
};

struct BeliefRecord {
    std::string beliefId;
    std::string subject;
    std::string relation;
    std::string object;
    BeliefStatus status = BeliefStatus::HYPOTHESIS;
    float confidence = 0.0f;
    float novelty = 0.0f;
    float utility = 0.0f;
    std::uint64_t createdAt = 0;
    std::uint64_t recheckAt = 0;
    std::vector<std::string> evidenceIds;
    std::vector<std::string> contradictionIds;
    std::vector<std::string> tags;
};

struct HypothesisRecord {
    std::string hypothesisId;
    std::string summary;
    std::string subsystem;
    float severity = 0.0f;
    float recurrence = 0.0f;
    float fixability = 0.0f;
    float estimatedCost = 1.0f;
    float expectedGain = 0.0f;
    std::vector<std::string> evidenceIds;
};

struct FuturePossibilityRecord {
    std::string recordId;
    std::string requestText;
    std::vector<std::string> blockers;
    std::vector<std::string> prerequisites;
    std::uint64_t revisitAt = 0;
    float plausibility = 0.0f;
};

struct OwnerIntentDecision {
    OwnerDecisionMode mode = OwnerDecisionMode::COMPLY;
    std::string rationale;
    std::string alternativePath;
    bool requiresApproval = false;
};

struct ExperimentRecord {
    std::string experimentId;
    std::string hypothesisId;
    std::vector<std::string> modifiedFiles;
    std::string rollbackCheckpoint;
    float expectedGain = 0.0f;
    float observedGain = 0.0f;
    ExperimentState state = ExperimentState::PROPOSED;
};

struct EvolutionEvent {
    std::string eventId;
    std::string category;
    std::string summary;
    std::uint64_t timestamp = 0;
    std::unordered_map<std::string, std::string> metadata;
};

struct WatchdogAlert {
    WatchdogAlertLevel level = WatchdogAlertLevel::NONE;
    std::string subsystem;
    std::string message;
};

} // namespace yuki::autonomy
