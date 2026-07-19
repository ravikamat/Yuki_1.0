#pragma once
// BrainTypes.h
// Yuki_1.0 — Canonical type definitions for the cognitive architecture.
// All structs defined in the Yuki Brain Specification live here.
// InputSourceKind lives in PerceptionLayer.h (extended with Brain Spec values).

#include "../input/PerceptionLayer.h"   // InputSourceKind, PerceptionSourceType
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// ──────────────────────────────────────────────────────────────────────────────
// § 3.1  Perception Layer — Canonical Input Event
// ──────────────────────────────────────────────────────────────────────────────

struct CanonicalInputEvent {
    std::string      eventId;
    InputSourceKind  sourceKind  = InputSourceKind::TYPED;
    std::string      rawText;
    std::string      normalizedText;
    uint64_t         timestampMs = 0;
    float            confidence  = 1.0f;
    std::map<std::string, std::string> metadata;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.2  Pattern Layer
// ──────────────────────────────────────────────────────────────────────────────

enum class RequestMode {
    QUESTION,
    COMMAND,
    CONTINUATION,
    CLARIFICATION,
    RESEARCH,
    IMPLEMENTATION,
    DESIGN,
    UNKNOWN
};

enum class OutputMode {
    TEXT,
    BULLETS,
    ARCHITECTURE,
    CODE,
    PATCH,
    REPORT,
    MIXED
};

struct PatternFrame {
    std::string              interactionId;
    std::string              rawInput;
    std::string              normalizedInput;
    RequestMode              requestMode       = RequestMode::UNKNOWN;
    OutputMode               outputMode        = OutputMode::TEXT;
    std::string              coreIntent;
    std::string              desiredOutcome;
    std::vector<std::string> entities;
    std::vector<std::string> explicitConstraints;
    std::vector<std::string> inferredConstraints;
    std::vector<std::string> unknownSlots;
    bool                     dependsOnHistory    = false;
    bool                     needsFreshKnowledge = false;
    float                    confidence          = 0.0f;
    // ── Phase 1 GoalSpec-enriched fields ──────────────────────────────────────
    // Populated from LanguageLayer + SemanticParser via GoalSpec.
    // Available to ALL downstream pipeline steps (agents, synthesis, etc.).
    std::string              domain;            // tech/communication/food/health/creative/system
    std::string              tone;              // emotional/urgent/inquisitive/directive/casual
    std::string              language;          // "en" / "hi-en" / "hi"
    std::string              responseStyle;     // "english" / "hinglish" / "hindi"
    std::string              goal;              // explicit goal from GoalSpec
    std::vector<std::string> gaps;              // missing information from GoalSpec
    bool                     isEmotional        = false;
    bool                     needsClarification = false;
    bool                     needsResearch      = false;
    bool                     needsExecution     = false;
    std::map<std::string, std::string> knownSlots;  // filled semantic slots from SemanticParser
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.3  Cognitive Situation Layer
// ──────────────────────────────────────────────────────────────────────────────

struct GoalHierarchy {
    std::string              primaryGoal;
    std::vector<std::string> secondaryGoals;
};

struct UserStateEstimate {
    float urgency          = 0.0f;
    float confusion        = 0.0f;
    float frustration      = 0.0f;
    float depthExpectation = 0.0f;
};

struct ConversationMomentum {
    int                      turnIndex           = 0;
    bool                     activeProjectThread = false;
    std::vector<std::string> unresolvedFromHistory;
};

struct CognitiveSituation {
    PatternFrame             pattern;
    GoalHierarchy            goals;
    UserStateEstimate        userState;
    ConversationMomentum     momentum;
    std::vector<std::string> likelyMemoryZones;
    std::vector<std::string> likelyToolZones;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 4  Task Genome
// ──────────────────────────────────────────────────────────────────────────────

enum class SearchMode {
    INTERNAL_ONLY,
    HYBRID_LOCAL_WEB,
    WEB_HEAVY,
    CODEBASE_HEAVY,
    TOOL_EXECUTION,
    MULTIMODAL
};

struct TaskGenome {
    std::string              taskId;
    std::string              coreGoal;
    std::vector<std::string> subGoals;
    std::vector<std::string> dependencies;
    std::vector<std::string> unresolvedFacts;
    std::vector<std::string> verificationNeeds;
    std::vector<std::string> suggestedAgentFamilies;
    SearchMode               searchMode           = SearchMode::INTERNAL_ONLY;
    float                    complexityScore      = 0.0f;
    float                    noveltyScore         = 0.0f;
    float                    riskScore            = 0.0f;
    bool                     canAnswerFromMemoryOnly    = true;
    bool                     requiresExternalGrounding = false;
    bool                     candidateForNewSkill      = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.5  Agent Swarm Layer
// ──────────────────────────────────────────────────────────────────────────────

struct AgentSpec {
    std::string              agentId;
    std::string              family;
    std::string              mission;
    std::vector<std::string> allowedSources;
    std::vector<std::string> requiredOutputs;
    std::vector<std::string> constraints;
    int                      maxSteps   = 5;
    int                      timeoutMs  = 3000;
    bool                     allowToolCalls = false;
    bool                     ephemeral  = true;
};

struct AgentResult {
    std::string                        agentId;
    bool                               success    = false;
    std::vector<std::string>           claims;
    std::vector<std::string>           unresolved;
    std::map<std::string, std::string> metadata;
    float                              confidence = 0.0f;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 8  Agent Plan
// ──────────────────────────────────────────────────────────────────────────────

struct AgentPlan {
    std::string              planId;
    std::vector<AgentSpec>   agents;
    int                      totalBudgetMs           = 5000;
    bool                     requireVerification     = true;
    bool                     requireContradictionPass = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.6  Evidence Layer
// ──────────────────────────────────────────────────────────────────────────────

struct EvidenceNode {
    std::string              nodeId;
    std::string              claim;
    std::string              sourceType;
    std::string              sourceId;
    float                    confidence  = 0.0f;
    bool                     verified    = false;
    std::vector<std::string> supports;
    std::vector<std::string> contradicts;
    std::vector<std::string> fillsSlots;
};

struct EvidenceGraph {
    std::vector<EvidenceNode> nodes;
    std::vector<std::string>  unresolvedQuestions;
    float                     trustScore = 0.0f;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.7  Synthesis Layer
// ──────────────────────────────────────────────────────────────────────────────

struct SynthesisPlan {
    OutputMode               outputMode          = OutputMode::TEXT;
    std::vector<std::string> mustInclude;
    std::vector<std::string> optionalEnhancements;
    bool                     includeUncertainty  = false;
    bool                     includeActionPlan   = false;
};

struct SynthesisResult {
    bool        complete            = false;
    std::string finalText;
    std::vector<std::string> notes;
    float       groundedConfidence  = 0.0f;
};

struct FinalResponse {
    std::string text;
    bool        canSpeak                = true;
    bool        requiresToolExecution   = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 9  Verification
// ──────────────────────────────────────────────────────────────────────────────

struct VerificationReport {
    bool                     satisfied         = false;
    std::vector<std::string> missingNeeds;
    std::vector<std::string> weakClaims;
    float                    satisfactionScore = 0.0f;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 10  Trace model
// ──────────────────────────────────────────────────────────────────────────────

struct FullTrace {
    std::string          traceId;
    CanonicalInputEvent  input;
    PatternFrame         pattern;
    CognitiveSituation   situation;
    TaskGenome           genome;
    AgentPlan            plan;
    std::vector<AgentResult> results;
    EvidenceGraph        evidence;
    SynthesisResult      synthesis;
    VerificationReport   verification;
    uint64_t             startedAtMs = 0;
    uint64_t             endedAtMs   = 0;
    bool                 success     = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 3.8  Experience and Evolution
// ──────────────────────────────────────────────────────────────────────────────

struct SkillCapsule {
    std::string              skillId;
    std::string              name;
    std::vector<std::string> triggers;
    std::vector<std::string> antiTriggers;
    std::vector<std::string> steps;
    std::vector<std::string> requiredTools;
    std::vector<std::string> validationRules;
    float                    successRate = 0.0f;
    bool                     active      = true;
};

struct ImprovementProposal {
    std::string              proposalId;
    std::string              targetLayer;
    std::string              summary;
    std::vector<std::string> expectedBenefits;
    std::vector<std::string> risks;
    bool                     requiresSandbox = true;
};

// ──────────────────────────────────────────────────────────────────────────────
// § 6  Retrieval Router
// ──────────────────────────────────────────────────────────────────────────────

struct RetrievalHit {
    std::string sourceId;
    std::string sourceType;
    std::string content;
    float       relevance   = 0.0f;
    float       trust       = 0.0f;
    uint64_t    timestampMs = 0;
};
