#pragma once
// AuditSystem.h — Trace storage + self-audit (merged from TraceStore + SelfAuditEngine)
#include "BrainTypes.h"
#include "brain/memory/KnowledgeStore.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "brain/reasoning/TaskSystem.h"
#include <string>
#include <mutex>
#include <vector>
#include <map>
#include <functional>

// ── §10 Trace Storage ─────────────────────────────────────────────────────────

class TraceStore {
public:
    TraceStore();
    explicit TraceStore(const std::string& filePath);

    bool append(const FullTrace& trace);
    std::vector<FullTrace> loadRecent(int maxCount = 50) const;
    int sessionCount() const;

private:
    std::string serializeTrace(const FullTrace& trace) const;
    bool parseTraceLine(const std::string& line, FullTrace& out) const;

    std::string      filePath_;
    int              sessionCount_ = 0;
    mutable std::mutex mutex_;
};

// ── Self-Audit ─────────────────────────────────────────────────────────────────

struct AuditFinding {
    std::string topic;
    int         failCount;
    float       avgConf;
    std::string action;
};

struct SelfAuditReport {
    int                       tracesAnalyzed = 0;
    int                       failuresFound  = 0;
    std::vector<AuditFinding> findings;
    std::string               summary;
};

class SelfAuditEngine {
public:
    SelfAuditEngine() = default;

    SelfAuditReport runAudit(
        TraceStore&      traceStore,
        KnowledgeDaemon* knowledge,
        TaskDecomposer&  decomposer,
        ConceptVault&    vault,
        int              lookbackTraces = 50);

    int  auditEveryNTurns = 10;
    bool isDue(int currentTurnCount) const;
    void markAuditRan(int currentTurnCount);
    const SelfAuditReport& lastReport() const { return lastReport_; }

private:
    std::map<std::string, std::vector<float>>
        findFailuresByTopic(const std::vector<FullTrace>& traces) const;
    std::string extractTopic(const FullTrace& trace) const;
    std::vector<std::string> findQualityIssues(const std::vector<FullTrace>& traces) const;
    static std::string toLower(const std::string& s);

    int             lastAuditTurn_ = 0;
    SelfAuditReport lastReport_;
};
