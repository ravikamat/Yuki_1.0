#include "brain/reasoning/GoalModel.h"
#pragma once
#include <string>
#include <vector>
#include <map>

enum class ApprovalType {
    NONE,
    INSTALL_TOOL,
    EXECUTE_STATE_CHANGE,
    ACCESS_PRIVATE_DATA,
    SELF_MODIFY_CODE,
    DESTRUCTIVE_ACTION
};

struct ApprovalRequest {
    bool required = false;
    std::vector<ApprovalType> types;
    std::string summary;
    std::vector<std::string> riskySteps;
};


struct CapabilityRecord {
    std::string capabilityId;
    std::string status; // UNKNOWN, LEARNING, KNOWN
    std::string method;
    std::vector<std::string> toolsRequired;
    std::vector<std::string> learnedSteps;
    float successRate = 0.0f;
    int64_t lastUsed = 0;
};

enum class ActionBackend {
    API,
    CLI_SCRIPT,
    GUI_AUTOMATION
};

struct ActionStep {
    std::string id;
    std::string description;
    ActionBackend backend;
    std::string commandOrApi;
    std::map<std::string, std::string> args;
};

struct ExecutionPlan {
    std::string planId;
    std::vector<std::string> blockingQuestions;
    std::vector<std::string> toolChecks;
    std::vector<ActionStep> stepsExpanded;
    std::vector<std::string> verificationSteps;
    bool requiresApproval = true;
    std::vector<ApprovalType> approvalTypes;
};

struct StepResult {
    std::string stepId;
    bool success = false;
    int exitCode = 0;
    std::string summary;
    std::vector<std::string> evidence;
};

struct UndoRecord {
    std::string operationId;
    std::string action;
    std::string sourcePath;
    std::string targetPath;
    int64_t timestamp = 0;
    bool reversible = false;
};

struct DependencyCheckResult {
    std::string toolName;
    bool alreadyInstalled = false;
    bool approvalRequested = false;
    std::string approvalSummary;
};

struct ExecutionResult {
    bool success = false;
    std::vector<StepResult> steps;
    std::string summary;
};

struct VerificationBundle {
    bool success = false;
    bool pendingApproval = false;
    ApprovalRequest approval;
    std::vector<std::string> evidence;
};

enum class ResponseAct {
    ACKNOWLEDGE,
    KNOWLEDGE_ANSWER,
    TASK_RESULT,
    TASK_FAILED,
    APPROVAL_REQUEST,
    CLARIFY
};

struct UtterancePlan {
    ResponseAct act = ResponseAct::ACKNOWLEDGE;
    std::string userFacingSummary;
    std::vector<std::string> riskySteps;
    std::vector<std::string> evidenceLines;
    bool requiresUserReply = false;
};
