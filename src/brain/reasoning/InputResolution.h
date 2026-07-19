#pragma once
// InputResolution.h — Clarification engine + unknown topic flow
//                     (merged from ClarificationEngine + UnknownTopicFlow)
#include "BrainTypes.h"
#include "brain/memory/KnowledgeStore.h"    // StreamParseResult, MiniIntent, ConceptVault
#include "brain/reasoning/EvidenceSystem.h"    // VerificationReport
#include "brain/learning/KnowledgeDaemon.h"
#include <string>
#include <vector>

// Forward declarations (heavy headers kept out of this header)
class WebReconAgent;
class TaskDecomposer;

// ── §ClarificationEngine ──────────────────────────────────────────────────────

struct ClarificationNeeded {
    bool        needed       = false;
    std::string question;
    std::string missingPiece;
    float       urgency      = 0.0f;
};

class VectorStore;
class EmbeddingEngine;
struct GoalModel;
class UserMemory;

// ── §ClarificationState — per-goal retry tracker (owned by MotherCore) ────────
// Tracks how many times each unknown slot has been asked, and which slots
// were already inferred so we never ask about them again.
struct ClarificationState {
    std::map<std::string, int> askCount;     // slot -> times asked so far
    std::map<std::string, std::string> inferred; // slot -> inferred value
    static constexpr int MAX_ASK_PER_SLOT = 2;  // ask once, retry once, then research

    void reset() { askCount.clear(); inferred.clear(); }
    bool exhausted(const std::string& slot) const {
        auto it = askCount.find(slot);
        return it != askCount.end() && it->second >= MAX_ASK_PER_SLOT;
    }
    void recordAsked(const std::string& slot) { askCount[slot]++; }
    bool wasAsked(const std::string& slot) const { return askCount.count(slot) > 0; }
    void markInferred(const std::string& slot, const std::string& value) { inferred[slot] = value; }
    bool isInferred(const std::string& slot) const { return inferred.count(slot) > 0; }
};

class ClarificationEngine {
public:
    ClarificationEngine() = default;
    
    void setDependencies(VectorStore* vectorStore, EmbeddingEngine* embeddingEngine);
    
    ClarificationNeeded evaluate(const VerificationReport& report,
                                  const StreamParseResult&  stream,
                                  const PatternFrame&       frame) const;
    
    // From GoalModel (Task Goal Driven) — original, kept for backward-compat
    std::string generateBlockingQuestion(const GoalModel& model) const;

    // GoalModel-driven + memory-aware + confidence-aware (new)
    // Returns the ONE question to ask, or "" if nothing needs asking.
    // Sets model.needsResearch=true (via out-param flag) if a slot is exhausted.
    std::string generateBlockingQuestion(const GoalModel& model,
                                         const UserMemory& memory,
                                         ClarificationState& state,
                                         bool& needsResearchOut) const;

    void recordAsked(const std::string& topic);
    bool alreadyAsked(const std::string& topic) const;
    void clearSession();
private:
    std::string generateQuestion(const std::string& missingPiece,
                                  const PatternFrame& frame,
                                  const std::vector<MiniIntent>& unclearIntents) const;
    static std::string toLower(const std::string& s);
    static bool        has(const std::string& h, const std::string& n);
    
    mutable std::vector<std::string> askedTopics_;
    VectorStore* vectorStore_ = nullptr;
    EmbeddingEngine* embeddingEngine_ = nullptr;
};

// ── §UnknownTopicFlow ─────────────────────────────────────────────────────────

enum class UnknownResolutionSource { VAULT, DAEMON, WEB, ASKED_USER, QUEUED };

struct UnknownTopicResult {
    bool                    handled      = false;
    std::string             response;
    UnknownResolutionSource source       = UnknownResolutionSource::QUEUED;
    float                   confidence   = 0.0f;
    std::string             learnedTerm;
    bool                    questionAsked = false;
};

class UnknownTopicFlow {
public:
    UnknownTopicFlow() = default;
    UnknownTopicResult handle(const std::string&   rawInput,
                               const PatternFrame&  frame,
                               const std::string&   pipelineAnswer,
                               KnowledgeDaemon*     knowledge,
                               ConceptVault&        vault,
                               WebReconAgent&       webRecon,
                               ClarificationEngine& clarif,
                               TaskDecomposer&      decomposer) const;
    static constexpr float CONFIDENCE_THRESHOLD = 0.42f;
private:
    bool tryVault(const std::string& term, ConceptVault& vault, UnknownTopicResult& out) const;
    bool tryDaemon(const std::string& term, KnowledgeDaemon* kd, UnknownTopicResult& out) const;
    bool tryWeb(const std::string& question, const std::string& term,
                WebReconAgent& webRecon, ConceptVault& vault, UnknownTopicResult& out) const;
    std::string extractUnknownTerm(const std::string& lower, const PatternFrame& frame) const;
    std::string buildLearningResponse(const std::string& term, bool questionAsked) const;
    static std::string toLower(const std::string& s);
    static bool        has(const std::string& h, const std::string& n);
};
