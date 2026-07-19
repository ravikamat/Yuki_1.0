# Yuki Brain Specification

Yuki should be built as a cognitive operating system with a stable Mother Core, dynamic specialist agents, a structured evidence graph, and a governed self-improvement loop. The architecture should be local-first, pattern-first, and trace-driven rather than prompt-first, so that understanding, retrieval, execution, and learning all flow through explicit runtime contracts.

## 1. System purpose

Yuki is not a single chatbot. Yuki is a persistent cognitive framework that:
- understands user intent and conversation pattern,
- searches internal memory first,
- creates task-specific logic graphs,
- spawns bounded micro-agents when needed,
- merges evidence structurally,
- produces grounded outputs,
- learns from traces,
- evolves its harness and skills through validated improvement loops.

This design combines ideas from multi-agent supervision, agentic retrieval, and continual-learning systems, while keeping self-modification governed through sandbox validation rather than uncontrolled live mutation. [web:522][web:526][web:531]

## 2. Design principles

1. Pattern-first: understand the human situation before retrieval.
2. Local-first: search internal memory and project knowledge before the web.
3. Mother-governed: all micro-agents are created, constrained, and evaluated by Mother Core.
4. Evidence-grounded: merge claims into a graph, not a raw context bundle.
5. Trace-driven: every task generates a reusable learning trace.
6. Skill-forming: repeated success becomes a reusable Skill Capsule.
7. Sandboxed evolution: routing, policy, and architectural improvements are tested before promotion.
8. Runtime truthfulness: every module reports real state, ownership, and health.
9. Bounded autonomy: every agent has limits, contracts, and kill paths.
10. Separation of cognition and execution: understanding, planning, execution, and learning are distinct layers.

## 3. Permanent layers

### 3.1 Perception Layer
Responsibilities:
- receive input from terminal, UI, speech, files, vision,
- normalize format,
- timestamp,
- create canonical input event,
- attach source metadata.

Core structs:
```cpp
enum class InputSourceKind {
    TERMINAL_TEXT,
    UI_TEXT,
    VOICE_FINAL,
    VOICE_PARTIAL,
    FILE_INPUT,
    IMAGE_INPUT,
    SYSTEM_INTERNAL
};

struct CanonicalInputEvent {
    std::string eventId;
    InputSourceKind sourceKind;
    std::string rawText;
    std::string normalizedText;
    uint64_t timestampMs = 0;
    float confidence = 1.0f;
    std::map<std::string, std::string> metadata;
};
```

### 3.2 Pattern Layer
Responsibilities:
- detect intent,
- detect requested output type,
- extract entities,
- infer implicit constraints,
- determine whether request depends on history,
- estimate whether freshness is required.

Core structs:
```cpp
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
    std::string interactionId;
    std::string rawInput;
    std::string normalizedInput;
    RequestMode requestMode = RequestMode::UNKNOWN;
    OutputMode outputMode = OutputMode::TEXT;
    std::string coreIntent;
    std::string desiredOutcome;
    std::vector<std::string> entities;
    std::vector<std::string> explicitConstraints;
    std::vector<std::string> inferredConstraints;
    std::vector<std::string> unknownSlots;
    bool dependsOnHistory = false;
    bool needsFreshKnowledge = false;
    float confidence = 0.0f;
};
```

### 3.3 Cognitive Situation Layer
Responsibilities:
- merge PatternFrame with memory, conversation momentum, and user-state estimate,
- build the live situation model used by Mother Core.

Core structs:
```cpp
struct GoalHierarchy {
    std::string primaryGoal;
    std::vector<std::string> secondaryGoals;
};

struct UserStateEstimate {
    float urgency = 0.0f;
    float confusion = 0.0f;
    float frustration = 0.0f;
    float depthExpectation = 0.0f;
};

struct ConversationMomentum {
    int turnIndex = 0;
    bool activeProjectThread = false;
    std::vector<std::string> unresolvedFromHistory;
};

struct CognitiveSituation {
    PatternFrame pattern;
    GoalHierarchy goals;
    UserStateEstimate userState;
    ConversationMomentum momentum;
    std::vector<std::string> likelyMemoryZones;
    std::vector<std::string> likelyToolZones;
};
```

### 3.4 Mother Core
Responsibilities:
- preserve identity and continuity,
- build Task Genome,
- decide decomposition strategy,
- spawn agents,
- allocate budgets,
- monitor progress,
- merge evidence,
- synthesize final result,
- trigger learning trace creation.

Core classes:
```cpp
class MotherCore {
public:
    FinalResponse handleInput(const CanonicalInputEvent& input);

private:
    PatternFrame buildPatternFrame(const CanonicalInputEvent& input);
    CognitiveSituation buildSituation(const PatternFrame& frame);
    TaskGenome buildTaskGenome(const CognitiveSituation& situation);
    AgentPlan buildAgentPlan(const TaskGenome& genome);
    std::vector<AgentResult> runAgents(const AgentPlan& plan);
    EvidenceGraph buildEvidenceGraph(const std::vector<AgentResult>& results,
                                     const CognitiveSituation& situation);
    SynthesisResult synthesize(const EvidenceGraph& graph,
                               const CognitiveSituation& situation);
    FinalResponse finalize(const SynthesisResult& synthesis,
                           const CognitiveSituation& situation);
    void writeTrace(const FullTrace& trace);
};
```

### 3.5 Agent Swarm Layer
Responsibilities:
- host ephemeral micro-agents,
- enforce contracts,
- enforce timeouts,
- enforce permissions,
- collect structured outputs.

Core structs:
```cpp
struct AgentSpec {
    std::string agentId;
    std::string family;
    std::string mission;
    std::vector<std::string> allowedSources;
    std::vector<std::string> requiredOutputs;
    std::vector<std::string> constraints;
    int maxSteps = 0;
    int timeoutMs = 0;
    bool allowToolCalls = false;
    bool ephemeral = true;
};

struct AgentResult {
    std::string agentId;
    bool success = false;
    std::vector<std::string> claims;
    std::vector<std::string> unresolved;
    std::map<std::string, std::string> metadata;
    float confidence = 0.0f;
};
```

### 3.6 Evidence Layer
Responsibilities:
- normalize claims,
- preserve provenance,
- connect support and contradiction links,
- track unresolved slots.

Core structs:
```cpp
struct EvidenceNode {
    std::string nodeId;
    std::string claim;
    std::string sourceType;
    std::string sourceId;
    float confidence = 0.0f;
    bool verified = false;
    std::vector<std::string> supports;
    std::vector<std::string> contradicts;
    std::vector<std::string> fillsSlots;
};

struct EvidenceGraph {
    std::vector<EvidenceNode> nodes;
    std::vector<std::string> unresolvedQuestions;
    float trustScore = 0.0f;
};
```

### 3.7 Synthesis Layer
Responsibilities:
- match evidence to original need,
- choose answer structure,
- surface uncertainty,
- produce the actual output.

Core structs:
```cpp
struct SynthesisPlan {
    OutputMode outputMode = OutputMode::TEXT;
    std::vector<std::string> mustInclude;
    std::vector<std::string> optionalEnhancements;
    bool includeUncertainty = false;
    bool includeActionPlan = false;
};

struct SynthesisResult {
    bool complete = false;
    std::string finalText;
    std::vector<std::string> notes;
    float groundedConfidence = 0.0f;
};

struct FinalResponse {
    std::string text;
    bool canSpeak = true;
    bool requiresToolExecution = false;
};
```

### 3.8 Experience and Evolution Layer
Responsibilities:
- store traces,
- create Skill Capsules,
- improve routing policy,
- propose harness changes,
- sandbox-test candidate improvements.

Core structs:
```cpp
struct SkillCapsule {
    std::string skillId;
    std::string name;
    std::vector<std::string> triggers;
    std::vector<std::string> antiTriggers;
    std::vector<std::string> steps;
    std::vector<std::string> requiredTools;
    std::vector<std::string> validationRules;
    float successRate = 0.0f;
    bool active = true;
};

struct ImprovementProposal {
    std::string proposalId;
    std::string targetLayer;
    std::string summary;
    std::vector<std::string> expectedBenefits;
    std::vector<std::string> risks;
    bool requiresSandbox = true;
};
```

## 4. Task Genome

Task Genome is the internal logic blueprint for one request. It is generated after PatternFrame and CognitiveSituation are built.

```cpp
enum class SearchMode {
    INTERNAL_ONLY,
    HYBRID_LOCAL_WEB,
    WEB_HEAVY,
    CODEBASE_HEAVY,
    TOOL_EXECUTION,
    MULTIMODAL
};

struct TaskGenome {
    std::string taskId;
    std::string coreGoal;
    std::vector<std::string> subGoals;
    std::vector<std::string> dependencies;
    std::vector<std::string> unresolvedFacts;
    std::vector<std::string> verificationNeeds;
    std::vector<std::string> suggestedAgentFamilies;
    SearchMode searchMode = SearchMode::INTERNAL_ONLY;
    float complexityScore = 0.0f;
    float noveltyScore = 0.0f;
    float riskScore = 0.0f;
    bool canAnswerFromMemoryOnly = false;
    bool requiresExternalGrounding = false;
    bool candidateForNewSkill = false;
};
```

Genome rules:
- build once per interaction,
- may be revised if early evidence changes task understanding,
- determines decomposition,
- determines retrieval mode,
- determines whether contradiction checking is mandatory,
- determines whether this trace should become a skill candidate.

## 5. Agent families

1. Intent Analyst — clarifies hidden intent and answer shape.
2. History Diver — mines prior conversation and long memory.
3. Local Knowledge Scout — searches internal semantic memory.
4. Code Archaeologist — searches codebase, docs, diffs, tests.
5. Web Recon Agent — external retrieval for unresolved slots only.
6. Graph Builder — builds relation graph across entities and sources.
7. Contradiction Hunter — finds disagreement and weak support.
8. Execution Planner — turns solved problem into action steps.
9. Skill Smith — proposes a Skill Capsule from repeated success.
10. Optimizer Agent — proposes policy or harness improvements.
11. Verifier Agent — checks final answer against original task.

Each family must have:
- strict input schema,
- allowed source list,
- output schema,
- budget,
- timeout,
- kill switch,
- no hidden permissions.

## 6. Retrieval architecture

Yuki should use a hybrid retrieval stack:
- semantic vector search for conceptual matches,
- keyword/BM25 for exact identifiers and APIs,
- knowledge graph traversal for relationships,
- code index search for repository understanding,
- trace search for successful prior runs,
- optional web search for missing or fresh facts.

Suggested interfaces:
```cpp
struct RetrievalHit {
    std::string sourceId;
    std::string sourceType;
    std::string content;
    float relevance = 0.0f;
    float trust = 0.0f;
    uint64_t timestampMs = 0;
};

class RetrievalRouter {
public:
    std::vector<RetrievalHit> searchInternal(const PatternFrame& frame,
                                             const std::vector<std::string>& zones);
    std::vector<RetrievalHit> searchCode(const PatternFrame& frame);
    std::vector<RetrievalHit> searchGraph(const PatternFrame& frame);
    std::vector<RetrievalHit> searchWeb(const PatternFrame& frame,
                                        const std::vector<std::string>& unresolved);
};
```

Rules:
- internal zones searched first,
- unresolved slots explicitly tracked,
- external search only fills missing slots,
- all web evidence tagged as external and time-sensitive.

## 7. Mother Core flow

Detailed flow:
1. Receive CanonicalInputEvent.
2. Build PatternFrame.
3. Build CognitiveSituation.
4. Build TaskGenome.
5. Decide if simple direct answer or decomposed swarm.
6. Build AgentPlan.
7. Run internal agents first.
8. Measure coverage.
9. If unresolved slots remain and freshness or missing facts require it, run external agents.
10. Normalize all outputs into EvidenceNodes.
11. Build EvidenceGraph.
12. Run contradiction and verification passes if needed.
13. Build SynthesisPlan.
14. Generate SynthesisResult.
15. Validate against PatternFrame.
16. Emit FinalResponse.
17. Write FullTrace.
18. Mark candidate for Skill Smith or Optimizer if worthy.

## 8. AgentPlan and execution runtime

```cpp
struct AgentPlan {
    std::string planId;
    std::vector<AgentSpec> agents;
    int totalBudgetMs = 0;
    bool requireVerification = true;
    bool requireContradictionPass = false;
};

class AgentRuntime {
public:
    AgentResult execute(const AgentSpec& spec,
                        const CognitiveSituation& situation,
                        const TaskGenome& genome);
    void cancel(const std::string& agentId);
};
```

Runtime rules:
- each agent gets explicit mission,
- no free-form browsing,
- all outputs structured,
- every step logged,
- timeout means forced return with partial evidence,
- agent failure should not crash Mother Core.

## 9. Verification and contradiction handling

```cpp
struct VerificationReport {
    bool satisfied = false;
    std::vector<std::string> missingNeeds;
    std::vector<std::string> weakClaims;
    float satisfactionScore = 0.0f;
};

class Verifier {
public:
    VerificationReport verify(const PatternFrame& frame,
                              const SynthesisResult& synthesis,
                              const EvidenceGraph& graph);
};
```

Contradiction rules:
- critical tasks must always run contradiction check,
- unsupported claims must be marked or removed,
- conflicting external sources should trigger more retrieval or explicit uncertainty,
- synthesis must not silently flatten contradictions.

## 10. Trace model

```cpp
struct FullTrace {
    std::string traceId;
    CanonicalInputEvent input;
    PatternFrame pattern;
    CognitiveSituation situation;
    TaskGenome genome;
    AgentPlan plan;
    std::vector<AgentResult> results;
    EvidenceGraph evidence;
    SynthesisResult synthesis;
    VerificationReport verification;
    uint64_t startedAtMs = 0;
    uint64_t endedAtMs = 0;
    bool success = false;
};
```

Trace storage uses:
- append-only log,
- redaction for private fields,
- retrieval index by task type,
- skill-mining pass,
- failure-cluster pass,
- benchmark replay set generation.

## 11. Skill learning

Skill creation pipeline:
1. collect successful traces,
2. cluster by similarity,
3. detect repeated high-performing trajectories,
4. extract triggers,
5. extract minimal steps,
6. define validations,
7. create Skill Capsule,
8. test on replay traces,
9. activate skill only if regression-free.

Skill runtime use:
- Mother Core checks if any active Skill Capsule matches the PatternFrame,
- if yes, skill can shortcut planning or provide a template,
- skill never bypasses safety or verification.

## 12. Continual learning layers

Yuki should learn at three levels:

### 12.1 Context learning
- update user profile,
- update project memory,
- refine long-term summaries,
- refine entity and preference tracking.

### 12.2 Harness learning
- improve decomposition policy,
- improve retrieval routing,
- improve agent family selection,
- improve confidence thresholds,
- improve caching and query construction.

### 12.3 Model learning
- only offline,
- only with curated traces,
- only after validation,
- not required for daily improvement.

This separation follows modern continual-learning guidance that improvement can happen at memory, harness, and model layers rather than only via retraining. [web:526]

## 13. Experience Refinery

A background process should periodically analyze traces.

```cpp
class ExperienceRefinery {
public:
    std::vector<SkillCapsule> mineSkills(const std::vector<FullTrace>& traces);
    std::vector<ImprovementProposal> mineImprovements(const std::vector<FullTrace>& traces);
    BenchmarkSuite buildReplayBenchmarks(const std::vector<FullTrace>& traces);
};
```

Responsibilities:
- cluster traces,
- identify repeated failure modes,
- identify strong compositions of agents,
- propose new skills,
- propose routing policy updates,
- build benchmark cases,
- pass proposals to sandbox.

## 14. Sandbox optimizer

Improvements should never be promoted blindly.

```cpp
class SandboxEvaluator {
public:
    SandboxReport evaluate(const ImprovementProposal& proposal,
                           const BenchmarkSuite& suite);
};
```

Promotion rule:
- improvement must beat baseline on groundedness, completeness, regression rate, latency budget, and contradiction rate.

## 15. Safety and governance

Hard rules:
- no live self-rewriting of production core code,
- no unrestricted external data import into memory,
- all agent families permission-scoped,
- all actions auditable,
- all improvements reversible,
- all autonomous execution threshold-gated,
- user-private memory redacted in learning jobs,
- kill switch for every runtime and agent.

## 16. Threading and ownership

Recommended ownership:
- MotherCore: long-lived singleton-like orchestrator owned by main runtime.
- AgentRuntime pool: long-lived worker manager owned by MotherCore.
- Individual micro-agents: ephemeral tasks owned by AgentRuntime.
- RetrievalRouter: long-lived service owned by MotherCore.
- MemoryStore: long-lived persistent service.
- TraceWriter: async writer with bounded queue.
- ExperienceRefinery: background scheduled worker.
- SandboxEvaluator: offline or low-priority runtime.

No detached threads.
All long-lived workers require:
- explicit owner,
- start/stop methods,
- join path,
- failure reporting,
- bounded queues.

## 17. Suggested file architecture

```text
src/
  brain/
    MotherCore.h
    MotherCore.cpp
    PatternEngine.h
    PatternEngine.cpp
    CognitiveSituation.h
    TaskGenome.h
    TaskGenomeBuilder.h
    TaskGenomeBuilder.cpp
    AgentRuntime.h
    AgentRuntime.cpp
    AgentFamilies.h
    EvidenceGraph.h
    EvidenceGraph.cpp
    SynthesisEngine.h
    SynthesisEngine.cpp
    Verifier.h
    Verifier.cpp
    RetrievalRouter.h
    RetrievalRouter.cpp
    TraceStore.h
    TraceStore.cpp
    SkillRegistry.h
    SkillRegistry.cpp
    ExperienceRefinery.h
    ExperienceRefinery.cpp
    SandboxEvaluator.h
    SandboxEvaluator.cpp
  memory/
    MemoryStore.h
    MemoryStore.cpp
    KnowledgeGraph.h
    KnowledgeGraph.cpp
    VectorIndex.h
    VectorIndex.cpp
  runtime/
    RuntimeSupervisor.h
    RuntimeSupervisor.cpp
  tools/
    ToolContracts.h
  ui/
    PresenceShellIntegration.cpp
  main.cpp
```

## 18. Integration with current Yuki

Current modules like BabyMode, CommandRouter, SubsystemControl, MouthRuntime, and SpeechToTextRuntime can be preserved but moved under the new hierarchy:
- BabyMode becomes a thin interaction adapter or is replaced by MotherCore front-end orchestration.
- CommandRouter becomes one execution subsystem used by MotherCore.
- MouthRuntime remains the gold-standard execution subsystem for output.
- SpeechToTextRuntime feeds CanonicalInputEvent into Perception Layer.
- PerceptionLayer is replaced or upgraded into the new Evidence/Trace system.

## 19. Step-by-step build order

Phase 1:
- CanonicalInputEvent
- PatternFrame
- MotherCore skeleton
- RetrievalRouter internal-only
- SynthesisEngine basic
- Verifier basic
- FullTrace logging

Phase 2:
- TaskGenomeBuilder
- AgentRuntime
- Intent Analyst
- History Diver
- Local Knowledge Scout
- Code Archaeologist
- EvidenceGraph

Phase 3:
- Web Recon Agent
- Contradiction Hunter
- advanced verification
- hybrid retrieval stack

Phase 4:
- SkillRegistry
- Skill Smith
- ExperienceRefinery
- benchmark replay

Phase 5:
- SandboxEvaluator
- policy evolution
- safe self-improvement loop

## 20. Final guiding idea

Yuki should not be designed as a model that answers. Yuki should be designed as a brain framework that:
- understands patterns,
- builds internal logic graphs,
- creates temporary minds for subtasks,
- merges evidence structurally,
- verifies its own outputs,
- learns from traces,
- and upgrades itself safely over time.

That is the path toward a truly differentiated assistant architecture grounded in current multi-agent, agentic retrieval, and continual-learning design patterns while still pushing beyond them into a more cognitive operating-system style system. [web:522][web:527][web:531]
