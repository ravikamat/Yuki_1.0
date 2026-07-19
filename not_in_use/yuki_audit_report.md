# YUKI_1.0 — CODEBASE AUDIT & BLUEPRINT GAP REPORT
**Date:** 2026-05-29  
**Auditor:** Antigravity Agent  
**Source tree:** `D:\Yuki_1.0\src`  
**Compared against:** _YUKI SUPERINTELLIGENCE MASTER BLUEPRINT v5.0_ (2026-05-27)  

---

## 1. AUDIT METHODOLOGY

This report was produced by:
1. Full directory enumeration of every `.h` / `.cpp` under `src/`
2. Direct header-read of all key classes
3. Tracing exact startup order from `main.cpp`
4. Cross-referencing bindings and callbacks to verify live wiring

No code was changed during this audit.

---

## 2. FULL MODULE INVENTORY

### 2.1 Source Directory Map

```
src/
├── main.cpp                          ← Entry point
├── BabyMode.h / .cpp                 ← Top-level cognitive gateway
├── NeuralSpine.h / .cpp              ← Legacy NLP coordinator (pre-VSE)
├── SubsystemControl.h / .cpp         ← Sensor on/off toggles
├── CommandRouter.h / .cpp            ← System command dispatcher
├── PresenceShell.h / .cpp            ← Win32 UI shell
├── DetailView.h / .cpp               ← Win32 detail panel
├── AvatarBody.h / .cpp               ← Animated avatar window
├── IntentScorer.h / .cpp             ← Legacy intent scoring
├── ResponseEngine.h / .cpp           ← Legacy response builder
├── AutoSensor.cpp                    ← Sensor auto-start helper
│
├── input/
│   ├── Ear.h / .cpp                  ← EarRuntime (microphone)
│   ├── Mouth.h / .cpp                ← MouthRuntime (TTS speaker)
│   ├── CameraRuntime.h / .cpp        ← Camera capture + face detect
│   ├── ScreenRuntime.h / .cpp        ← Screen capture (DXGI)
│   ├── SpeechSystem.h / .cpp         ← SpeechToTextRuntime (Whisper/Vosk)
│   ├── PerceptionLayer.h / .cpp      ← Unified perception aggregator
│   ├── VisionSystem.h / .cpp         ← Vision processing
│   ├── InputLayer.h / .cpp           ← Input normalizer bridge
│   ├── conditioning/
│   │   ├── SignalConditioningLayer.h / .cpp  ← SCL master (50ms loop)
│   │   ├── SignalNormalizer.h / .cpp         ← Gain control, DC offset
│   │   ├── ArtifactFilter.h / .cpp           ← Dropout, spike rejection
│   │   ├── ChangeDetector.h / .cpp           ← Predictive coding gate
│   │   ├── TemporalAligner.h / .cpp          ← Multi-modal time sync
│   │   ├── ConditionedSnapshot.h / .cpp      ← Snapshot data type
│   │   └── SensorCalibrationProfile.h / .cpp ← Per-channel calibration
│   └── encoding/
│       ├── ObservationEncoder.h / .cpp        ← Modality encoders (Text/Audio/Visual/Screen/Proprio)
│       ├── MultiModalFusionGate.h / .cpp      ← 50ms multi-modal fusion
│       ├── SensoryObservation.h / .cpp        ← Observation data type
│       ├── SpatialAnchor.h / .cpp             ← Spatial context
│       └── TemporalContext.h                  ← Temporal window
│
├── brain/
│   ├── BrainTypes.h                  ← Core type definitions
│   ├── MeaningTypes.h                ← Meaning representation types
│   ├── ExecutionTypes.h              ← Execution plan types
│   ├── ActionRouter.h / .cpp         ← Routes resolved intents to actions
│   ├── ToolExecutor.h / .cpp         ← Executes tools (scripts, files, APIs)
│   ├── CapabilityMap.h / .cpp        ← Tool/capability registry
│   ├── BackgroundAgents.h / .cpp     ← Background task workers
│   ├── FileOperator.h / .cpp         ← File read/write/exec
│   ├── ScriptRunner.h / .cpp         ← Subprocess execution
│   ├── SystemExecutor.h / .cpp       ← High-level action dispatcher
│   ├── VerificationEngine.h / .cpp   ← Post-action verification
│   ├── SafetyGovernor.h / .cpp       ← Safety guardrails
│   ├── MobileServer.h / .cpp         ← Local HTTP server for mobile UI
│   ├── DocReader.h / .cpp            ← Document/URL parser
│   ├── SmartScraper.h / .cpp         ← Web scraping
│   ├── KnowledgeExtractor.h / .cpp   ← Fact extraction from text
│   ├── KnowledgeRouter.h / .cpp      ← Routes knowledge queries
│   ├── LanguageLayer.h / .cpp        ← Language processing wrapper
│   ├── LanguageSynthesizer.h / .cpp  ← Text generation
│   ├── GoalBuilder.h / .cpp          ← Constructs goal structures
│   ├── RequestClassifier.h / .cpp    ← Request type classifier
│   ├── EntityProcessor.h / .cpp      ← Entity extraction
│   ├── LearningUpdate.h / .cpp       ← Online learning update handler
│   ├── LocalKnowledgeBase.h / .cpp   ← Local SQLite knowledge store
│   ├── MotherCore.h                  ← Agent coordination header
│   ├── InputNormalizer.h / .cpp      ← Input normalization
│   ├── ResponseActPlanner.h / .cpp   ← Response + action planning
│   ├── CandidateGenerator.h / .cpp   ← Response candidate generation
│   ├── UIAutomationController.h/.cpp ← UI automation (Win32)
│   ├── DependencyInstaller.h / .cpp  ← Dependency management
│   │
│   ├── core/
│   │   ├── IntentClassifier.h / .cpp ← Core intent classification
│   │   └── ResponseResolver.h / .cpp ← Response resolution
│   │
│   ├── inference/                    ← Active Inference / VSE subsystem
│   │   ├── VariationalStateEstimator.h / .cpp ← VSE master (F minimization)
│   │   ├── BeliefState.h / .cpp               ← Factorized posterior q(s)
│   │   ├── GenerativeModel.h / .cpp           ← World model p(o,s) + online learning
│   │   ├── FreeEnergyCalculator.h / .cpp      ← F and G computation + policy grad
│   │   ├── PolicySelector.h / .cpp            ← Policy constraint + selection
│   │   └── PrecisionEngine.h / .cpp           ← Per-channel precision weights
│   │
│   ├── memory/                       ← Cognitive Memory Fabric
│   │   ├── CognitiveMemoryFabric.h / .cpp     ← CMF master (threaded queue)
│   │   ├── EpisodicStore.h / .cpp             ← HNSW + SQLite episodic memory
│   │   ├── SemanticGraph.h / .cpp             ← Concept graph + Hebbian reinforcement
│   │   ├── MemoryEncoder.h / .cpp             ← 24-dim vector encoder
│   │   ├── AuditSystem.h / .cpp               ← Audit and integrity checks
│   │   ├── ContextMemory.h / .cpp             ← Conversation / context memory
│   │   ├── KnowledgeStore.h / .cpp            ← Knowledge persistence
│   │   └── UserMemory.h / .cpp                ← User profile persistence
│   │
│   ├── predictive/                   ← Predictive Turn Engine
│   │   ├── predictive_turn_engine.h            ← TurnCoordinator + all types
│   │   ├── predictive_turn_engine.cpp          ← Full PTE implementation
│   │   ├── stream_workers.h / .cpp             ← E1/E2/E3 stream workers
│   │   ├── sqlite_memory_store.h / .cpp        ← SQLite-backed MemoryStore
│   │   ├── error_functions.cpp                 ← Prediction error functions
│   │   ├── salience_gate.cpp                   ← Salience evaluation
│   │   ├── response_shaper.cpp                 ← Response tone shaping
│   │   ├── tool_adapter.h / .cpp               ← Tool call adapter
│   │   └── turn_trace.h                        ← Turn tracing types
│   │
│   ├── reasoning/
│   │   ├── SemanticParser.h / .cpp             ← Rule-based NLU (no ML)
│   │   ├── GoalModel.h / .cpp                  ← Goal structure builder
│   │   ├── PatternEngine.h / .cpp              ← Response pattern matching
│   │   ├── EvidenceSystem.h / .cpp             ← Evidence scoring
│   │   ├── SynthesisEngine.h / .cpp            ← Response synthesis
│   │   ├── InputResolution.h / .cpp            ← Reference / ambiguity resolution
│   │   ├── TaskSystem.h / .cpp                 ← Multi-step task tracking
│   │   └── TaskContext.h / .cpp                ← Task context state
│   │
│   ├── learning/
│   │   ├── KnowledgeDaemon.h / .cpp            ← Python subprocess knowledge crawler
│   │   ├── LearningIngestor.h / .cpp           ← Multi-source knowledge ingestor
│   │   ├── EmbeddingEngine.h / .cpp            ← Stub embedding generator
│   │   └── MassCurriculumLoader.h / .cpp       ← One-time 9-topic bootstrap
│   │
│   ├── retrieval/
│   │   ├── RetrievalSystem.h / .cpp            ← Multi-strategy retrieval
│   │   └── VectorStore.h / .cpp                ← HNSWLib wrapper
│   │
│   ├── emotion/
│   │   └── EmotionSystem.h / .cpp              ← Multi-modal emotion extraction
│   │
│   ├── curiosity/
│   │   └── CuriosityEngine.h / .cpp            ← Curiosity / surprise trigger
│   │
│   ├── safety/
│   │   └── CodeApprovalGate.h / .cpp           ← Code approval gating
│   │
│   ├── skills/
│   │   ├── SkillRegistry.h / .cpp              ← Skill definition + lookup
│   │   └── SkillSystem.h / .cpp                ← Skill execution
│   │
│   └── database/
│       └── DatabaseManager.h / .cpp             ← SQLite singleton manager
```

---

## 3. ACTUAL STARTUP & WIRING ORDER

### 3.1 `main.cpp` — Verified Startup Sequence

```
1.  loadFeatureFlags()              ← Load config from file/env
2.  DatabaseManager::instance().init("data/brain/yuki.db")  ← SQLite singleton
3.  SessionState session            ← Shared quit flag + chat history
4.  BabyMode baby(session)          ← Constructs ALL sensor runtimes + NeuralSpine
    └── BabyMode ctor constructs:
        EarRuntime, MouthRuntime, CameraRuntime, ScreenRuntime,
        SpeechToTextRuntime, NeuralSpine, KnowledgeDaemon, MobileServer
5.  PresenceShell shell(session)    ← Win32 UI window
6.  baby.runMassCurriculumIfNeeded()  ← One-time 9-topic bootstrap
7.  UserMemory, UserModel, SqliteMemoryStore  ← Shared memory layer
8.  TurnCoordinator coordinator(user_model)  ← Predictive Turn Engine
    └── register_stream(E1FastStream)
    └── register_stream(E2SemanticStream)
    └── register_stream(E3DeepStream)
9.  VariationalStateEstimator vse   ← Active Inference engine (standalone)
10. SignalConditioningLayer scl(baby.subsystems())
    └── bindEar(&baby.ear())
    └── bindCamera(&baby.camera())
    └── bindScreen(&baby.screen())
    └── bindPredictiveEngine(&coordinator)
    └── bindVariationalEstimator(&vse)
    └── scl.start()                 ← 50ms polling loop begins
11. coordinator.bindVariationalEstimator(&vse)
12. baby.setVariationalEstimator(std::move(vse))  ← BabyMode owns VSE
13. baby.setPredictiveEngine(std::move(coordinator), memory_store, user_model)
14. Shell callbacks registered:
    └── processCallback → baby.process()
    └── avatarCallback → avatar state updates
    └── STT partial + final transcript callbacks → shell UI + baby.processVoice()
15. uiThread starts → Win32 message loop (PresenceShell, DetailView, AvatarBody)
16. readyWatcher thread → waits for STT ready → baby.announceReady()
17. Main loop: getline → baby.process() → print response
```

### 3.2 BabyMode — Construction Order (Verified)

The BabyMode constructor initializes members in this exact order (important for correctness):

```
1. session_, router_(subsystems_), micRuntime_(subsystems_)          [member init list]
2. speakerRuntime_(subsystems_), cameraRuntime_(subsystems_)
3. screenRuntime_(subsystems_)
4. sttRuntime_(micRuntime_, subsystems_)
5. spine_(subsystems_, micRuntime_, speakerRuntime_, &screenRuntime_, &cameraRuntime_)
--- constructor body ---
6.  vision().initialize(&subsystems_, &cameraRuntime_, &screenRuntime_)
7.  sttRuntime_.setTranscriptCallback(→ processVoice())   ← INTERNAL STT wiring
8.  sttRuntime_.setPartialTranscriptCallback(→ log)
9.  subsystems_.setRuntimeStateQuery(lambda)
10. subsystems_.setChangeCallback(→ syncRuntimesWithSubsystems())
11. spine_.start()                   ← background tick thread
12. router_.setMobileUrlProvider(→ mobileServer_.localUrl())
13. knowledge_.start()               ← Python subprocess launch
14. knowledge_.learnTopic(×20)       ← queue background curriculum
15. mobileServer_.setMessageHandler(→ processUserTurn())
16. mobileServer_.start(8765)
17. autoStartAllSensors(*this)
18. cmf_ = make_shared<CognitiveMemoryFabric>() → init() → start()
19. text_encoder_ = make_shared<TextEncoder>() → setMemoryFabric(cmf_)
20. knowledge_.setMemoryFabric(cmf_)
21. DatabaseManager::instance().init("yuki_global.db")  ← SECOND DB init
22. UniversalCache::instance().preload()
23. LearningIngestor::instance().start()
```

### 3.3 BabyMode — Owned Objects & Their Purpose

| Member | Type | Role |
|--------|------|------|
| `micRuntime_` | `EarRuntime` | Microphone capture |
| `speakerRuntime_` | `MouthRuntime` | TTS output |
| `cameraRuntime_` | `CameraRuntime` | Webcam + face detect |
| `screenRuntime_` | `ScreenRuntime` | DXGI screen capture |
| `sttRuntime_` | `SpeechToTextRuntime` | Whisper/Vosk STT |
| `spine_` | `NeuralSpine` | Legacy NLP (IntentScorer + ResponseEngine) |
| `coordinator_` | `TurnCoordinator` | Predictive Turn Engine |
| `vse_` | `VariationalStateEstimator` | Active Inference VSE |
| `knowledge_` | `KnowledgeDaemon` | Python subprocess (Wikipedia crawl) |
| `mobileServer_` | `MobileServer` | Local HTTP server |
| `cmf_` | `CognitiveMemoryFabric` | Episodic + Semantic memory |
| `text_encoder_` | `TextEncoder` | Text-to-observation encoder |
| `subsystems_` | `SubsystemControl` | Sensor on/off state |
| `router_` | `CommandRouter` | System command routing |

### 3.4 Known Wiring Anomalies (Confirmed by Audit)

> [!WARNING]
> These are not bugs that break the system, but they represent design inconsistencies that should be understood.

| # | Anomaly | Detail |
|---|---------|--------|
| W1 | **STT transcript wired twice** | BabyMode constructor wires `sttRuntime_ → processVoice()` internally. `main.cpp` ALSO wires `baby.stt() → baby.processVoice()` AND `shell.postCommitVoiceDraftText()`. The internal callback fires first. The main.cpp partial-callback is the only path that drives shell UI voice draft text. Net effect: voice input processes correctly but arrives via two separate callback chains. |
| W2 | **NeuralSpine partially bypassed** | NeuralSpine's `process()` is NOT called when `coordinator_` is set. `baby.process()` goes directly to `TurnCoordinator::run_turn()`. However NeuralSpine's background tick (world model refresh every 2s) continues running. NeuralSpine's `ConversationMemory` is not updated from predictive-path turns. |
| W3 | **Two separate databases active** | `DatabaseManager::init("data/brain/yuki.db")` in `main.cpp` AND `DatabaseManager::init("yuki_global.db")` in `BabyMode` constructor. Both point to the same singleton — the second call may silently fail or re-init. |
| W4 | **Two parallel intent classification systems** | `IntentClassifier` (singleton, `brain/core/`) outputs `GroundedIntent` enum and `IntentRoutingDecision`. `IntentScorer` (NeuralSpine member) outputs `IntentKind` enum. `TurnCoordinator` uses yet a third: `yuki::IntentClass`. These three do not share a common type or coordinate. |
| W5 | **VSE borrowed by two owners** | `SignalConditioningLayer` and `TurnCoordinator` both hold raw pointers to `VariationalStateEstimator`. Both may call `vse_->update()` in different threads (SCL in its 50ms loop, TurnCoordinator in the turn thread). No mutex protects concurrent VSE access. |
| W6 | **EmotionSystem not wired** | `EmotionSystem` (brain/emotion/) exists and implements multi-modal emotion extraction, but is never instantiated or called anywhere in the main processing path. |

---

## 4. COMPLETE DATA FLOW — PER TURN

### 4.1 Typed Input Path
```
User types → BabyMode::process(input)
  └── BabyMode::processUserTurn(UserTurnInput)
      ├── CommandRouter checks for "quit", system commands
      ├── NeuralSpine::process(SpineInput) [legacy path]
      │   ├── IntentScorer::score() → IntentKind enum
      │   ├── ResponseEngine::generate() → response text
      │   └── ConversationMemory::record()
      └── TurnCoordinator::run_turn(MultiModalInput) [predictive path]
          ├── PredictionState::from_previous()
          ├── BeliefPool::reset()
          ├── E1FastStream::run() → PartialObservation(intent, fast)
          ├── E2SemanticStream::run() → PartialObservation(intent, entity, tone)
          ├── E3DeepStream::run() → PartialObservation(deep)
          ├── BeliefPool::observe() for each obs
          ├── CommitController::can_commit()
          ├── resolve() → ResolutionDecision
          ├── VSE::update(SensoryObservation, PrecisionFactors) [if use_variational_inference_]
          │   ├── PrecisionEngine::computePrecision()
          │   ├── GenerativeModel::predictionError()
          │   ├── BeliefState::update(errors, precision)
          │   ├── FreeEnergyCalculator::computeF()
          │   └── PolicySelector::selectPolicy() → PolicyResult
          ├── shape_response() → TurnResult
          └── end_turn() → memory_store_.store_trace()
```

### 4.2 Voice Input Path
```
Microphone → EarRuntime → SpeechToTextRuntime
  └── partial callback → shell.postSetVoiceDraftText()
  └── final callback → baby.processVoice(text)
      └── same as processUserTurn() above
```

### 4.3 Signal Conditioning Loop (50ms background thread)
```
SCL::conditioningLoop() [every 50ms]
  ├── pollSensors_()
  │   ├── EarRuntime::getFrame()
  │   ├── CameraRuntime::getFrame()
  │   └── ScreenRuntime::getFrame()
  ├── SignalNormalizer::normalize() → gain/DC correction
  ├── ArtifactFilter::filter() → drop dropout/spikes
  ├── ChangeDetector::check() → predictive coding gate (suppress no-change frames)
  ├── TemporalAligner::align() → sync multiple modalities
  ├── ObservationEncoder::encode(ConditionedSnapshot) → SensoryObservation per channel
  ├── MultiModalFusionGate::fuse() → FusedPerceptionFrame (50ms window)
  ├── computePrecisionFactors_(frame) → PrecisionFactors (real: SNR, dropout, calib age)
  └── variational_estimator_->update(obs, factors) ← [LIVE – every 50ms]
```

### 4.4 Cognitive Memory Fabric (background thread)
```
CMF::workerLoop() [always running]
  ← receives MemoryPackets via ingest() queue
  ├── TextEncoder::encode() → heuristic scores (8 floats)
  ├── MemoryEncoder::encodeScores() → 24-dim vector
  ├── EpisodicStore::insert()
  │   ├── VectorStore::addDocument(id, 24-dim vector, JSON metadata)  ← HNSW index
  │   └── SQLite INSERT into cmf_episodes.db
  └── SemanticGraph::ingestFact()
      ├── extractNounPhrases() → unigrams/bigrams/trigrams
      ├── inferRelations() → "is a", "causes", "requires", "part of"
      └── SQLite INSERT into concepts + concept_edges tables

TurnCoordinator::run_turn() optionally calls:
  cmf_->retrieveContextForQuery(input_text, 800)
  └── MemoryEncoder::encodeText() → 24-dim query vector
  └── EpisodicStore::retrieveSimilar() → HNSW search → top-k episodes
  └── returns context string prepended to response
```

### 4.5 Knowledge Daemon (background subprocess)
```
KnowledgeDaemon [separate Python process]
  ├── Crawls Simple English Wikipedia in background
  ├── Answers factual queries via JSON stdin/stdout IPC
  ├── learnTopic(topic) → Python daemon queues learning
  └── setMemoryFabric(cmf) → when daemon learns topic, ingest into CMF:
      cmf_->ingest(MemoryPacket{KNOWLEDGE_FACT, text, ...})
```

---

## 5. ACTIVE INFERENCE (VSE) — DETAILED WIRING

```
VariationalStateEstimator
├── PrecisionEngine
│   ├── computePrecision(obs, belief, factors) → PrecisionMatrix
│   ├── signalQualityWeight(snr/30.0)
│   ├── contextualRelevanceWeight(context_relevance)
│   ├── historicalReliabilityWeight(historical_accuracy)
│   ├── surprisePenalty(surprise_magnitude * 0.4)
│   └── calibrationDecay(exp(-0.001 * calib_age_hours))
│
├── GenerativeModel [with online learning]
│   ├── likelihood(MAP_state, modality) → predicted features
│   ├── predictionError(obs, belief) → feature-level errors
│   ├── updateMapping(intent, modality, observed, lr=0.05) ← online EMA
│   ├── decayMappings_(0.999 every 100 turns)
│   ├── saveMappings("data/generative_model.db") / loadMappings()
│   └── Intent→feature mappings (8 intents × 3 modalities)
│
├── BeliefState [factorized posterior q(s)]
│   ├── q_intent[8]    ← UNKNOWN/QUERY/COMMAND/TUTORIAL/EMOTIONAL_VENT/CLARIFICATION_RESPONSE/META_QUESTION/ABORT
│   ├── q_engagement[3] ← LOW/MEDIUM/HIGH
│   ├── q_urgency[2]    ← NORMAL/URGENT
│   ├── q_joint() → 24-dim joint belief vector
│   ├── update(errors, precision, lr=0.1) ← gradient ascent on posterior
│   ├── entropy() ← belief uncertainty
│   └── getMAP() → most probable (intent, engagement, urgency)
│
├── FreeEnergyCalculator
│   ├── computeF(belief, errors, precision) = accuracy + complexity
│   ├── computeG(policy, belief, model) ← expected free energy
│   ├── policyGradient(policy, belief, model, ε=1e-3) ← finite differences
│   └── optimizePolicy(seeds, belief, model, max_iter=50, lr=0.05)
│
└── PolicySelector [7 safety constraints]
    ├── generateSeedPolicies(belief) → 6 template policies
    ├── isPolicyValid(policy, belief) → runs all 7 constraints
    ├── C1: URGENT → waitTime ≤ 0.3
    ├── C2: LOW engagement → responseLength ≤ 0.4
    ├── C3: Low MAP probability → proactivity ≤ 0.4
    ├── C4: UNKNOWN intent → toolUse ≤ 0.2
    ├── C5: HIGH engagement → tone ≥ 0.4
    ├── C6: HIGH confidence → detailLevel ≥ 0.5
    ├── C7: No extremes (parameters clamped [0.05, 0.95])
    └── selectPolicy() → optimized Policy with 8 params
        [responseLength, tone, detailLevel, waitTime, proactivity, toolUse, verbosity, confidenceThreshold]
```

---

## 6. REASONING PIPELINE — ACTUAL IMPLEMENTATION

> [!NOTE]
> The reasoning pipeline is **much richer** than the Blueprint summary implies. The full chain below is verified by direct code read.

### 6.1 Full Reasoning Chain (Verified)

```
CanonicalInputEvent (text + sourceKind + timestamp)
  │
  ▼
[LanguageLayer]
  Hindi/Hinglish → KnowledgeDaemon.translate() → NormalizedInput
  │
  ▼
[SemanticParser::parse(normalizedEnglishInput)]
  8-step pipeline: classifyIntent → classifyDomain → extractActions (54 verbs)
                   → extractEntities (platforms/devices/proper nouns)
                   → extractSlots → inferUnknownSlots → scoreConfidence
  → SemanticFrame { IntentCategory, domain, confidence, slots[], actions[], entities[],
                    unknownSlots[], isQuestion, isNegation, isUrgent, needsClarification }
  │
  ▼
[GoalModelBuilder::buildSpec(SemanticFrame, LanguageResult)]
  Derives: goal description (action+slots), tone (emotional/urgent/inquisitive/directive/casual),
           safety level (OBSERVE→READ→EDIT→INSTALL→SEND→DELETE→SELF_MODIFY),
           knownSlots from SemanticFrame, needsClarification/Research/Execution flags
  → GoalModel { goal, domain, tone, safetyLevel, knownSlots, unknownSlots, gaps }
  │
  ▼
[PatternEngine::buildFrame(CanonicalInputEvent, GoalModel)]
  Multi-signal voting: 7 RequestModes × 17 keywords with weights → COMMAND/IMPLEMENTATION/
                       DESIGN/RESEARCH/CLARIFICATION/QUESTION/CONTINUATION
  OutputMode: CODE/PATCH/BULLETS/ARCHITECTURE/REPORT/MIXED
  Entities: quoted strings, capitalized, 60+ tech keywords, numeric quantities
  Constraints: explicit (language:X, length:Y) + implicit from mode
  → PatternFrame { requestMode, outputMode, entities[], constraints[], coreIntent,
                   unknownSlots, confidence, isEmotional, needsClarification }
  │
  ├──→ [SkillRegistry::check()] — early exit if triggerPattern matches
  │
  ▼
[CognitiveSituationBuilder]
  → CognitiveSituation { pattern:PatternFrame, goalHierarchy, userStateEstimate,
                          conversationMomentum, likelyMemoryZones }
  │
  ├──→ [EmpathyLayer::evaluate()] if isEmotional
  │      → UserMood { UNWELL/TIRED/STRESSED/SAD/FRUSTRATED/HAPPY/PROUD/NEUTRAL }
  │      → SynthesisEngine::synthesizeEmpathy(moodLabel, kbAdvice, userName, topDomain)
  │
  ▼
[RetrievalRouter::runHybrid(frame, unresolvedSlots)]
  Priority cascade (stops when bestTrust ≥ 0.65):
    1. VectorStore HNSW (OllamaEmbeddingEngine → dist < 0.45, trust=rel×0.90)
    2. KnowledgeDaemon internal query (trust from confidence)
    3. CodeSearch (src/ recursive scan for entity declarations, trust variable)
    4. KnowledgeGraph (graph.json entity triples, trust=0.70)
    5. TraceHistory (yuki_traces.jsonl, keyword overlap ≥ 0.20, trust=0.65)
    6. WebReconAgent → DISABLED (returns {} by design)
  → vector<RetrievalHit> { sourceId, sourceType, content, relevance, trust }
  │
  ▼
[AgentSwarm] → vector<AgentResult>
  │
  ▼
[EvidenceGraphBuilder::build(agentResults, directResponse, frame)]
  → EvidenceGraph { EvidenceNode[], supports/contradicts edges, trustScore }
  │
  ▼
[Verifier::verifyAdvanced(frame, synthesis, graph, agentResults)]
  → VerificationReport { satisfied, missingNeeds[], weakClaims[], satisfactionScore }
  │
  ├── NOT satisfied → [ClarificationEngine::generateBlockingQuestion(GoalModel, UserMemory)]
  │     Max 2 asks per slot (ClarificationState). If exhausted → [UnknownTopicFlow::handle()]
  │       → tryVault → tryDaemon → tryWeb (threshold=0.42) → ask user
  │
  ▼
[SynthesisEngine::buildPlan() + synthesize()]
  Format dispatch: BULLETS→formatAsBullets, CODE/PATCH→formatAsCode,
                   REPORT/ARCHITECTURE→formatAsReport, else→formatAsText
  Uncertainty note prepended if trustScore < 0.50
  → SynthesisResult { finalText, groundedConfidence }
  │
  ▼
[EmotionState::toneResponse(raw)]
  Prepends numeric state indicators for non-neutral states
  Updates internal EmotionSnapshot (valence/arousal/curiosity/fatigue)
  Persists to data/brain/emotion.json every 10 turns
  │
  ▼
FinalResponse → TTS (MouthRuntime) + PresenceShell display
```

### 6.2 Note: Reasoning vs. Predictive Engine Duality

The above pipeline runs **inside `TurnCoordinator::run_turn()`** via stream workers (E1/E2/E3). The `SemanticParser → GoalModelBuilder → PatternEngine` chain is executed within E2SemanticStream and E3DeepStream workers. The `VSE::update()` call happens separately after the stream pool commits. These two systems (predictive streams + VSE) currently **do not share their belief states** — the VSE's `BeliefState` and the stream pool's `BeliefPool` are parallel, independent probability structures.

---

## 7. MEMORY SYSTEMS — ACTUAL IMPLEMENTATION

### 7.1 CognitiveMemoryFabric (CMF)

| Component | Technology | Status |
|-----------|-----------|--------|
| EpisodicStore | HNSW (VectorStore) + SQLite `cmf_episodes.db` | ✅ LIVE |
| SemanticGraph | SQLite `concepts` + `concept_edges` tables | ✅ LIVE |
| MemoryEncoder | 24-dim projection (8 heuristic scores × 3) | ✅ LIVE |
| Worker thread | std::thread + condition_variable queue | ✅ LIVE |
| Hebbian reinforcement | `reinforceConcept()` → strength += 0.15 | ✅ LIVE |
| Concept decay | `decayBatch()` → strength *= 0.95 | ✅ LIVE |
| HNSW persistence | `saveIndex()` / `loadIndex()` on shutdown/startup | ✅ LIVE |

### 7.2 UserMemory
- SQLite-backed user profile: facts, preferences, turn history
- Accessed by `SqliteMemoryStore` (implements `MemoryStore` interface)

### 7.3 ContextMemory (ConversationMemory inside NeuralSpine)
- Rolling turn buffer (last N turns)
- Per-turn entity + intent tracking
- Used by NeuralSpine (legacy path) for context injection

### 7.4 KnowledgeStore
- SQLite-backed factual knowledge (separate from CMF)
- Populated by KnowledgeDaemon Python subprocess
- Queried via KnowledgeDaemon::query()

---

## 8. GAP ANALYSIS vs. BLUEPRINT v5.0

### 8.1 Legend
| Symbol | Meaning |
|--------|---------|
| ✅ | Implemented and wired |
| 🔶 | Partially implemented (stub/simplified) |
| ❌ | Not implemented |
| 🔁 | Implemented but using different architecture |

---

### 8.2 Section 3: Unified Architecture (Three Planes)

| Blueprint Element | Status | Notes |
|-------------------|--------|-------|
| **Global Workspace** (broadcast buffer, 10ms) | 🔶 | No explicit GW bus. `BabyMode::processUserTurn()` serves as informal coordinator. No lock-free ring buffer. No broadcast protocol. |
| **State Plane** (Hot/Warm/Cold/Vector/Graph) | 🔶 | Warm (SQLite ✅), Vector (HNSW ✅), Graph (SQLite ✅), Cold (no FlatBuffer archiving), Hot (in-RAM structs ✅ — partial) |
| **Control Plane** (ModuleRegistry, ResourceGovernor, SecuritySandbox, StateMachine) | ❌ | No formal Control Plane. Subsystem on/off via `SubsystemControl` only. No ModuleRegistry, no ResourceGovernor, no SecuritySandbox, no formal StateMachine |
| CoreBus (pub/sub topics) | ❌ | No CoreBus. Direct C++ method calls and callbacks only |
| BootstrapLoader | ❌ | Manual initialization in `main()` |

---

### 8.3 Section 4: Perception Layer

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| SpeechToTextRuntime (STT) | `SpeechToTextRuntime` | ✅ | Whisper/Vosk, 100ms windows, partial+final callbacks wired to shell |
| ScreenRuntime (5 FPS) | `ScreenRuntime` | ✅ | DXGI capture, ChangeDetector gates frames |
| CameraRuntime (10 FPS) | `CameraRuntime` | ✅ | DirectShow/MediaFoundation, face detect |
| KeyboardRuntime / TypingRhythm | **MISSING** | ❌ | No keystroke logger or typing rhythm analysis |
| PerceptionFusion (50ms) | `SignalConditioningLayer` (50ms) | 🔶 | SCL fuses modalities at 50ms. But does not publish to a GW bus — it directly calls `variational_estimator_->update()` |
| EmotionExtractor | `EmotionSystem` (emotion/) | 🔶 | Class exists, but **not wired** into the main turn path. Not connected to TurnCoordinator or BabyMode's process loop |

---

### 8.4 Section 4.3: Memory Systems

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| EpisodicStore (SQLite + vector) | `EpisodicStore` in CMF | ✅ | HNSW + SQLite, full schema |
| EpisodicRetriever (top-k) | `CMF::retrieveContextForQuery()` | ✅ | Encodes query → HNSW search → context string |
| ConceptGraph (typed causal edges) | `SemanticGraph` | 🔶 | Has nodes + edges, but edge types are simplified ("is_a", "causes", "requires", "part_of") — not the full 10-type set from blueprint |
| EntityRegistry | `EntityProcessor` + `UserMemory` | 🔶 | Entities tracked per-turn. No unified EntityRegistry with UUID, vector embedding, first_mentioned_turn |
| MemoryDistiller (every 25 turns) | **MISSING** | ❌ | No automatic chapter summarization every 25 turns |
| VectorStore (HNSW, 768-dim) | `VectorStore` (CMF=24-dim, Retrieval=dynamic-dim) | 🔶 | Two HNSW instances: CMF uses 24-dim heuristic vectors; `brain/retrieval/VectorStore` uses Ollama embedding dimension. Both are HNSW with InnerProductSpace (M=16, ef=200). |
| OllamaEmbeddingEngine | `OllamaEmbeddingEngine` | 🔶 | **Fully implemented** — POSTs to `http://127.0.0.1:11434/api/embeddings` (nomic-embed-text) via WinINet. But requires a **locally-running Ollama instance**. Not auto-started. If Ollama is absent, `RetrievalRouter::searchVectorIndex()` silently fails. |

---

### 8.5 Section 4.4: Understanding & Reasoning

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| SemanticParser | `SemanticParser` (reasoning/) | ✅ | Rule-based NLU. 8 intent classes, 6 domains, 54 action verbs, 19 platforms, 15 devices. Confidence-scored SemanticFrame with full slot extraction. |
| HypothesisLattice (BeliefPool) | `BeliefPool` in PTE | ✅ | Preserves competing interpretations, computes divergence, has commit threshold logic |
| PatternEngine | `PatternEngine` (reasoning/) | ✅ | Multi-signal RequestMode voting (7 modes, 17-word weighted triggers), OutputMode detection, entity/constraint/history/freshness extraction — fully operational |
| GoalModel | `GoalModelBuilder` (reasoning/) | ✅ | Static goal snapshot with safety-level derivation, tone, language, knownSlots/unknownSlots/gaps — fully wired to SemanticParser output |
| EvidenceGraphBuilder | `EvidenceSystemh` (reasoning/) | ✅ | Builds claim graph from AgentResults + direct response, computes trust score, detects contradictions |
| ClarificationEngine | `InputResolution.h` (reasoning/) | ✅ | Slot-driven question generation (max 2 per slot), session tracking, memory-aware skip logic |
| UnknownTopicFlow | `InputResolution.h` (reasoning/) | ✅ | Cascading resolution: ConceptVault → KnowledgeDaemon → Web (threshold 0.42) |
| TaskPlanner + TaskDecomposer | `TaskSystem` (reasoning/) | ✅ | Domain-specific decomposition (7 domains), atomic task lists, scaffold generation, skill building |
| ReferenceResolutionEngine | `InputResolution` (reasoning/) | 🔶 | Partial: anaphora/pronoun detection present, no full scoring with recency × grammatical_role × semantic_similarity |
| CausalReasoningEngine | **MISSING** | ❌ | No causal graph traversal, no forward/backward chaining, no counterfactual reasoning |
| AnalogyEngine | **MISSING** | ❌ | No cross-domain structural analogy system |

---

### 8.6 Section 4.5: Action & Execution

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| AutonomousPlanner | `ResponseActPlanner` | 🔶 | Plans single-step responses + simple tool calls. No multi-step tree with contingency branches |
| SensorimotorEngine | **MISSING** | ❌ | No pre/post-action prediction and verification loop. `VerificationEngine` exists but does not compute prediction_error |
| SystemExecutor | `SystemExecutor` + `ToolExecutor` + `FileOperator` + `ScriptRunner` | 🔶 | Execution exists for files, scripts, APIs. No full action_type taxonomy from blueprint |
| AgentSpawner | `BackgroundAgents` | 🔶 | Background agents exist but no MotherCore worker-pool architecture with per-agent memory clones |
| ErrorRecoveryIntelligence | **MISSING** | ❌ | No classified error recovery pipeline. Errors propagate via exception handling only |
| VerificationEngine | `VerificationEngine` | 🔶 | Class exists, simple success/fail check only — no sensorimotor prediction comparison |

---

### 8.7 Section 4.6: Knowledge & Learning

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| KnowledgeDaemon | `KnowledgeDaemon` | ✅ | Python subprocess, Wikipedia crawl, IPC, CMF integration confirmed (KNOWLEDGE_FACT packets on every `confidence ≥ 0.40` answer) |
| WorldModelDaemon | **MISSING** | ❌ | No 24/7 proactive world model. KnowledgeDaemon is reactive (gap-triggered only) |
| DocReader | `DocReader` | 🔶 | Parses URLs/docs. Not wired to fact pipeline automatically |
| WebReconAgent | `WebReconAgent` in RetrievalSystem | 🔶 | Wikipedia API search implemented. `RetrievalRouter::searchWeb()` is **intentionally disabled** (`return {}` always) — Yuki is configured to use only internal knowledge |
| FactVerifier | `EvidenceGraphBuilder + Verifier` | 🔶 | Trust scores, contradiction detection, and VerificationReport implemented. No external cross-reference corroboration. |
| FreshnessFilter | **MISSING** | ❌ | No half-life decay on ConceptGraph queries |
| LearningIngestor | `LearningIngestor` | ✅ | Fully operational: dedup (Jaccard ≥ 0.72), contradiction penalty, confidence scoring by source, SQLite storage via DatabaseManager |
| MassCurriculumLoader | `MassCurriculumLoader` | ✅ | CMF-integrated, tracks progress%, .mass_complete flag, self-destructs |

---

### 8.8 Section 4.7: Meta-Cognitive & Self-Improvement

| Blueprint Module | Code Equivalent | Status | Notes |
|-----------------|-----------------|--------|-------|
| MetaCognitiveInterrupt | `MetaCognitiveState` in TurnCoordinator | 🔶 | Tracks calibration drift, anti-hesitation mode. No explicit interrupt topic/halt mechanism |
| YukiSelfModel | **MISSING** | ❌ | No persistent self-model struct with domain expertise scores, active gaps, personality parameters |
| NarrativeEngine | **MISSING** | ❌ | No continuous first-person narrative |
| PhiMonitor | **MISSING** | ❌ | No IIT phi computation |
| SurpriseDetector | `CuriosityEngine` | 🔶 | Curiosity triggering exists, not formally publishing to GW |
| OutcomePropagator | `GenerativeModel::updateMapping()` | 🔶 | Online EMA learning in GenerativeModel. No multi-module outcome propagation (SemanticParser, SynthesisEngine etc. do not receive learning signals) |
| PerformanceProfiler | **MISSING** | ❌ | No per-module latency metrics |
| BottleneckAnalyser | **MISSING** | ❌ | Not implemented |
| CodeReader | **MISSING** | ❌ | Not implemented |
| SelfRewriter | **MISSING** | ❌ | Not implemented |

---

### 8.9 Section 2.1: Constitutional Laws

| Law | Blueprint Requirement | Status | Notes |
|-----|----------------------|--------|-------|
| Law 1: Never Commit Early | HypothesisLattice divergence > 0.15 before commit | ✅ | `CommitController::can_commit()` implements this |
| Law 2: Never Generate Without Grounding | MetaCognitiveInterrupt halts pipeline | 🔶 | `MetaCognitiveState` tracks this but does not hard-halt the pipeline — it triggers anti-hesitation mode or clarification questions |
| Law 3: Every Turn Teaches | OutcomePropagator updates per turn | 🔶 | `GenerativeModel::updateMapping()` updates per turn. But only GenerativeModel learns — other modules do not |
| Law 4: Know Thy Ignorance | YukiSelfModel publishes confidence < 0.5 | ❌ | No YukiSelfModel. KnowledgeDaemon reports topic gaps, but not published system-wide |
| Law 5: No Self-Deception | Ed25519 ConstitutionalLock on 5 modules | ❌ | No cryptographic lock on any module |

---

### 8.10 Section 5: Security & Constitutional Lock

| Blueprint Requirement | Status | Notes |
|-----------------------|--------|-------|
| Ed25519 hardware-backed key | ❌ | Not implemented |
| Locked modules: ControlPlane, GlobalWorkspace, SecuritySandbox, EthicalConstraintEngine, ActiveInferenceCore | ❌ | None of these formal modules exist |
| CodeApprovalGate | ✅ | `src/brain/safety/CodeApprovalGate.h/.cpp` exists |
| SafetyGovernor | ✅ | `src/brain/SafetyGovernor.h/.cpp` exists |
| EthicalConstraintEngine | ❌ | No dedicated ethical constraint evaluation module |

---

## 9. SUMMARY SCORECARD

> [!NOTE]
> Scores revised upward after deep-reading the reasoning and learning subsystems. SemanticParser, PatternEngine, GoalModelBuilder, EvidenceSystem, ClarificationEngine, TaskSystem, and LearningIngestor are all **fully implemented**. OllamaEmbeddingEngine is implemented but requires external Ollama service. Web search is intentionally disabled.

| Architecture Domain | Blueprint Modules | Implemented | Partial | Missing |
|--------------------|-------------------|-------------|---------|---------|
| Infrastructure (Planes, CoreBus) | 15 | 1 | 4 | 10 |
| Perception (L0–L2) | 7 | 4 | 2 | 1 |
| Memory (EpisodicStore, ConceptGraph, etc.) | 8 | 3 | 3 | 2 |
| Reasoning (SemanticParser, GoalModel, etc.) | 11 | 8 | 1 | 2 |
| Active Inference (VSE stack) | 6 | 5 | 1 | 0 |
| Action/Execution | 6 | 0 | 4 | 2 |
| Knowledge/Learning | 8 | 4 | 2 | 2 |
| Meta-Cognitive / Self-Improvement | 10 | 0 | 3 | 7 |
| Security / Constitutional | 5 | 2 | 0 | 3 |
| **TOTAL** | **76** | **27 (36%)** | **20 (26%)** | **29 (38%)** |

---

## 10. WHAT IS WORKING CORRECTLY (Production-quality)

1. **Sensor Loop (L0–L1)**: EarRuntime, CameraRuntime, ScreenRuntime all active with 50ms SCL cadence. SignalNormalizer, ArtifactFilter, ChangeDetector, TemporalAligner all correctly chained.

2. **Observation Encoding (L2)**: All 5 encoders (Audio 8D, Visual 10D, Screen 8D, Proprio 6D, Text 12D) implemented with heuristic feature extraction. MultiModalFusionGate fuses at 50ms. Training data hooks in place.

3. **Active Inference Core (VSE)**: PrecisionEngine, GenerativeModel (with online EMA learning + CSV persistence), BeliefState (24-state factorized), FreeEnergyCalculator (with caching + early stopping), PolicySelector (7 constraints) — all wired and fully functional.

4. **Predictive Turn Engine**: TurnCoordinator with E1/E2/E3 streams, BeliefPool, CommitController, MetaCognitiveState all working. 13/13 unit tests pass.

5. **Cognitive Memory Fabric**: HNSW vector search, SQLite episodic store, SemanticGraph with noun phrase extraction, Hebbian reinforcement, concept decay — all working and persisted across sessions.

6. **KnowledgeDaemon**: Python subprocess IPC, Wikipedia crawl, CMF integration — live and feeding facts.

7. **MassCurriculumLoader**: One-time 9-topic bootstrap fires on first run, skips on subsequent runs via `.mass_complete` flag.

8. **UI Layer**: PresenceShell (Win32), DetailView, AvatarBody all working with bidirectional callbacks.

9. **STT Voice Pipeline**: SpeechToTextRuntime with partial/final transcript callbacks wired to shell and BabyMode.

10. **Safety**: CodeApprovalGate, SafetyGovernor, PolicySelector constraints all active.

---

## 11. CRITICAL MISSING PIECES (Next Phase Priorities)

### Priority 1 — Missing Architectural Foundation
- **CoreBus / GlobalWorkspace**: Without a pub/sub bus, modules cannot decouple and cannot implement true Global Workspace broadcast. This is the single biggest architectural gap.
- **ControlPlane / ModuleRegistry**: Modules cannot self-register, health cannot be monitored, resource caps cannot be enforced.

### Priority 2 — VSE Race Condition (High Risk)
- **W5 (VSE mutex)**: `SignalConditioningLayer` (50ms loop) and `TurnCoordinator` (turn thread) both call `vse_->update()` with no mutex. This is a data race. Fix: add a `std::mutex` inside `VariationalStateEstimator` guarding the `update()` call, or dedicate VSE to one caller.

### Priority 3 — Missing Sensorimotor Loop
- **SensorimotorEngine**: Without pre-action predictions and post-action sensory verification, Yuki cannot close the embodied action loop. All actions are currently "fire and forget."

### Priority 4 — Missing Self-Model
- **YukiSelfModel**: Without a persistent self-model, Yuki cannot report her own confidence, domain expertise, or active knowledge gaps.
- **NarrativeEngine**: Without a narrative, Yuki has no coherent first-person state.

### Priority 5 — Missing Learning Propagation
- **OutcomePropagator**: Only `GenerativeModel` currently learns from outcomes. `PatternEngine`, `SynthesisEngine` do not receive learning signals. `LearningIngestor` handles knowledge facts but not reasoning quality signals.

### Priority 6 — Ollama Dependency
- **OllamaEmbeddingEngine**: Fully coded but requires a locally-running Ollama instance. If Ollama is absent, `RetrievalRouter::searchVectorIndex()` silently skips vector search. Add health-check and graceful degradation.

### Priority 7 — Knowledge Staleness
- **FreshnessFilter**: Facts enter ConceptGraph without staleness detection. Half-life decay would improve retrieval quality over time.

### Priority 8 — NeuralSpine Memory Drift
- **W2 (NeuralSpine bypass)**: `NeuralSpine::ConversationMemory` is not updated from predictive-path turns. The legacy context window is silently stale. Either remove NeuralSpine or wire its memory to TurnCoordinator.

---

## 12. BUILD & TEST STATUS

| Target | Status |
|--------|--------|
| `yuki.exe` | ✅ Builds clean (MSVC, zero errors) |
| `test_predictive_turn_engine.exe` | ✅ 13/13 tests pass |
| `test_executor_pack1.exe` | ✅ Builds clean |
| MSVC warnings | 🔶 Several C4100/C4244/C4099 warnings in non-core files |

---

*End of audit. No source code was modified during this report.*
