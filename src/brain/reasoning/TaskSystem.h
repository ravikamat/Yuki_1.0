#pragma once
// TaskSystem.h — Plan generation + deep atomic decomposition (merged from TaskPlanner + TaskDecomposer)
#include "BrainTypes.h"
#include "brain/skills/SkillSystem.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "ToolExecutor.h"
#include <string>
#include <vector>
#include <functional>

// ── §TaskPlanner types ────────────────────────────────────────────────────────

enum class PlanStatus { PENDING_APPROVAL, APPROVED, RUNNING, COMPLETED, CANCELLED, FAILED };

struct PlanStep {
    int         stepNo;
    std::string action;
    std::string tool;
    std::string expectedOutput;
    bool        requiresNet = false;
    bool        writesFiles = false;
    bool        runsCode    = false;
};

struct TaskPlan {
    std::string             planId;
    std::string             goal;
    std::string             requestMode;
    std::vector<PlanStep>   steps;
    std::vector<std::string> requiredTools;
    std::vector<std::string> requiredPermissions;
    std::vector<std::string> risks;
    std::string             estimatedDuration;
    PlanStatus              status    = PlanStatus::PENDING_APPROVAL;
    std::string             savedPath;
};

class TaskPlanner {
public:
    TaskPlan    buildPlan(const CognitiveSituation& situation) const;
    std::string formatForApproval(const TaskPlan& plan) const;
    bool        savePlan(TaskPlan& plan) const;
    bool        approvePlan(const std::string& planId);
    bool        isApprovalSignal(const std::string& input) const;
    bool        isRejectionSignal(const std::string& input) const;
    const TaskPlan& lastPlan() const { return lastPlan_; }
    bool        hasPendingPlan() const { return lastPlan_.status == PlanStatus::PENDING_APPROVAL; }
private:
    TaskPlan buildWebsitePlan(const CognitiveSituation& sit) const;
    TaskPlan buildTradingPlan(const CognitiveSituation& sit) const;
    TaskPlan buildResearchPlan(const CognitiveSituation& sit) const;
    TaskPlan buildGenericPlan(const CognitiveSituation& sit) const;
    static std::string makePlanId();
    mutable TaskPlan lastPlan_;
};

// ── §TaskDecomposer types ─────────────────────────────────────────────────────

struct AtomicTask {
    std::string id, topic, why, learnQuery;
    int  estimatedMins;
    bool isBlocker;
};

struct DecompositionTree {
    std::string              domain, goalSummary, scaffoldPath, scaffoldCode;
    std::vector<AtomicTask>  atoms;
    std::vector<std::string> libraries, apis, triggerPatterns;
    int  totalAtoms              = 0;
    int  estimatedLearningHours  = 0;
    bool needsApproval           = true;
};

struct CustomTaskDef {
    std::string              name, description;
    std::vector<std::string> keywords, userHints;
    bool approved = false;
};

struct DomainKnowledgeEntry {
    std::string              domainName;
    std::vector<std::string> detectionKeywords, libraries, apis, triggerPatterns;
    std::vector<AtomicTask>  atoms;
};

class TaskDecomposer {
public:
    TaskDecomposer();
    static bool       isNewTaskRequest(const std::string& input);
    DecompositionTree decompose(const std::string& taskDescription);
    void              queueLearning(const DecompositionTree& tree, KnowledgeDaemon* daemon);
    RuntimeSkill      buildSkill(const DecompositionTree& tree) const;
    bool              writeScaffold(const DecompositionTree& tree) const;
    std::string       formatPlan(const DecompositionTree& tree) const;
    DecompositionTree registerCustomTask(const std::string& name, const std::vector<std::string>& hints);
    std::string       extractTaskSubject(const std::string& rawInput) const;
    void load();
    void save() const;
private:
    std::vector<DomainKnowledgeEntry> domainDB_;
    std::vector<CustomTaskDef>        customTasks_;
    std::string detectDomain(const std::string& input) const;
    DecompositionTree decomposeDomain(const DomainKnowledgeEntry& entry, const std::string& rawInput) const;
    DecompositionTree decomposeGeneric(const std::string& taskDescription) const;
    std::vector<std::string> extractKeywords(const std::string& text) const;
    std::string generateScaffold(const DecompositionTree& tree) const;
    std::string makeAtomId(int i) const;
    static std::string toLower(const std::string& s);
    static bool hasWord(const std::string& h, const std::string& n);
};
