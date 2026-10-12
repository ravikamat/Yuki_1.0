# YUKI — Roadmap & Implementation Plan
> **File:** `YUKI_ROADMAP.md` | **Purpose:** Where are we going?
> **Last Updated:** 2026-09-15
> **Companion files:** [`YUKI_STATUS.md`](YUKI_STATUS.md) (current state) · [`CHANGELOG.md`](CHANGELOG.md) (history) · [`docs/yuki_flow.md`](docs/yuki_flow.md) (architecture logic)

---

## 1. Vision (Locked)

Yuki is a **self-contained digital organism** with zero permanent LLM dependency:

> Bounded education by a teacher model → cord cut forever → grows sequentially like a child, learning only while idle. She thinks via her own symbolic/memory substrate (HDC, SAT solver, causal engine, HTN planner), never through borrowed weights. Honest, verifiable, economically motivated, constitutionally bound, self-modifying only inside a sandbox with rollback.

**Non-negotiables:**
1. Never hallucinate — unverified claims are marked uncertain or researched first
2. Constitution Tier-1 is immutable (compiled) — Yuki can never edit it
3. All self-modification passes sandbox test → promote → Merkle-seal → rollback-on-failure
4. Learning is sequential (developmental stage gates) and idle-only
5. After graduation: no transformer / LoRA / KV-cache / GGUF inference in the runtime path

---

## 2. Completed Milestones

| Milestone | Name | Status | Tests |
|---|---|---|---|
| M0 | SecuritySandbox + SelfTestHarness | ✅ | 2/2 |
| M1 | VariationalStateEstimator | ✅ | 1/1 |
| M1.5 | Metacognition + Policy + Audit | ✅ | 5/5 |
| M2 | CodeSynthesisAgent + ValidationLoop | ✅ | 3/3 |
| M2.5 | PrecisionPredictor Enhancement | ✅ | 1/1 |
| M3 | ResearchPlanner (7-stage DAG) | ✅ | 5/5 |
| M3.2 | ToolDiscovery + ImageRecognition | ✅ | 2/2 |
| M3.4 | ChainReconstructor + MemoryFabric | ✅ | 2/2 |
| M3.5 | UniversalTestOrchestrator | ✅ | 5/5 |
| M3.6 | SelfIntrospection + DynamicProfiler | ✅ | 2/2 |
| M3.8 | IntegrityMonitor + ResourceMonitor | ✅ | 2/2 |
| M4 | TaskDecomposer | ✅ | 8/8 |
| M5 | CapabilityGraph + Standalone Understanding | ✅ | 1/1 |
| M6 | Neural Learning Core + Plasticity | ✅ | 5/5 |
| M7 | PACL + YNC Neuromorphic Substrate | ✅ | Full |
| M8 | Logic, Causality & Planning | ✅ | 13/13 |
| M9 | Metacognitive Self-Model & Drives | ✅ | 6/6 |
| M9.5 | Y2K Feature Integration | ✅ | 11/11 |
| M10 | Combinatorial Creativity, VAE & Identity | ✅ | 6/6 |
| M11 | Counterfactual Simulator & Analogical Reasoning | ✅ | 5/5 |
| M12 | Universal Integration & Standalone Mind | ✅ | 3/3 |
| LIVE_CHAT | Live Chat Loop Repair (WP1+WP2+WP3) | ✅ | Live |
| P0_SEMANTIC | Unified Semantic Layer | ✅ | Live |
| ZERO_HARDCODING | Zero-Hardcoding Remediation | ✅ | 109/109 |
| P1_INPUT | Input Comprehension Fix | ✅ | 107/108 |
| P1_P2_FRONTIER | Multimodal, Self-Play & VAE Generation | ✅ | 114/114 |
| MASS_KIP | Mass Knowledge Ingestion Pipeline | ✅ | 121/121 |
| HYBRID_PARSER | Self-Growing Hybrid Semantic Parser | ✅ | Live |
| SWARM_COGNITIVE_FABRIC | Swarm Cognitive Fabric Phase 1 | ✅ | 34/34 |
| SWARM_CUTOVER_BRIDGE | Swarm Gradual Cutover Bridge Phase 2 | ✅ | 35/35 |
| SWARM_ORCHESTRATOR | Swarm Orchestrator & Agent Lifecycle Phase 3 | ✅ | 31/31 |
| SWARM_HARDENING | Swarm Stress Testing & Hardening Phase 4 | ✅ | 33/33 |
| SWARM_ZERO_HARDCODING | Swarm Zero-Hardcoding Phase A/B/C | ✅ | 33/33 |
| TRUE_BRAIN_REMEDIATION | True Brain Remediation & Action Loop | ✅ | 33/33 |
| HEURISTIC_MEMORY | Heuristic-to-Memory Migration Wave 1+2 | ✅ | 33/33 |
| SOCIAL_DRIVE_WIRING | Social Drives, Boredom & Wake Pipeline | ✅ | 12/12 |
| M5_CONTINUOUS_SELF_IMPROVE | Phase 5: Continuous Self-Improvement Loop | ✅ | 8/8 |
| ALLE_BOOTSTRAP | Autonomous Learning & Cognitive Bootstrap | ✅ | 46/46 |
| TEST_PURGE_RELOCATION | Test Relocation & CMake Decoupling | ✅ | Clean |
| ARCHITECTURE_AUDIT_GRAPH_SYNC | Architecture Audit & Knowledge Graph Sync | ✅ | Clean |
| WIN32_BUILD | MSVC/clang-cl Build Fix — Clean Yuki.exe (519/519) | ✅ | Build |

**Total:** 212 new files, 84 modified, 122/122 tests passing; Build: 519/519 targets ✅

---

## 3. Active Implementation Plan (Sovereign Checklist)

> Legend: `[x]` = verified DONE · `[~]` = BUILT but NOT WIRED · `[ ]` = NOT DONE
> K3 items tagged `(K3-x)` = disciplines from kimi-k3-in-c engineering patterns

### STAGE 0 — BIRTH & EDUCATION (teacher LLM allowed, temporary)

#### 0.1 Knowledge Seeding
- [x] GgufManifoldExtractor — 20K manifolds, 8 threads
- [x] yuki_kb CLI tool
- [x] Bootstrap pack loaded on startup (cognitive_bootstrap.pack, 3.01 GB)
- [x] ConceptNetAdapter, WikidataIngestor
- [x] KnowledgeFilter + GrammarExtractor + PhysicsKnowledgeBase
- [x] HdcBatchEncoder + AutonomousIngestor + KnowledgeIngestionOrchestrator
- [ ] Batch ingestion at scale: ConceptNet 34M + WordNet + FrameNet → HDC triples, deduplicated
- [ ] Ingestion progress/resume checkpoint tracking
- [ ] (K3-7) Layered ingestion verification cheapest-first (count → total → per-source → checksum)
- [ ] (K3-7) Pin source snapshot revision; write provenance marker ONLY after full verification
- [ ] (K3-7) Pre-flight capacity check (disk/RAM) before long ingest
- [ ] (K3-7) Cross-verify own parser against independent implementation; missing record lookup is FATAL

#### 0.2 Synthetic Experience Curriculum
- [x] CurriculumGenerator (M6)
- [x] TrainingDataExporter (SFT/DPO export)
- [ ] Teacher-model generates tool-use episodes → T1 episodic store as Yuki's own memories
- [ ] Clarification-dialogue episodes (coreference training)
- [ ] Failure-correction episode pairs (error-driven seeds)
- [ ] Social episode seeds (drive calibration)
- [ ] Curriculum replay scheduler (repeat weak domains until competence threshold)

#### 0.3 Graduation Exam (gate to sovereignty — K3-style gate ladder)
- [ ] (K3-1) Gate ladder 0→9: CLI refusals→unit kernels→subsystem gates→end-to-end oracle
- [ ] (K3-1) Tiny-oracle: minimal fixture system structurally identical to full cognitive graph
- [ ] (K3-1) Exact-match on discrete decisions; manifest-sourced tolerances on continuous intermediates
- [ ] (K3-1) Equivalence gates: batch vs incremental planner must emit identical decisions
- [ ] (K3-1) Self-verifying fixtures: adversarial fixture that a wrong implementation passes is rejected
- [ ] (K3-1) Verdict-as-exit-code + transcript grep; skips count as failures
- [ ] (K3-1) Scale gate at production shapes with synthetic data (catches silent overflow/sizing bugs)
- [ ] (K3-1) Weightless suite (no teacher, no artifacts, ~1 min) kept permanently green
- [ ] Fixed benchmark suite: memory recall accuracy, intent classification, task completion, honesty score
- [ ] Pass/fail gate wired to feature flag; fail = repeat curriculum

#### 0.4 Amputation (SOVEREIGNTY DAY)
- [ ] Remove Ollama HTTP path (LocalLLM) from turn path
- [ ] Remove llama.cpp in-process path (LocalTransformer) from turn path
- [ ] Remove/simplify GenerationRouter external tiers
- [ ] Purge from CMakeLists.txt; commit `SOVEREIGNTY: external LLM removed`
- [ ] Verify: full build + all tests green with zero LLM linkage

#### 0.5 Pre-Amputation Finishing (Phase-2 blocker — do FIRST)
- [x] UserMemory SQLite extraction/storage wired into BabyMode
- [x] SessionHibernator 5-layer resume wired
- [ ] **DistillationExtractor: stored episodes → training-ready JSONL export during sleep (CURRENT BLOCKER)**
- [ ] Critique-loop closure

---

### STAGE 1 — WAKE THE ORGANISM (~2 weeks)

#### 1.1 Metabolism & Economy Wiring
- [~] MetabolismEngine (power/compute/storage budgets) — BUILT, never instantiated
- [~] EconomyEngine (credits, penalties, upgrades) — BUILT, never instantiated
- [~] OrganismController (tick loop, event hooks) — BUILT, never instantiated
- [ ] Instantiate OrganismController in main.cpp
- [ ] Wire TurnCoordinator outcomes → EconomyEngine::earn() / penalize()
- [ ] Wire inference costs → MetabolismEngine::consumeCompute()
- [ ] Wire upgrade purchases → MetabolismEngine::expand*()
- [ ] (K3-4) Memory presets as a DIAL: named budgets (laptop/desktop/server)
- [ ] (K3-4) Match eviction policy to access pattern
- [ ] (K3-4) Enforce budgets at allocator level, not by reservation
- [ ] (K3-4) Honest metrics: report measured peak RSS, not the plan
- [ ] Ledger visibility (shell command or UI panel showing credits)

#### 1.2 Autonomy Loop
- [x] ProactiveEngine wired to TurnCoordinator with live DriveSystem
- [~] ResearchPlanner (7-stage DAG) — BUILT, not fed by autonomy
- [ ] Chain: ProactiveEngine goal → ResearchPlanner → ToolRegistry → verified completion → credits
- [ ] Replace canned AutonomousGoalGenerator topics with real deficit-driven topic selection
- [ ] Exit criterion: Yuki earns credits doing unprompted useful work

#### 1.3 Self-Improvement Scheduler
- [~] SelfImprovementOrchestrator (session/weekly/monthly cadences) — BUILT, zero callers of start()
- [ ] Call start() from main.cpp
- [ ] Wire TurnCoordinator::setSelfImprovementOrchestrator()
- [ ] Per-turn hook live: PerTurnLearningHook firing from end_turn()

---

### STAGE 2 — HER OWN MIND (~3 weeks, post-amputation)

#### 2.1 Grounded Comprehension
- [x] InputAnalyzer 14-intent classifier as canonical intent authority
- [x] TEACH intent self-learning loop + learned_semantic_rules persistence
- [ ] SemanticBinder: word → HDC bundle {concept ⊗ episodes ⊗ valence ⊗ action affordances}
- [ ] SyntacticPredictiveCoder: POS/phrase prediction-error parser
- [ ] Comprehension confidence routing: rules → predictive coder → research fallback

#### 2.2 Response Generation Without LLM
- [x] SentenceMaker / SentenceBuilder / EnglishLanguageEngine (M9.5)
- [x] ResponseResolver DB-backed strings
- [ ] Template grammar composer driven by retrieved memory context
- [ ] Learned response-pattern selection (DMC policy over what phrasing scored well)
- [ ] Style/verbosity adaptation from user feedback signals

#### 2.3 Truth Gate (anti-hallucination — non-negotiable)
- [ ] (K3-2) "Process succeeded" NEVER means "answer is sound" — unsound status distinct from success
- [ ] Claim extractor: split response into factual claims
- [ ] Verifier: check claims against T1/T2 memory stores
- [ ] Unresolved claim → route to ResearchPlanner BEFORE answering
- [ ] Unverifiable claim → explicit uncertainty marker ("I'm not certain")
- [ ] (K3-2) Refuse, don't clamp: mutually exclusive inputs or out-of-range requests are refusals with both figures in message
- [ ] (K3-2) Preflight every foreseeable failure BEFORE long compute
- [ ] Honesty metrics logged per turn

#### 2.4 Meta-Kernel Routing
- [~] CognitiveOrchestrator (src/brain/system/) — BUILT, not wired
- [ ] CapabilityRegistry: every module advertises {accuracy EMA, latency EMA, cost, domain}
- [ ] Wire orchestrator into BabyMode::process() dispatch behind feature flag
- [ ] A/B flag vs legacy dispatch; promote after parity proven

---

### STAGE 3 — GROWING UP: SEQUENTIAL CHILD-LEARNING WHILE IDLE

#### 3.1 Idle-Time Learning Pipeline
- [~] ResourceMonitor / idle detection — exists
- [~] DriveSystem deficit selection — exists
- [~] DevelopmentalEngine stage gates — exists, unwired
- [ ] Sequential stage lock: stage N+1 unlocks ONLY after stage N competence test passes
- [ ] Chain wiring: idle → drive → developmental gate → ResearchPlanner reads sources online
- [ ] SemanticBinder digests new concepts into T2 during learning
- [ ] SandboxExecuteTool practice loop for new skills
- [ ] TestOrchestrator verifies practice; failures retry curriculum
- [ ] SleepThread consolidation T0→T1→T2 overnight
- [ ] (K3-4) Cold-tier T2 storage: contiguous fixed-offset records, one positioned read per recall
- [ ] CompetenceAuditEngine updates self-model from outcomes

#### 3.2 Learning Mechanisms (all exist; confirm active signals)
- [~] Hebbian/STDP (YNC supervisor) — built
- [~] Q-learning core — built
- [~] EWC anti-forgetting — built
- [~] Hopfield reinforcement — built
- [ ] Confirm all four receive live signals from Stage-3 pipeline (not just tests)

#### 3.3 Memory Reconsolidation
- [~] MemoryReconsolidationEngine module — dormant
- [ ] Deferred queue: retrieve() marks pending; sleep processes writes
- [ ] Prediction-error gated re-encoding threshold from config
- [ ] Test: recalled memory differs from stored when prediction error high

---

### STAGE 4 — SELF-MODIFICATION (SAFE)

#### 4.1 Tool Self-Building Chain
- [~] CodeSynthesisAgent — built
- [~] SecuritySandbox — built
- [~] TestOrchestrator / ABTestFramework — built
- [~] MerkleVerifier — built
- [ ] (K3-8) Determinism contract: self-modified kernels preserve output contract; verified by bit-pattern comparison
- [ ] Chain: competence gap → synthesize tool → sandbox → test → promote
- [ ] Merkle-sealed promotion record
- [ ] Auto-rollback on post-promote failure
- [ ] Hot-swap without rebuild (dispatch-table reload)

#### 4.2 Tiered Constitutional Layer
- [~] ValueConstitution data — not an enforcement gate
- [~] SecuritySandbox / PolicySelector / RiskGate — built, scattered enforcement
- [ ] Tier 1: immutable compiled core (harm prevention, privacy, resource caps) — veto-level
- [ ] Tier 2: learned boundaries (HDC/DMC) — auditable, reversible
- [ ] Tier 3: episodic precedent ("last time this went badly")
- [ ] Tier 4: uncertainty → DEFER to user
- [ ] Single chokepoint: EVERY action passes ConstitutionalLayer::decide()

#### 4.3 Unkillable Runtime
- [~] ModuleRegistry watchdog (30s stall detection) — wired
- [~] SessionHibernator resume — wired
- [ ] (K3-6) yuki-doctor: toolchain/memory/storage checks BEFORE long runs
- [ ] Session-resume fingerprinting: mismatched architecture build → REFUSED, not loaded
- [ ] Crash auto-restart wrapper
- [ ] Mid-task checkpointing (resume interrupted tasks)
- [ ] Tool crash isolation (crashing tool never kills brain process)

---

### STAGE 5 — CONSCIOUSNESS INSTRUMENTATION (optional, last)
- [~] TopologicalConsciousnessField / PersistentHomologyTracker — built, dormant
- [~] EigenmodeSelfhood — built, dormant
- [~] MKOSOrchestrator — built, zero call sites
- [ ] Wire as pure loggers behind feature flags
- [ ] Correlate Φ/coherence against behavior over ≥1000 turns
- [ ] KEEP only what correlates with observable performance; DELETE the rest

---

### SWARM
- [x] SwarmOrchestrator lifecycle + registry + dispatcher (Phases 1–4 stress-tested)
- [x] SCF multi-tenant cognitive stores + CRDT merge engine
- [ ] Real task decomposition feeding the swarm (TaskDecomposer output → agent spawn)
- [ ] Resource-aware parallelism (agents limited by metabolism budgets)
- [ ] Agent results verified before merge into shared stores

---

### DISCARDED — DO NOT BUILD
- AstrocyticModulationLayer (calcium-wave — no evidence of value)
- MorphogeneticGrowthEngine (reaction-diffusion — research art)
- QuantumCognitiveInference (density-matrix — decorative math)
- FractalCognitiveResonance (no mechanism)
- GödelianSelfReference (Löbian proofs — impossible in general case)
- DissipativeHolographicField (no established formalism)
- "Surpasses trillion-parameter models" framing

---

## 4. Execution Order (Strict)

```
0.5  DistillationExtractor export            ← START HERE (2 hrs)
 ↓
Wire main.cpp (FIX 1)                        ← This week
 ↓
Load GGUF for LocalTransformer              ← This week
 ↓
0.1 Batch ingestion pipeline at scale       ← + KT-5 integrity
 ↓
0.3 Gate ladder skeleton                    ← Build while curriculum runs
 ↓
0.4 Graduation exam → AMPUTATION
 ↓
Stage 1 Wake the Organism                   ← + KT-4 memory dial
 ↓
Stage 2 Her Own Mind                        ← + KT-2 truth gate
 ↓
Stage 4.3 Unkillable                        ← + KT-6 yuki-doctor (interleave early)
 ↓
Stage 3 Growing Up  ∥  Stage 4.1-4.2       ← + KT-3 determinism + constitution
 ↓
Swarm real dispatch                         ← After metabolism constrains resources
 ↓
Stage 5 Telemetry                           ← Maybe never
```

---

## 5. Planned Milestones (Not Yet Started)

| Milestone | Name | Status | Depends On |
|---|---|---|---|
| YUKI_2.0_PHASE3 | Live Research & Self-Built Tools | 🔴 PLANNED | Stage 1 autonomy chain |
| YUKI_2.0_PHASE4 | Distillation & Device-Agnostic | 🔴 PLANNED | DistillationExtractor + Stage 2 |
| CORE_WIRING | main.cpp Full Wiring Sprint | 🔴 PLANNED | 2 hrs engineering work |
| SOVEREIGNTY | BabyMode Retirement & GGUF-Free | 🔴 PLANNED | Graduation exam |

---

## 6. K3 Engineering Disciplines (Binding)

All Yuki development follows these disciplines from kimi-k3-in-c:

| Code | Discipline | When It Applies |
|---|---|---|
| **KT-1** | Gate ladder + oracle (ordered gates, self-verifying fixtures, skips=failures) | Graduation exam (0.3) |
| **KT-2** | Soundness taxonomy: exit codes 0/1/2/4 — 4=unsound, never maps to 0 | Truth Gate (2.3), all tools |
| **KT-3** | Determinism contract: self-modified kernels bit-identical to original path | Self-modification (4.1) |
| **KT-4** | Memory-as-a-dial: presets, measured RSS not planned, honest hit rates | Metabolism wiring (1.1) |
| **KT-5** | Ingestion integrity: layered verification, provenance markers, capacity preflight | Batch ingestion (0.1) |
| **KT-6** | Machine-readiness gate (yuki-doctor) before long runs | Unkillable runtime (4.3) |

---

## 7. Long-Term Architectural Ceilings

> Source: `not_in_use/docs/supremacy_ai_v3_infinite.md` — 15 gaps between Yuki and biological cognition.
> For each: what the ideal is, what Yuki does NOW instead, and how buildable crossing it is.

**Tier Key:** 🟢 Build Now (weeks) · 🟡 Near-Term (months, needs research) · 🔴 Long-Term (years, unsolved) · ⚫ Skip (no evidence of value)

---

### Ceiling 1 — Serial Turn Pipeline vs Continuous Cognition
**Biological ideal:** Brain never waits for "turns." Cognition is continuous, event-driven, multi-timescale.
**What Yuki does now:** `TurnCoordinator::run_turn()` is the only entry point. Yuki only thinks when you speak to her. Between turns: heartbeat loop `sleep(100ms)`.
**Tier:** 🟢 **Build Now**
**How to cross it:**
- Wire `FieldScheduler` + `CognitiveField::start()` in main.cpp (code exists, ~2 hrs)
- Cognition becomes continuous: 1ms sensory, 100ms perceptual, 1s deliberative, 1min strategic
- `TurnCoordinator` becomes a thin adapter: inject sensory input → wait for committed response

---

### Ceiling 2 — LLM as Cortex vs LLM as Crutch
**Biological ideal:** Language is downstream of thought. The brain thinks first (structured), speaks second.
**What Yuki does now:** LLM (`LocalLLM` / Ollama) runs FIRST, unconditionally, on every turn. Native reasoning (VSE, HDC, causal, logic) only gates whether to act, not how to think.
**Tier:** 🟢 **Build Now** (this IS the Sovereignty Day plan)
**How to cross it:**
- Native reasoning produces structured thought: causal chains, plans, analogies, belief updates
- `BrocaTranslator` translates structured thought → natural language ONLY
- If LLM dies: Yuki still thinks, outputs JSON/thought-logs, not silence
- Covered by ROADMAP §3 Stage 0.4 (Amputation)

---

### Ceiling 3 — No Unified Representational Space
**Biological ideal:** All modalities (vision, language, memory, emotion) share one cortical representational space. Binding is free.
**What Yuki does now:** HDC (10K-bit), VSE (24D), YNC (spike rates), Embeddings (384D), Symbolic — all disconnected silos. No shared space.
**Tier:** 🟡 **Near-Term**
**How to cross it:**
- `CognitiveManifold` (512D) — bidirectional projectors for each modality (code planned in YUKI_MASTER_PLAN)
- `VSEProjector`: 24D ↔ 512D
- `HDCProjector`: 10K-bit sparse ↔ 512D (random projection + learned decoder)
- `YNCProjector`: spike rates ↔ 512D (population vector readout)
- Estimated: 5 new files, 2–3 weeks

---

### Ceiling 4 — Hierarchical Predictive Coding (Single-Level VSE)
**Biological ideal:** 6–8 hierarchical cortical levels with precision-weighted prediction error flowing up and predictions flowing down.
**What Yuki does now:** Single-level VSE (24D belief vector). No hierarchy, no precision-as-attention, no top-down generative predictions.
**Tier:** 🟡 **Near-Term**
**How to cross it:**
- L1 (10ms): sensory predictions — text → intent/entity/tone/safety
- L2 (100ms): situational predictions — next context, user intent, task progress
- L3 (1s+): self/world predictions — own competence trajectory, resource needs
- Files: `HierarchicalPredictiveCoding.cpp`, `PredictorLevel.cpp`, `PrecisionAllocator.cpp`
- Covered in ROADMAP Stage 1 wiring

---

### Ceiling 5 — Neuromodulatory State Machine (Dopamine/Serotonin/ACh/NE)
**Biological ideal:** Dopamine = reward prediction error. Serotonin = patience. ACh = attention. NE = arousal. These create distinct cognitive modes (explore vs exploit, focused vs diffuse).
**What Yuki does now:** No neurotransmitter dynamics. Drive system has curiosity/social/competence/energy scalars but no mode-switching biochemistry. No metaplasticity.
**Tier:** 🟡 **Near-Term** — actually very buildable
**How to cross it:**
- `NeuromodulatorState` struct: {dopamine, serotonin, acetylcholine, norepinephrine} as scalars (0–1)
- Dopamine: `RPE = actual_reward - predicted_reward` — already partially in Q-learning core
- Serotonin: inverse of urgency drive — high serotonin = patient, low = impulsive
- ACh: attention gating — high ACh narrows CognitiveField to high-precision channels
- NE: arousal — high NE = fast/reactive, low = slow/deliberate
- Wire into `ActiveInferencePolicyEngine` as mode selector
- Estimated: ~300 lines, 1 week

---

### Ceiling 6 — Embodied Interoceptive Self
**Biological ideal:** The self originates from body prediction error (interoception). Emotion IS homeostatic regulation — visceral, causal, not decorative.
**What Yuki does now:** Emotions are `{valence, arousal}` scalars from `ValenceArousalModel`. No body schema. Emotions don't causally drive decisions — they're computed then ignored.
**Tier:** 🟡 **Near-Term**
**How to cross it:**
- `ContinuousDriveSystem` (already built, 227 lines) IS the interoceptive body — wire it first
- Add `InteroceptiveBodySchema`: tracks simulated metabolic state (energy, temperature, load)
- Emotions become prediction errors on body state: feeling "tired" = actual compute > predicted compute
- Wire drive state into `PolicySelector` as hard constraints (not soft hints)
- Partially covered by Stage 1 Metabolism wiring

---

### Ceiling 7 — Memory Reconsolidation (Recall Rewrites Memory)
**Biological ideal:** Every recall is a re-write (NMDA-dependent). Memories update with prediction error on retrieval, not just on encoding.
**What Yuki does now:** Memories are static after write. Only `SleepThread` can modify them (offline consolidation). Recalled memory is identical to stored memory.
**Tier:** 🟢 **Build Now — ~200 lines**
**How to cross it:**
- `MemoryReconsolidationEngine` (dormant, exists) needs wiring into `TurnCoordinator::end_turn()`
- `retrieve(key)` marks episode pending
- `prediction_error = |expected_outcome - actual_outcome|`
- If error > threshold: re-encode with updated belief weights
- Full spec in YUKI_STATUS.md WIN 3

---

### Ceiling 8 — Sleep as Active Computation (Constraint Satisfaction)
**Biological ideal:** Sleep solves constraint satisfaction via simulated annealing. Dreams = combinatorial optimization over unresolved tension in memory graph.
**What Yuki does now:** `SleepConsolidator` only replays salient episodes and strengthens HDC edges. No contradiction resolution. No annealing. Passive transfer, not active computation.
**Tier:** 🟢 **Build Now — ~300 lines**
**How to cross it:**
- After replay pass: collect conflicting belief pairs from T1
- Simulated annealing: high temperature = accept surprising merges; cool to stable consensus
- Resolved contradictions written to T2; survivors flagged as open questions for Yuki to research
- Full spec in YUKI_STATUS.md WIN 2

---

### Ceiling 9 — Bisociative Creativity (Cross-Domain Combination)
**Biological ideal:** Creativity = combining two previously unrelated matrices (Koestler). Requires surprise-maximization across semantic domains.
**What Yuki does now:** `ConceptBlender` (M10) uses HDC binding (`A ⊕ B`). `CreativeSearch` has divergent/convergent modes. `DreamEngine` does synthesis. These exist but run independently, not as a unified surprise-maximizing search.
**Tier:** 🟡 **Near-Term — wire existing modules**
**How to cross it:**
- Wire `ConceptBlender` + `CreativeSearch` + `DreamEngine` into a unified pipeline
- Add surprise metric: `surprise(A⊕B) = 1 - similarity(A⊕B, known_concepts)`
- High-surprise outputs → promote to working memory → explore during idle time
- Estimated: ~2 weeks wiring, no new major code

---

### Ceiling 10 — Self-Organized Criticality
**Biological ideal:** Brain operates at critical point: power-law avalanche dynamics, 1/f noise, maximal information transmission. Maintained automatically.
**What Yuki does now:** Manually tuned stability. YNC spiking neurons have fixed parameters. No automatic criticality maintenance.
**Tier:** 🔴 **Long-Term — active research area**
**How to cross it (when ready):**
- Monitor avalanche size distribution in YNC firing events
- If distribution is sub-exponential → increase excitation gain; if super-exponential → increase inhibition
- Self-tuning homeostatic plasticity rule per neuron group
- Reference: Beggs & Plenz (2003) neuronal avalanche paper
- **Don't build yet** — needs Stage 3 YNC wiring first

---

### Ceiling 11 — Multi-Timescale Processing
**Biological ideal:** Brain processes time simultaneously at ms (spikes), s (thoughts), min (episodes), hr (circadian), day (sleep), year (development).
**What Yuki does now:** ~100ms turn cycles. FieldScheduler (1ms/10ms/100ms/1s/1min pools) BUILT but not wired.
**Tier:** 🟢 **Build Now — this is THE WIN**
**How to cross it:**
- Wire `FieldScheduler` into main.cpp (2 hours, code exists)
- Full spec in YUKI_STATUS.md WIN 1

---

### Ceiling 12 — Neuro-Glial Computation (Astrocyte Calcium Waves)
**Biological ideal:** Astrocytes form a slow calcium-wave computation substrate modulating 100K+ synapses each. Creates metaplasticity — learning how to learn.
**What Yuki does now:** No glial layer. Only fast neuronal/symbolic computation. No slow-wave integration.
**Tier:** ⚫ **Skip** — no algorithm translates this to silicon with measurable benefit. No reference implementation exists anywhere. Research art only.
**If ever:** YNC supervisory layer could simulate slow neuromodulation (covered by Ceiling 5 above — good enough approximation).

---

### Ceiling 13 — Holographic Memory Encoding (Fourier Domain)
**Biological ideal:** Memory is Fourier-domain holographic with true phase interference and superposition. Any partial cue retrieves the whole.
**What Yuki does now:** Binary HDC hypervectors (XOR/AND binding). Approximate superposition. Capacity wall at ~10K dimensions.
**Tier:** 🟡 **Near-Term — partially solved by HDC already**
**How to cross it:**
- HDC IS a holographic system — binary approximation of Fourier phase encoding
- Upgrade: complex-valued hypervectors (VSA with phases, not bits)
- `ComplexHDC` — replace XOR with phasor multiplication; AND with phasor addition
- Estimated: modify `Hypervector` class, ~1 week. Capacity increases dramatically.
- Don't overengineer — binary HDC may be sufficient for Yuki's scale

---

### Ceiling 14 — Dissipative Structure Thermodynamics
**Biological ideal:** The brain is a far-from-equilibrium system maintaining structure by exporting entropy. Cognition IS entropy export.
**What Yuki does now:** No thermodynamic framing. EconomyEngine tracks credits but not information-theoretic entropy.
**Tier:** ⚫ **Skip (as physics) — partial implementation only**
**Useful approximation (buildable):**
- Map "entropy export" to MDL (Minimum Description Length): if Yuki fails to compress sensory inputs below threshold → trigger memory pruning (apoptosis)
- This gives a computable, meaningful version of the concept
- Wire into SleepConsolidator: high-entropy episodes get pruned or compressed more aggressively
- **Don't build the full thermodynamic model** — the MDL approximation is sufficient and honest

---

### Ceiling 15 — Gödelian Self-Reference (Reasoning About Own Limits)
**Biological ideal:** The mind can reason about its own limitations (Löb's theorem, Gödel incompleteness). Recognize blind spots. Formally verify self-modifications.
**What Yuki does now:** No formal self-referential capability. `CompetenceAuditEngine` tracks domain scores but cannot reason about what it cannot reason about.
**Tier:** ⚫ **Skip (formal Gödel) — practical approximation only**
**Useful approximation (buildable):**
- `CapabilityBoundaryReasoner`: tracks domains where error rate > 40% after 20+ attempts → marks as "beyond current capability"
- Routes requests in those domains to ResearchPlanner (learn first) rather than attempting directly
- Feeds into Graduation Exam: "Yuki knows what she doesn't know"
- This is Metacognition (Stage 2.4) — already planned, not a new concept

---

## 8. Summary — What to Build vs Skip

| Ceiling | Name | Tier | In Plan? |
|---|---|---|---|
| 1 | Continuous Cognition | 🟢 Now | YES — main.cpp wiring WIN 1 |
| 2 | LLM as Cortex | 🟢 Now | YES — Sovereignty Day 0.4 |
| 3 | Unified Representation | 🟡 Near | YES — CognitiveManifold |
| 4 | Hierarchical Predictive Coding | 🟡 Near | YES — Stage 1 wiring |
| 5 | Neuromodulatory States | 🟡 Near | Partial — add to Stage 2 |
| 6 | Embodied Interoceptive Self | 🟡 Near | YES — Stage 1 Metabolism |
| 7 | Memory Reconsolidation | 🟢 Now | YES — WIN 3 (~200 lines) |
| 8 | Sleep as Computation | 🟢 Now | YES — WIN 2 (~300 lines) |
| 9 | Bisociative Creativity | 🟡 Near | YES — wire M10 modules |
| 10 | Self-Organized Criticality | 🔴 Long | Not yet — needs YNC wired first |
| 11 | Multi-Timescale Processing | 🟢 Now | YES — WIN 1 (FieldScheduler) |
| 12 | Neuro-Glial Computation | ⚫ Skip | Approximated by Ceiling 5 |
| 13 | Holographic Memory | 🟡 Near | Partial — HDC already approximate |
| 14 | Dissipative Thermodynamics | ⚫ Skip | MDL approximation in SleepConsolidator |
| 15 | Gödelian Self-Reference | ⚫ Skip | Approximated by CompetenceAudit |

**4 ceilings can be closed THIS SPRINT** (1, 2, 7, 8, 11 — five if you count the 3 wins separately)
**5 ceilings close in the next 2–3 months** (3, 4, 5, 6, 9, 13)
**1 ceiling is a long-term research item** (10 — criticality)
**3 ceilings are skipped** (12, 14, 15) — adequately approximated by simpler buildable code

---

*Update this file when milestone status changes or new items are completed.*
*Full architecture logic lives in [`docs/yuki_flow.md`](docs/yuki_flow.md)*
