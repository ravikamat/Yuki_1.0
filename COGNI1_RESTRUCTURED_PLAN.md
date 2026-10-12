# COGNI-1 Restructured Plan (Yuki Target Architecture)
**Merged from both plans | ~22 weeks | Yuki C++ codebase as port target**

---

## Architecture Mapping (Yuki → COGNI-1 Layers)

| COGNI-1 Layer | Yuki Component | Status | Files |
|---|---|---|---|
| **Perception (VSE + HPC)** | `VariationalStateEstimator`, `HierarchicalPredictiveCoding`, `PrecisionPredictor` | ✅ Complete | `inference/VariationalStateEstimator.*`, `core/HierarchicalPredictiveCoding.*`, `inference/PrecisionPredictor.*` |
| **HDC/VSA Algebra** | `SparseHDC` (1% sparse 10K), `Hypervector` (binary 10K + FHRR 2048) | ✅ Complete | `memory/SparseHDC.h`, `memory/Hypervector.h` |
| **Item Memory / Cleanup** | `HdcSemanticGraph` (concepts + bound edges) | ⚠️ Partial - needs namespaced roles/fillers, decayed accumulator fix | `memory/HdcSemanticGraph.*` |
| **Workspace** | `GlobalWorkspace` (coalition competition) + `CognitiveSnapshot` (phonological store) | ⚠️ Wrong abstraction - needs slot-based WM with attention gate | `infrastructure/GlobalWorkspace.h`, `field/CognitiveSnapshot.h` |
| **Knowledge (Facts + Rules + Causal)** | `HdcSemanticGraph`, `CausalGraph`, `CausalWorldModel`, `KnowledgeDaemon` | ⚠️ Facts exist, Rules + STRIPS model missing | `memory/HdcSemanticGraph.*`, `causality/*`, `learning/KnowledgeDaemon.*` |
| **Reasoner** | `ActiveInferencePolicyEngine`, `LatentMPCPlanner`, `PatternEngine`, `TaskSystem` | ❌ Missing: deduction/abduction/analogy, Clingo integration | `policy/*`, `planning/*`, `reasoning/*` |
| **Verifier** | `ContradictionDetector` | ⚠️ Wrong abstraction - needs tiered (constraints → world-model → simulator), fail-closed | `knowledge/ContradictionDetector.*` |
| **Learner** | `SleepThread`, `DreamEngine`, `CounterfactualReplayEngine`, `DistillationExtractor` | ⚠️ Partial - needs rule induction, automated acceptance gates, shadow mode | `sleep/*` |
| **Language** | `LocalTransformer`, `TieredModels`, `GrammarEngine`, `BrocaTranslator`, `WernickeComprehender` | ✅ Exists - needs to be optional plugin, verbalizer only for verified conclusions | `language/*` |

---

## Phase 0: Domain + Harness + Yuki Cleanup (Weeks 1-2)

### Yuki Cleanup (from Phase 0 original plan)
- [ ] **Fix VSEConfig duplication**: Single source in `CognitiveConfig.h`, remove alias in `VariationalStateEstimator.h:40`
- [ ] **Fix PrecisionPredictor header**: Remove forward declaration, use concrete include
- [ ] **not_in_use isolation**: Archive or delete 100+ debug files in `not_in_use/`

### Domain Selection: **Gridworld (Blocksworld variant)**
- Discrete 5x5 grid, 3-5 blocks, gripper agent
- Tasks: retrieval, multi-hop, deduction, abduction, analogy, planning
- Simulator = Tier 2 ground truth (deterministic, fast, resetable)
- Symbol set: `block(A)`, `on(A,B)`, `clear(A)`, `holding(A)`, `at(gripper,X,Y)`

### Harness Build
| Component | Yuki Target | New File |
|---|---|---|
| Simulator | `simulation/VirtualSimulation.h` | `tests/cogni1/GridworldSimulator.h` |
| Task Generator | — | `tests/cogni1/TaskGenerator.h` |
| Metrics + JSONL Logger | `field/CognitiveEvent` | `tests/cogni1/MetricsLogger.h` |
| Brute-force Oracle | — | `tests/cogni1/BruteForceOracle.h` (BFS over state space) |
| Baseline Runner | — | `tests/cogni1/BaselineRunner.h` |

### Gate 0
- Oracle solves 100% of generated task types
- Yuki baseline recorded on all task types (current system, no VSA reasoning)

---

## Phase 1: HDC Core Rewrite (Weeks 3-5)

### Fix: Decayed Accumulator (Critical Bug)
**Current**: `HdcSemanticGraph` thresholds only on read, accumulator decays silently  
**Fix**: `Accumulator` class with explicit decay-on-write, threshold-on-read, age tracking

```cpp
// New: memory/Accumulator.h
struct Accumulator {
    struct Entry { Hypervector vec; uint64_t age_ticks; uint32_t count; };
    void add(const Hypervector& v, uint64_t now);  // decay existing, add new, increment count
    std::optional<Hypervector> get(uint64_t now, float threshold);  // threshold on read
    void decay(float rate, uint64_t now);
};
```

### Namespaced ItemMemory (Roles vs Fillers Separate)
**Current**: Single `concept_cache_` for all concepts  
**Fix**: Two namespaces - `RoleMemory` (relations, fixed) + `FillerMemory` (entities, growing)

```cpp
// New: memory/ItemMemory.h
class ItemMemory {
    RoleMemory roles;      // "on", "clear", "holding", "at" - fixed, high precision
    FillerMemory fillers;  // "block_1", "block_2", "pos_3_4" - growing, age-tracked
    Hypervector bind(const std::string& role, const std::string& filler) const;
    std::optional<std::string> unbind_role(const Hypervector& query) const;
    std::optional<std::string> unbind_filler(const Hypervector& query) const;
};
```

### Nested Encoder/Decoder with Confidence Margin
- Encode: `bind(role, filler)` → `bundle(bind(r1,f1), bind(r2,f2), ...)`
- Decode: `unbind(role, composite)` → cosine similarity → return filler if `sim > margin`
- Margin = `mean(top_k) + 2*std(top_k)` (adaptive)

### Capacity Sweeps
| Parameter | Range | Metric |
|---|---|---|
| D (dimensions) | 2000, 5000, 10000, 20000 | Cleanup accuracy |
| Items per namespace | 50, 100, 200, 500 | Cleanup accuracy |
| Nesting depth | 1, 2, 3, 4 | Decode accuracy |
| Shared roles | 5, 10, 20 | Interference |

### Gate 1
- Cleanup accuracy ≥ 0.99 at domain's required D and depth (target: D=10000, depth=3, 100 items)
- All sweeps logged with seed

---

## Phase 2: Baselines (Weeks 5-6)

### Implement Baselines (run on ALL tasks)
1. **Structured Fact Store** - `HdcSemanticGraph` as explicit graph (no VSA reasoning)
2. **Dense Embedding Index** - `LocalTransformer` embeddings + FAISS/ANN
3. **Random Policy** - Random action selection
4. **Heuristic Policy** - `ActiveInferencePolicyEngine` with hand rules only

### Metrics per Task Type
| Task Type | Metric |
|---|---|
| Retrieval | Accuracy @1, @5 |
| Multi-hop | Path accuracy |
| Deduction | Proof success rate |
| Abduction | Best explanation accuracy |
| Analogy | Mapping accuracy |
| Planning | Success rate, steps to goal |

### Gate 2
- VSA beats **at least one baseline** on **at least one named axis**:
  - Noise tolerance (corrupted input)
  - Analogy (zero-shot mapping)
  - One-shot symbol addition (new block/relation)
  - Compactness (memory per fact)
- **If not**: Restrict VSA to analogy + similarity only; use explicit graph for deduction

---

## Phase 3: Perception + Grounding (Weeks 6-8)

### Interface: `PerceptGrounder`
```cpp
// New: perception/PerceptGrounder.h
struct GroundedSymbol { std::string symbol; float confidence; };
using GroundingResult = std::vector<GroundedSymbol>;

class PerceptGrounder {
public:
    virtual GroundingResult ground(const GridworldObservation& obs) = 0;
    virtual void calibrate(const std::vector<GroundingResult>& predictions,
                           const std::vector<std::vector<std::string>>& ground_truth) = 0;
};

// Implementations:
// 1. SymbolicGrounder - simulator symbols + noise/dropout layer (Phase 3a)
// 2. EncoderGrounder - pretrained encoder + prototype classifier + calibration (Phase 3b)
```

### Calibration
- Isotonic regression or temperature scaling per symbol
- Output: ECE (Expected Calibration Error) per symbol, overall

### Gate 3
- Symbol accuracy ≥ 95% on clean input
- ECE ≤ 0.05 on corrupted input (dropout 20%, Gaussian noise σ=0.1)

---

## Phase 4: Workspace + Knowledge (Weeks 8-10)

### 4a. Working Memory (Slot-based, Attention-gated)
**Replace** `GlobalWorkspace` coalition competition with:

```cpp
// New: workspace/WorkingMemory.h
struct WM_Slot {
    Hypervector content;      // Bound VSA structure
    float salience;           // Attention weight
    uint64_t age_ticks;       // For decay
    std::string provenance;   // "perception", "inference", "retrieval"
};

class WorkingMemory {
    static constexpr size_t MAX_SLOTS = 7;  // Miller's 7±2
    std::array<WM_Slot, MAX_SLOTS> slots_;
    
    // Write: attention-gated (salience > threshold)
    bool write(const Hypervector& content, float salience, const std::string& provenance);
    // Read: age-aware query (decay by age, boost by salience)
    std::vector<std::pair<Hypervector, float>> query(const Hypervector& cue, size_t k);
    // Clear expired
    void decay(float rate, uint64_t now);
};
```

### 4b. Clingo Fact Store with Provenance
```cpp
// New: reasoning/ClingoStore.h
// ASP facts: holds(block_1). on(block_1, block_2). clear(block_2).
// Provenance: each fact has {source, confidence, timestamp}
// Named constraints: :- not holds(X), holding(X).  [constraint_id: "precondition_hold"]
```

### 4c. Hand Rules (10-15) + STRIPS World Model
- Rules in ASP for: pickup, putdown, stack, unstack
- STRIPS operators with preconditions/effects
- All scaffolding - will be removed in Phase 6

### Gate 4
- Exact deduction: 100% on generated deduction tasks
- One-step world-model prediction matches simulator 100%

---

## Phase 5: Reasoner + Verifier + Confidence (Weeks 10-13)

### 5a. Reasoner (behind narrow interface)
```cpp
// New: reasoning/Reasoner.h
struct ReasoningResult {
    std::vector<std::string> conclusion;  // ASP atoms
    std::string proof_trace;              // Clingo --verbose output
    float confidence;                     // Propagated, calibrated
    std::string mode;                     // "deduction" | "abduction" | "analogy" | "planning"
};

class Reasoner {
public:
    // Deduction: forward chaining from WM + KB
    ReasoningResult deduce(const WorkingMemory& wm, const ClingoStore& kb);
    // Abduction: #minimize { cost(A) : assumption(A) }
    ReasoningResult abduct(const WorkingMemory& wm, const ClingoStore& kb, 
                           const std::string& observation);
    // Analogy: structure mapping via VSA
    ReasoningResult analogize(const Hypervector& source, const Hypervector& target,
                              const ItemMemory& im);
    // Planning: surprise-gated (policy by default, MPC on surprise)
    ReasoningResult plan(const WorkingMemory& wm, const ClingoStore& kb,
                         const CausalWorldModel& cwm, float surprise);
};
```

### 5b. Verifier (Tiered, Fail-Closed)
```cpp
// New: verification/Verifier.h
enum class VerifierTier { TIER_0_CONSTRAINTS, TIER_1_WORLD_MODEL, TIER_2_SIMULATOR };

struct VerificationResult {
    bool passed;
    VerifierTier failed_tier;
    std::string violation;
    float confidence_penalty;
};

class Verifier {
public:
    // Tier 0: ASP constraints (instant)
    VerificationResult checkConstraints(const ReasoningResult& r, const ClingoStore& kb);
    // Tier 1: CausalWorldModel prediction (fast)
    VerificationResult checkWorldModel(const ReasoningResult& r, const CausalWorldModel& cwm);
    // Tier 2: GridworldSimulator rollout (slow, ground truth)
    VerificationResult checkSimulator(const ReasoningResult& r, GridworldSimulator& sim);
    
    // Fail-closed: all tiers must pass
    bool verify(const ReasoningResult& r, const ClingoStore& kb, 
                const CausalWorldModel& cwm, GridworldSimulator& sim);
};
```

### 5c. Confidence Propagation + Calibration
- Per-signal calibration (VSE precision, HDC similarity, Clingo model probability, Verifier tier)
- Temperature scaling on held-out episodes

### 5d. Surprise Gating Validation
- Measure: `surprise` (from VSE/HPC) vs `verifier_failure`
- If AUC < 0.7: switch gate to `verifier_disagreement` or `ensemble_variance`

### Gate 5
- Matches or beats baselines on all task types
- Verifier catches ≥ 90% of injected errors (mutation testing)
- Surprise predicts verifier failure (AUC ≥ 0.7) OR gate switched to working signal

---

## Phase 6: Learner (Weeks 13-18) — **CRITICAL MILESTONE**

### 6a. Rule Induction (Template ILP)
```cpp
// New: learning/RuleInduction.h
struct RuleTemplate {
    std::string head;           // "on(X,Y) :- "
    std::vector<std::string> body_templates;  // ["clear(X)", "on(X,Z)", "clear(Y)"]
    std::vector<std::string> variables;       // ["X", "Y", "Z"]
};

class RuleInduction {
    // Mine frequent patterns from episode log (LearningEpisode)
    std::vector<RuleTemplate> minePatterns(const std::vector<LearningEpisode>& episodes);
    // Score candidate rules on held-out episodes
    float scoreRule(const RuleTemplate& rule, const std::vector<LearningEpisode>& test);
    // Generate ASP rule string
    std::string toASP(const RuleTemplate& rule) const;
};
```

### 6b. World-Model Learning
- Learn `CausalTransition` from `(state, action, next_state)` tuples
- Already partially in `CausalWorldModel::learnTransition` - complete it

### 6c. Compression-Based Concept Creation (MDL)
```cpp
// New: learning/ConceptFormation.h
// If new concept C reduces description length of episodes by > threshold, create it
float descriptionLength(const std::vector<LearningEpisode>& episodes, 
                        const ItemMemory& im);
bool shouldCreateConcept(const std::string& candidate, 
                         const std::vector<LearningEpisode>& episodes);
```

### 6d. Active Experimentation
- Identify weakest beliefs (low confidence, high surprise, high prediction error)
- Generate actions to test them (interventions via `CausalWorldModel`)
- Execute in simulator, update KB

### 6e. Sleep Replay + Pruning (Extend `SleepThread`)
- Replay failed episodes → generate candidate rules → test on held-out → prune bad
- Consolidate good rules into versioned store

### 6f. Versioned Rule Store + Shadow Mode + Automated Acceptance Gates
```cpp
// New: learning/RuleStore.h
struct RuleVersion {
    std::string rule_asp;
    float performance;          // On held-out test set
    uint64_t promoted_at;
    bool active;
};

class RuleStore {
    std::vector<RuleVersion> versions;
    
    // Shadow mode: run new rules alongside old, log disagreements
    void promoteToShadow(const std::string& rule_asp);
    // Automated gate: if shadow performance ≥ 0.7 * active performance on held-out
    // AND no new contradictions → auto-promote
    bool checkPromotionGate(const std::string& rule_asp);
    // Rollback: if active rule causes regression → revert
    void rollback(const std::string& rule_asp);
};
```

### Gate 6 (THE REAL MILESTONE)
- **Remove hand rules and hand-built world model**
- System recovers ≥ 70% of Phase 5 performance **from experience alone**
- With Learner off, live loop unchanged (shadow mode only)

---

## Phase 7: Generalization + Continual (Weeks 18-21)

### 7a. Compositional Held-Out Splits
- Train on: 2-block tasks, 3-block tasks
- Test on: 4-block tasks, novel configurations (never seen in training)

### 7b. Second Domain: **Gridworld Navigation** (same grid, different physics)
- Agent moves, no blocks, goal reaching
- Transfer: reuse VSA encoder, ItemMemory, Reasoner, Verifier
- Measure: fraction of Domain 1 performance with < 1000 new episodes

### 7c. 10K-Turn Continual Run
- Fixed held-out test set (100 tasks per type)
- Measure regression every 1000 turns
- Belief revision with branching hypotheses (from `BeliefRevisionConfig`)

### 7d. Belief Revision (Implement `BeliefRevisionConfig`)
- Surprise-triggered counterfactual generation
- Branching: maintain multiple hypothesis branches
- Consolidate when evidence converges

### Gate 7
- Novel compositions solved at stated percentage (target: ≥ 60%)
- ≤ 5% regression on old tasks over 10K turns
- Domain 2 reaches ≥ 50% of Domain 1 with ≤ 1000 episodes

---

## Phase 8: Language + Efficiency (Weeks 21-22)

### 8a. Language as Optional Plugin
```cpp
// New: language/Verbalizer.h (thin wrapper over existing GrammarEngine)
class Verbalizer {
    // Only verbalizes VERIFIED conclusions
    std::string verbalize(const ReasoningResult& r, const VerificationResult& v);
    // Proof trace → natural language
    std::string traceToText(const std::string& proof_trace);
};

// Existing LocalTransformer becomes optional QueryEncoder (only if free-form input needed)
```

### 8b. Profile-First Optimization
- Profile core loop (no language) with `perf` / `VTune`
- Apply sparsity (`SparseHDC`), INT8 quantization (`INT8Quantization.h`), event-driven compute
- **Only where measurements show bottleneck**

### Gate 8
- Core runs with language **disabled**
- Latency targets set from measurements (p50, p99 per component)

---

## Kill Criteria (Decision Points)

| Risk | Phase | Test | If Fails |
|---|---|---|---|
| VSA can't hold deep structure or beat baselines | 1-2 | Gate 1, Gate 2 | Use explicit graph (`HdcSemanticGraph`) + vectors only for similarity/analogy |
| Perception symbols too noisy | 3 | Gate 3 | Improve encoder or simplify symbol set before proceeding |
| Surprise doesn't predict errors | 5 | Gate 5 | Gate on verifier disagreement or ensemble variance |
| Rule induction doesn't recover hand-rule performance | 6 | Gate 6 | Narrow rule language (Horn clauses only), add minimal seed rules |
| No transfer to domain 2 | 7 | Gate 7 | Report honestly as strong single-domain reasoner |

---

## Yuki-Specific Implementation Notes

### Existing Assets to Leverage
| Yuki Component | COGNI-1 Role | Adaptation Needed |
|---|---|---|
| `SparseHDC` (10K, 1% sparse) | VSA algebra | Add `Accumulator`, namespaced `ItemMemory` |
| `HdcSemanticGraph` | Fact store | Add provenance, age-aware queries, namespaces |
| `CausalWorldModel` | World model | Complete `learnTransition`, `estimateInterventionEffect` |
| `SleepThread` + `DreamEngine` | Sleep consolidation | Add rule induction, shadow mode, automated gates |
| `ActiveInferencePolicyEngine` + `LatentMPCPlanner` | Planner | Add surprise gating, verifier integration |
| `ContradictionDetector` | Verifier Tier 0 | Restructure as tiered verifier, fail-closed |
| `CognitiveSnapshot` + `CognitiveEvent` | Logging/metrics | Extend for COGNI-1 task types, JSONL output |
| `LocalTransformer` | Perception encoder (optional) | Calibrate, use only if needed |

### New Files to Create
```
src/brain/memory/Accumulator.h
src/brain/memory/ItemMemory.h
src/brain/workspace/WorkingMemory.h
src/brain/reasoning/Reasoner.h
src/brain/reasoning/ClingoStore.h
src/brain/verification/Verifier.h
src/brain/learning/RuleInduction.h
src/brain/learning/ConceptFormation.h
src/brain/learning/RuleStore.h
src/brain/perception/PerceptGrounder.h
src/brain/perception/SymbolicGrounder.h
src/brain/perception/EncoderGrounder.h
src/brain/language/Verbalizer.h
tests/cogni1/GridworldSimulator.h
tests/cogni1/TaskGenerator.h
tests/cogni1/MetricsLogger.h
tests/cogni1/BruteForceOracle.h
tests/cogni1/BaselineRunner.h
```

### Integration Points (Narrow Typed Interfaces)
Every layer sits behind an interface so it can be swapped/ablated:
- `IVSAAlgebra` → `SparseHDC` / `Hypervector`
- `IItemMemory` → `ItemMemory` (roles/fillers)
- `IWorkingMemory` → `WorkingMemory`
- `IKnowledgeStore` → `ClingoStore` / `HdcSemanticGraph`
- `IReasoner` → `Reasoner` (deduction/abduction/analogy/planning)
- `IVerifier` → `Verifier` (tiered, fail-closed)
- `ILearner` → `RuleInduction` + `RuleStore` (shadow mode, auto-gates)
- `IWorldModel` → `CausalWorldModel` / `GridworldSimulator`
- `IPerceptGrounder` → `SymbolicGrounder` / `EncoderGrounder`
- `ILanguage` → `Verbalizer` (optional)

---

## Success Criteria (Measurable, from both plans)

1. **Novel compositional tasks** solved at stated % on held-out split (Gate 7)
2. **Every answer** has proof trace + verifier pass (Gate 5)
3. **Learned rules** reach ≥ 70% of hand-rule performance (Gate 6)
4. **No forgetting** > 5% over 10K turns (Gate 7)
5. **Zero unverified claims** reach output by construction (Gate 5 fail-closed)
6. **Transfer** to Domain 2 at stated fraction with little new data (Gate 7)
7. **Core runs without language**; latency from measurements (Gate 8)

---

## Dependencies

- **Clingo** (ASP solver) - behind `IReasoner`/`IKnowledgeStore` interface
  - If zero external deps needed later: replace with native forward-chainer
- **Pretrained encoder** - only for `EncoderGrounder` (perception), optional
  - Core reasoner never needs it

---

## What Makes This Different from LLM (from plan)

- Explicit proof trace for every answer
- Acts on verifier-checked conclusion, not likelihood
- Learns from small data via experimentation + compression
- Knowledge inspectable and editable

---

## Next Action

**Confirm domain choice** (Gridworld/Blocksworld) and I'll generate:
1. Concrete task specifications for Gate 0
2. File-by-file implementation order for Phase 0-1
3. Interface definitions for all new components