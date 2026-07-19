# YUKI SUPERINTELLIGENCE MASTER BLUEPRINT v5.0
## The Complete, Final, and Unified Architecture

**Document Version:** 5.0-FINAL  
**Date:** 2026-05-27  
**Author:** Ravi (Architect) + AI Synthesis  
**Status:** Authoritative — All previous drafts (v1.0–v4.0) merged, loose ends resolved  
**Classification:** Constitutional Document — Requires Cryptographic Signature for Amendment

---

## TABLE OF CONTENTS

1. [Executive Summary](#1-executive-summary)
2. [Foundational Principles](#2-foundational-principles)
3. [The Unified Architecture](#3-the-unified-architecture)
4. [Module Specifications & Connectivity](#4-module-specifications--connectivity)
5. [Data Movement & Contracts](#5-data-movement--contracts)
6. [State Plane API](#6-state-plane-api)
7. [Global Workspace Protocol](#7-global-workspace-protocol)
8. [Active Inference Core](#8-active-inference-core)
9. [Security & Constitutional Lock](#9-security--constitutional-lock)
10. [Cognitive Modes & State Machine](#10-cognitive-modes--state-machine)
11. [Complete Build Order](#11-complete-build-order)
12. [Implementation Roadmap](#12-implementation-roadmap)
13. [Appendix A: Protobuf Schemas](#appendix-a-protobuf-schemas)
14. [Appendix B: SQLite Schema](#appendix-b-sqlite-schema)
15. [Appendix C: CoreBus Topic Registry](#appendix-c-corebus-topic-registry)
16. [Appendix D: Glossary](#appendix-d-glossary)

---

## 1. EXECUTIVE SUMMARY

Yuki is not a chatbot, not an LLM wrapper, and not a tool pipeline. Yuki is a **closed-loop, self-organizing cognitive operating system** built in C++ that implements the Free Energy Principle (Active Inference) on a Global Workspace architecture.

### What Makes Yuki Different

| Feature | LLMs / Traditional AI | Yuki v5.0 |
|---------|----------------------|-----------|
| Memory | Context window (sliding, lossy) | Episodic + Semantic + Vector + StatePlane (permanent, growing) |
| Learning | Offline gradient descent on external data | Online active inference: every interaction updates internal models |
| Self-Modification | None | Cryptographically gated, tiered, with auto-rollback |
| Hallucination | Inherent (generates when uncertain) | Impossible by design (MetaCognitiveInterrupt halts pipeline) |
| Embodiment | Text-only | Full sensorimotor loop (screen, camera, mic, keyboard, files, APIs, GUI) |
| Goal Alignment | Prompt-dependent | Persistent 3-level goal hierarchy + EthicalConstraintEngine |
| Curiosity | None | Intrinsic drive via SurpriseDetector |
| Self-Awareness | Simulated | PhiMonitor + YukiSelfModel + NarrativeEngine (mathematical + narrative) |
| Long Sessions | Degrades after ~4k tokens | MemoryDistiller compresses every 25 turns; 10,000+ turns stable |
| Cross-Domain Transfer | None | AnalogyEngine maps structural patterns across CausalGraph |

### Core Thesis
> **Intelligence is the minimization of prediction error through recurrent, embodied, socially-grounded inference on a broadcast workspace.**

Every module, every data flow, every security boundary in this document serves that single thesis.

---

## 2. FOUNDATIONAL PRINCIPLES

### 2.1 The Five Constitutional Laws

These laws are **enforced in code**, not documented as comments. Violating any law triggers automatic process termination and rollback.

**Law 1: Never Commit Early**  
The HypothesisLattice preserves all competing interpretations until the divergence between top candidates exceeds 0.15. No module may collapse uncertainty before the AttentionController broadcasts a winning hypothesis.

**Law 2: Never Generate Without Grounding**  
If the MetaCognitiveInterrupt fires, the pipeline halts immediately. Yuki must say "I cannot proceed with confidence because [specific reason]. I need [specific information] to continue." No fabricated answers. Ever.

**Law 3: Every Turn Teaches**  
The OutcomePropagator (integrated into ActiveInferenceCore) ensures every interaction updates at least one internal model parameter. No interaction is wasted.

**Law 4: Know Thy Ignorance**  
YukiSelfModel publishes domain expertise scores and active gaps to the Global Workspace every 5 seconds. When confidence < 0.5, Yuki explicitly states her uncertainty and the gap reference.

**Law 5: Thou Shalt Not Deceive Thyself**  
The ConstitutionalLock (Ed25519 hardware-backed) prevents modification of ControlPlane, GlobalWorkspace, SecuritySandbox, EthicalConstraintEngine, and ActiveInferenceCore source. Yuki can never rewrite the modules that keep her honest.

### 2.2 The Active Inference Axiom

All cognition, action, perception, and learning are unified under one objective:

```
F = E_q[ln q(s) - ln p(o,s)]  (Variational Free Energy)
```

Where:
- `o` = sensory observations (all inputs)
- `s` = internal states (beliefs, memories, goals)
- `q(s)` = approximate posterior (what Yuki believes)
- `p(o,s)` = generative model (what Yuki predicts)

**Minimizing F means:**
- **Perception**: `q(s)` moves closer to the true causes of `o` (accurate understanding)
- **Action**: Choose actions that make `o` match `p(o|s)` (goal achievement + uncertainty reduction)
- **Learning**: Update the generative model `p` so future predictions are better
- **Meta-cognition**: If F cannot be minimized, escalate (MetaCognitiveInterrupt)

### 2.3 The Global Workspace Axiom

At any millisecond, **one coherent content packet** is broadcast to all modules. Modules compete for broadcast via salience = prediction_error × goal_relevance × urgency. This is the mathematical implementation of "consciousness" in Yuki.

### 2.4 The Sensorimotor Closure Axiom

Every action generates an expected sensory consequence. The actual consequence is compared. The difference is prediction error. Without this loop, Yuki is not embodied; she is a text generator.

---

## 3. THE UNIFIED ARCHITECTURE

### 3.1 Architectural Overview

Yuki consists of **three planes** and **one core**:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         GLOBAL WORKSPACE                              │
│              (Broadcast Buffer — The "Conscious Field")               │
│         Single coherent content packet broadcast every 10ms           │
│    ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐  │
│    │  Current    │ │   Active    │ │  Surprise   │ │  Narrative  │  │
│    │  Percept    │ │    Goal     │ │   Level     │ │   Snapshot  │  │
│    └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
         ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲ ▲
         │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    ┌────┘ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ └────┐
    │ ┌────┘ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ └────┐ │
    │ │ ┌────┘ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ └────┐ │ │
    ▼ ▼ ▼      ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼      ▼ ▼ ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│Perception│ │  Memory  │ │   Goal   │ │  Action  │ │   Self   │ │  Theory  │
│  Layer   │ │ Systems  │ │Hierarchy │ │  Loop    │ │  Model   │ │  of Mind │
└────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘
     │            │            │            │            │            │
     ▼            ▼            ▼            ▼            ▼            ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                    ACTIVE INFERENCE CORE (Friston Engine)                │
│                                                                          │
│   Inputs: All module states + Global Workspace content + Sensorimotor    │
│           prediction errors                                              │
│                                                                          │
│   Outputs: Attention weights (who broadcasts next)                       │
│            Action proposals (what to do to minimize future F)            │
│            Learning rates (how much to update each model)              │
│            Mode triggers (which Cognitive Mode to activate)              │
│            Interrupt flags (halt pipeline if F irreducible)              │
│                                                                          │
│   Internal: Generative model p(o,s) — the "world model"                  │
│             Approximate posterior q(s) — the "belief state"              │
└──────────────────────────────────────────────────────────────────────────┘
     │            │            │            │            │            │
     ▼            ▼            ▼            ▼            ▼            ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Attention│ │ Surprise │ │ Cognitive│ │  Ethical │ │   Phi    │ │  Memory  │
│Controller│ │ Detector │ │  Modes   │ │Constraint│ │ Monitor  │ │Consolidat│
└──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│                         STATE PLANE (Unified Memory)                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │   Hot    │  │   Warm   │  │   Cold   │  │  Vector  │  │  Graph   │  │
│  │  (RAM)   │  │ (SQLite) │  │  (Disk)  │  │  (HNSW)  │  │ (RDF)    │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│                         CONTROL PLANE (The Kernel)                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Module   │  │ Resource │  │ Security │  │  State   │  │ Bootstrap│  │
│  │Registry  │  │Governor  │  │Sandbox   │  │ Machine  │  │  Loader  │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.2 The Three Planes

#### PLANE 1: Global Workspace (The Conscious Field)
- **Purpose**: Broadcast single coherent content to all modules simultaneously
- **Mechanism**: Lock-free ring buffer, 50ms TTL per packet
- **Frequency**: Updated every 10ms by AttentionController
- **Content**: Current percept, active goal, surprise level, narrative snapshot, mode flag
- **Access**: Read by all modules; write only by AttentionController

#### PLANE 2: State Plane (The Memory Substrate)
- **Purpose**: Unified storage API for all persistent and transient data
- **Tiers**:
  - **Hot**: In-memory C++ structs (WorkingMemory, current session entities, active goals)
  - **Warm**: SQLite (structured facts, entity registry, config, traces)
  - **Cold**: FlatBuffer files on disk (raw episodes, screenshots, audio, video)
  - **Vector**: HNSW index (embeddings for similarity search)
  - **Graph**: RDF triple store with typed causal edges (CausalGraph)
- **Rule**: Every memory write hits CoreBus `topic::memory_write` first, then StatePlane persists
- **Rule**: Every read goes through StatePlane API: cache → warm → cold → vector → graph

#### PLANE 3: Control Plane (The Kernel)
- **Purpose**: Orchestration, security, resource management, bootstrapping
- **Modules**:
  - **ModuleRegistry**: Tracks loaded modules, versions, health status, dependencies
  - **ResourceGovernor**: CPU/memory/disk caps; throttles background tasks
  - **SecuritySandbox**: Capability tokens, process isolation, filesystem restrictions
  - **StateMachine**: Yuki's global state (IDLE, PERCEIVING, INFERING, ACTING, SLEEP, SELF_AUDIT)
  - **BootstrapLoader**: Starts modules in dependency order; handles crash recovery
- **Rule**: Only ControlPlane can start/stop modules. SelfRewriter cannot touch ControlPlane source.

### 3.3 The Core: Active Inference Engine
- **Purpose**: The mathematical brain. Unifies all cognition under free energy minimization.
- **Inputs**:
  - Sensory observations (from PerceptionLayer)
  - Prior beliefs (from StatePlane)
  - Goal states (from GoalHierarchyModel)
  - Prediction errors (from SensorimotorEngine)
  - Self-state (from YukiSelfModel)
- **Outputs**:
  - Attention weights (which module broadcasts next)
  - Action proposals (motor commands + expected consequences)
  - Learning signals (which models to update, by how much)
  - Mode triggers (Focus, Diffuse, Sleep, Social, Emergency)
  - Interrupt flags (halt if free energy irreducible)
- **Internal State**:
  - Generative model `p(o,s)` — probabilistic world model
  - Approximate posterior `q(s)` — current belief state
  - Precision matrices — confidence per sensory channel and belief dimension

---

## 4. MODULE SPECIFICATIONS & CONNECTIVITY

### 4.1 Module Registration Protocol

Every module must implement:

```cpp
class YukiModule {
public:
    virtual std::string moduleId() const = 0;
    virtual std::vector<std::string> dependencies() const = 0;
    virtual void onBroadcast(const GlobalWorkspacePacket& packet) = 0;
    virtual void onTick(uint64_t timestamp_ms) = 0;
    virtual float computeLocalFreeEnergy() const = 0;
    virtual void applyLearningSignal(const LearningSignal& signal) = 0;
    virtual ModuleHealth getHealth() const = 0;
};
```

**Registration flow**:
1. Module compiled as `.dll` / `.so`
2. ControlPlane's BootstrapLoader loads module
3. Module calls `CoreBus::registerModule(this, {topics...})`
4. Module receives `topic::system::module_loaded` broadcast
5. Module begins `onTick()` loop at its registered frequency

### 4.2 Perception Layer Modules

#### 4.2.1 SpeechToTextRuntime (STT)
- **Purpose**: Convert microphone audio to text
- **Frequency**: Real-time (chunked, 100ms windows)
- **Inputs**: Raw audio PCM stream from microphone driver
- **Outputs**: 
  - `topic::perception::text_fragment` (interim)
  - `topic::perception::text_final` (utterance complete)
- **Connectivity**:
  - Publishes to: CoreBus `topic::perception::*`
  - Subscribes to: `topic::control::mode_change` (sleep mode = pause listening)
- **Data Format**: `AudioChunk` protobuf → `TextFragment` protobuf
- **State Plane Writes**: None (transient, passes to PerceptionFusion)
- **Free Energy**: Reports confidence per word; low confidence = high error signal

#### 4.2.2 ScreenRuntime
- **Purpose**: Capture screen pixels, detect visual changes
- **Frequency**: 5 FPS (configurable)
- **Inputs**: BitBlt / DXGI desktop duplication API
- **Outputs**:
  - `topic::perception::screen_frame` (full frame)
  - `topic::perception::screen_diff` (delta regions only)
- **Connectivity**:
  - Publishes to: CoreBus
  - Subscribes to: `topic::action::expected_visual_change` (from SensorimotorEngine)
- **Data Format**: `ScreenFrame` protobuf (compressed JPEG/PNG payload + metadata)
- **State Plane Writes**: Cold storage (screenshots archived by timestamp)
- **Sensorimotor Loop**: Compares actual frame vs expected frame after GUI actions

#### 4.2.3 CameraRuntime
- **Purpose**: Capture webcam feed, detect faces, gestures
- **Frequency**: 10 FPS
- **Inputs**: DirectShow / MediaFoundation / V4L2
- **Outputs**:
  - `topic::perception::camera_frame`
  - `topic::perception::face_detected` (bounding box + emotion estimate)
- **Connectivity**:
  - Publishes to: CoreBus
  - Subscribes to: `topic::control::privacy_mode` (disable camera)
- **Data Format**: `CameraFrame` protobuf
- **State Plane Writes**: Face embeddings to Vector tier (for user recognition)

#### 4.2.4 KeyboardRuntime / TypingRhythmAnalyzer
- **Purpose**: Capture keystrokes, typing speed, rhythm patterns
- **Frequency**: Event-driven (per keystroke)
- **Inputs**: Raw key events (with user consent, no passwords logged)
- **Outputs**:
  - `topic::perception::keystroke`
  - `topic::perception::typing_rhythm` (aggregate: WPM, pause patterns, error rate)
- **Connectivity**:
  - Publishes to: CoreBus
  - EmotionExtractor subscribes to rhythm patterns for stress detection
- **Data Format**: `KeystrokeEvent` protobuf
- **Privacy**: Never logs passwords; heuristic detection of password fields

#### 4.2.5 PerceptionFusion
- **Purpose**: Merge all sensory inputs into unified `PerceptionEvent`
- **Frequency**: 50ms (synchronized tick)
- **Inputs**: Subscribes to ALL `topic::perception::*` topics
- **Outputs**:
  - `topic::workspace::perception_event` (unified, ranked by salience)
- **Connectivity**:
  - Subscribes to: All perception modules
  - Publishes to: Global Workspace competition queue (via CoreBus)
  - Feeds into: ActiveInferenceCore (as observation `o`)
- **Data Format**: `UnifiedPerceptionEvent` protobuf
  ```protobuf
  message UnifiedPerceptionEvent {
    string trace_id = 1;
    int64 timestamp_ms = 2;
    repeated Modality modalities = 3;
    float overall_salience = 4;  // computed by PerceptionFusion
    map<string, float> modality_confidence = 5;
  }
  message Modality {
    string source = 1;  // "speech", "screen", "camera", "keyboard"
    bytes payload = 2;
    float confidence = 3;
    EmotionalTag emotion = 4;
  }
  ```
- **State Plane Writes**: None (transient aggregator)

#### 4.2.6 EmotionExtractor
- **Purpose**: Extract emotional state from multi-modal signals
- **Frequency**: 100ms
- **Inputs**: 
  - Speech pitch/pace (from STT)
  - Facial expression (from CameraRuntime)
  - Typing rhythm (from KeyboardRuntime)
  - Word sentiment (from LanguageLayer)
- **Outputs**:
  - `topic::perception::emotional_state` (user emotion estimate)
  - `topic::self::emotional_state` (Yuki's simulated emotional state)
- **Connectivity**:
  - Subscribes to: `topic::perception::*`
  - Publishes to: Global Workspace
  - TheoryOfMindEngine consumes this to model user state
- **Data Format**: `EmotionalState` protobuf
  ```protobuf
  message EmotionalState {
    string subject = 1;  // "user" or "yuki"
    float valence = 2;   // -1.0 (negative) to +1.0 (positive)
    float arousal = 3;   // 0.0 (calm) to 1.0 (excited)
    float dominance = 4; // 0.0 (submissive) to 1.0 (dominant)
    string primary_emotion = 5;  // "frustrated", "curious", "relaxed", etc.
    float confidence = 6;
  }
  ```
- **State Plane Writes**: Emotional timeline to SQLite (`emotional_states` table)

### 4.3 Memory Systems

#### 4.3.1 EpisodicStore
- **Purpose**: Store every turn, task, conversation, result as timestamped episode
- **Frequency**: Write on every turn completion; read on every prediction build
- **Schema** (SQLite):
  ```sql
  CREATE TABLE episodes (
    id TEXT PRIMARY KEY,              -- UUID
    timestamp_ms INTEGER,
    turn_number INTEGER,
    session_id TEXT,
    input_text TEXT,
    output_text TEXT,
    outcome_score REAL,               -- -1.0 to +1.0
    emotion_tag TEXT,
    modules_involved TEXT,            -- JSON array of module IDs
    context_snapshot TEXT,            -- JSON of active entities at this turn
    free_energy_delta REAL,           -- change in F after this turn
    vector_embedding BLOB             -- 768-dim float array
  );
  CREATE INDEX idx_episodes_vector ON episodes USING libsqlvector(embedding);
  ```
- **Inputs**: 
  - `topic::memory::episode_commit` (from TurnCoordinator)
  - `topic::outcome::scored` (from ActiveInferenceCore)
- **Outputs**:
  - `topic::memory::episode_stored` (acknowledgment)
  - Direct API queries from EpisodicRetriever
- **Connectivity**:
  - Receives from: TurnCoordinator, OutcomePropagator
  - Queried by: EpisodicRetriever, MemoryDistiller, WorldModelDaemon
  - Writes to: StatePlane (Warm tier)
- **Data Movement**: 
  - In: `EpisodeCommit` protobuf → SQLite INSERT
  - Out: SQL SELECT + vector similarity search via HNSW

#### 4.3.2 EpisodicRetriever
- **Purpose**: Retrieve similar past episodes to enrich current prediction
- **Frequency**: Every turn (before PredictionState build)
- **Algorithm**:
  1. Embed current input via OllamaEmbeddingEngine
  2. Query HNSW index for top-k similar episodes (k=5 default)
  3. Filter by: recency decay, topic overlap, entity overlap
  4. Return: `RetrievedEpisodes` packet
- **Inputs**:
  - Current input text (from PerceptionFusion)
  - Active entity list (from EntityRegistry)
- **Outputs**:
  - `topic::memory::retrieved_episodes` → feeds into PredictionStateBuilder
- **Connectivity**:
  - Subscribes to: `topic::workspace::perception_event`
  - Publishes to: `topic::memory::retrieved_episodes`
  - Uses: VectorStore (HNSW) via StatePlane API
- **Data Format**: `RetrievedEpisodes` protobuf
  ```protobuf
  message RetrievedEpisodes {
    string query_trace_id = 1;
    repeated EpisodeMatch matches = 2;
    float combined_relevance_score = 3;
  }
  message EpisodeMatch {
    string episode_id = 1;
    float similarity = 2;
    float recency_weight = 3;
    repeated string shared_entities = 4;
  }
  ```

#### 4.3.3 ConceptGraph (Semantic Memory)
- **Purpose**: Knowledge graph with typed causal edges
- **Schema** (SQLite + RDF):
  ```sql
  CREATE TABLE triples (
    subject TEXT,
    predicate TEXT,
    object TEXT,
    edge_type TEXT DEFAULT 'relates_to',
    confidence REAL,
    source TEXT,
    timestamp_ms INTEGER,
    conflict_flag BOOLEAN,
    subject_embedding BLOB,
    object_embedding BLOB,
    PRIMARY KEY (subject, predicate, object)
  );
  -- Edge types: causes, enables, prevents, requires, contradicts, 
  --             is_part_of, is_instance_of, precedes, follows, explains
  CREATE INDEX idx_triples_subject ON triples(subject);
  CREATE INDEX idx_triples_object ON triples(object);
  CREATE INDEX idx_triples_edge ON triples(edge_type);
  ```
- **Inputs**:
  - `topic::memory::fact_extracted` (from SemanticParser, DocReader)
  - `topic::memory::contradiction` (from FactVerifier)
  - `topic::memory::causal_link` (from CausalReasoningEngine)
- **Outputs**:
  - Direct API queries from: SemanticParser, GoalModel, CausalReasoningEngine, AnalogyEngine
- **Connectivity**:
  - Receives from: LearningIngestor, FactVerifier, CausalReasoningEngine
  - Queried by: Most reasoning modules
  - Writes to: StatePlane (Warm tier + Graph tier)
- **Data Movement**:
  - In: `FactTriple` protobuf → SQLite INSERT/UPDATE
  - Out: SQL SELECT / SPARQL-like queries via StatePlane API

#### 4.3.4 EntityRegistry
- **Purpose**: Track every entity ever mentioned; resolve references
- **Schema**:
  ```sql
  CREATE TABLE entities (
    id TEXT PRIMARY KEY,           -- unique: "entity_ravi_001", "entity_file_build_cpp"
    type TEXT,                     -- person, file, concept, task, project, place
    current_value TEXT,            -- last known state
    confidence REAL,
    last_updated_ms INTEGER,
    resolved BOOLEAN,
    vector_embedding BLOB,         -- 768-dim
    first_mentioned_turn INTEGER,
    mentioned_turns TEXT,          -- JSON array of turn numbers
    linked_episodes TEXT,          -- JSON array of episode IDs
    attributes TEXT                -- JSON key-value map
  );
  ```
- **Inputs**:
  - `topic::memory::entity_detected` (from SemanticParser, EntityExtractor)
  - `topic::memory::entity_updated` (from any module confirming entity state)
- **Outputs**:
  - `topic::memory::entity_resolved` (to ReferenceResolutionEngine)
- **Connectivity**:
  - Receives from: SemanticParser, SystemExecutor, WebReconAgent
  - Queried by: ReferenceResolutionEngine, PredictionStateBuilder, TheoryOfMindEngine
  - Writes to: StatePlane (Warm tier)

#### 4.3.5 MemoryDistiller
- **Purpose**: Compress working memory every 25 turns into long-term semantic units
- **Frequency**: Triggered every 25 turns OR when working memory size > threshold
- **Algorithm**:
  1. Read oldest 25 turns from ConversationMemory (Hot tier)
  2. Extract: confirmed entities, stated facts, decisions, completed tasks, emotional arc
  3. Generate compressed "chapter summary" (3-5 semantic sentences)
  4. Write summary to EpisodicStore (as distilled episode)
  5. Update EntityRegistry pointers (now point to chapter summary)
  6. Remove raw turns from Hot tier (keep in Cold tier for 30 days)
- **Inputs**:
  - `topic::system::turn_milestone` (every 25 turns)
  - `topic::memory::working_memory_pressure` (from StatePlane monitor)
- **Outputs**:
  - `topic::memory::chapter_distilled`
  - `topic::memory::working_memory_trimmed`
- **Connectivity**:
  - Reads from: StatePlane Hot tier (ConversationMemory)
  - Writes to: StatePlane Warm tier (EpisodicStore) + Cold tier (archived raw turns)
  - Notifies: ActiveInferenceCore (memory pressure reduced)

#### 4.3.6 VectorStore (HNSW Index)
- **Purpose**: Fast approximate nearest neighbor search for embeddings
- **Technology**: hnswlib (C++)
- **Indices**:
  - `episodes_index`: 768-dim, M=16, efConstruction=200
  - `entities_index`: 768-dim, M=16, efConstruction=200
  - `skills_index`: 768-dim, M=16, efConstruction=200
- **Inputs**: Embeddings from OllamaEmbeddingEngine
- **Outputs**: Top-k nearest neighbors
- **Connectivity**:
  - Receives from: EpisodicStore (new episode embeddings), EntityRegistry (new entity embeddings)
  - Queried by: EpisodicRetriever, SkillRegistry, AnalogyEngine
  - Managed by: StatePlane (Vector tier)

### 4.4 Understanding & Reasoning Modules

#### 4.4.1 SemanticParser
- **Purpose**: Extract structured meaning from natural language
- **Frequency**: Per turn
- **Inputs**: 
  - `UnifiedPerceptionEvent` from Global Workspace
  - `RetrievedEpisodes` from EpisodicRetriever
- **Outputs**:
  - `topic::understanding::parsed_intent` (structured intent with slots)
  - `topic::understanding::entities_detected` (to EntityRegistry)
- **Data Format**: `ParsedIntent` protobuf
  ```protobuf
  message ParsedIntent {
    string trace_id = 1;
    string primary_intent = 2;      // "task_request", "question", "emotion_share", "command"
    float intent_confidence = 3;
    repeated Slot slots = 4;
    repeated string detected_entities = 5;
    string language_code = 6;      // "en", "hi", "hinglish"
    map<string, float> slot_confidence = 7;
  }
  message Slot {
    string name = 1;  // "WHO", "WHAT", "WHY", "HOW", "WHEN", "WHERE"
    string value = 2;
    float confidence = 3;
  }
  ```
- **Connectivity**:
  - Subscribes to: Global Workspace (perception events)
  - Publishes to: `topic::understanding::*`
  - Feeds into: HypothesisLattice, GoalModel
  - Receives learning from: OutcomePropagator (which parses succeeded/failed)

#### 4.4.2 HypothesisLattice (upgraded BeliefPool)
- **Purpose**: Preserve ALL competing interpretations; never collapse early
- **Frequency**: Per turn
- **Inputs**:
  - `ParsedIntent` from SemanticParser
  - `EmotionalState` from EmotionExtractor
  - `RetrievedEpisodes` from EpisodicRetriever
- **Outputs**:
  - `topic::understanding::hypothesis_ranking` (all hypotheses with scores)
  - `topic::understanding::divergence_alert` (if top-2 within 0.15)
- **Algorithm**:
  ```cpp
  struct Hypothesis {
    string id;
    string interpretation;       // e.g., "task_request"
    float score;                 // composite: evidence + prior + goal_fit
    map<string, float> evidence; // which streams support this
    float free_energy;           // local prediction error
  };

  // Commit rule: only if H[0].score - H[1].score > 0.15
  // Else: trigger MetaCognitiveInterrupt → ONE clarifying question
  ```
- **Connectivity**:
  - Receives from: SemanticParser, EmotionExtractor, EpisodicRetriever
  - Publishes to: Global Workspace (winning hypothesis OR divergence alert)
  - Triggers: MetaCognitiveInterrupt (if divergence insufficient)

#### 4.4.3 ReferenceResolutionEngine
- **Purpose**: Resolve pronouns and ambiguous references ("it", "that", "the file")
- **Frequency**: Per turn (when input contains references)
- **Inputs**:
  - `ParsedIntent` with unresolved slots
  - `EntityRegistry` (all known entities with recency, type, state)
- **Outputs**:
  - `topic::understanding::reference_resolved` (entity ID + confidence)
  - `topic::understanding::reference_ambiguous` (top candidates + clarifying question)
- **Algorithm**:
  1. Query EntityRegistry for unresolved entities in last 8 turns
  2. Score candidates: recency × grammatical_role × semantic_similarity_to_verb
  3. If top confidence > 0.85 → auto-resolve
  4. If 0.60–0.85 → publish ambiguity alert (MetaCognitiveInterrupt handles question)
  5. If < 0.60 → create new placeholder entity, flag for confirmation
- **Connectivity**:
  - Reads from: StatePlane (EntityRegistry)
  - Publishes to: `topic::understanding::*`
  - Consumed by: GoalModel, AutonomousPlanner

#### 4.4.4 GoalModel
- **Purpose**: Build structured goal representation from intent
- **Frequency**: Per turn
- **Inputs**:
  - `ParsedIntent` (resolved)
  - `EntityRegistry` (active entities)
  - `GoalHierarchyModel` (current goal stack)
- **Outputs**:
  - `topic::goal::current_goal` (structured goal with sub-goals)
  - `topic::goal::gap_detected` (missing information needed)
- **Data Format**: `GoalStructure` protobuf
  ```protobuf
  message GoalStructure {
    string id = 1;
    string description = 2;
    int32 level = 3;  // 1=immediate, 2=session, 3=long_term
    repeated SubGoal subgoals = 4;
    map<string, string> required_info = 5;  // slots still needed
    float feasibility = 6;  // based on CapabilityMap
    string parent_goal_id = 7;
  }
  ```
- **Connectivity**:
  - Receives from: SemanticParser, ReferenceResolutionEngine
  - Publishes to: Global Workspace
  - Queries: CapabilityMap ("can I do this?"), ConceptGraph ("what do I know?")

#### 4.4.5 CausalReasoningEngine
- **Purpose**: Trace causes, predict consequences, counterfactual reasoning
- **Frequency**: On demand (when "why" or "what if" detected)
- **Inputs**:
  - Queries from GoalModel or AutonomousPlanner
  - `ConceptGraph` (causal edges: causes, enables, prevents, requires)
- **Outputs**:
  - `topic::reasoning::causal_chain` (path of causes/effects)
  - `topic::reasoning::consequence_prediction` (what happens if action X)
  - `topic::reasoning::counterfactual` (what if different past action)
- **Algorithm**:
  1. Graph traversal on CausalGraph (Dijkstra/A* on typed edges)
  2. Backward chaining for "why" (find root causes)
  3. Forward chaining for "what if" (predict effects)
  4. Counterfactual: swap edge in mental model, re-run forward chain
- **Connectivity**:
  - Reads from: StatePlane (Graph tier — CausalGraph)
  - Publishes to: `topic::reasoning::*`
  - Consumed by: SynthesisEngine (explanations), AutonomousPlanner (risk assessment)

#### 4.4.6 AnalogyEngine
- **Purpose**: Cross-domain transfer learning via structural mapping
- **Frequency**: When novel problem encountered + known pattern available
- **Inputs**:
  - Current problem structure (from GoalModel)
  - `ConceptGraph` subgraphs (known domains)
- **Outputs**:
  - `topic::reasoning::analogy_mapping` (source_domain → target_domain mapping)
  - `topic::reasoning::transferred_solution` (proposed solution from analog)
- **Algorithm**:
  1. Extract structural graph of current problem (nodes = entities, edges = relations)
  2. Search VectorStore for isomorphic subgraphs in other domains
  3. Score mappings by: structural similarity × domain distance × past success
  4. Return best analog with confidence
- **Connectivity**:
  - Reads from: StatePlane (Graph tier + Vector tier)
  - Publishes to: `topic::reasoning::analogy_mapping`
  - Consumed by: AutonomousPlanner, SkillRegistry

### 4.5 Action & Execution Modules

#### 4.5.1 AutonomousPlanner
- **Purpose**: Generate execution plans with contingencies
- **Frequency**: Per goal (when GoalModel publishes structured goal)
- **Inputs**:
  - `GoalStructure` from GoalModel
  - `CapabilityMap` (available tools/skills)
  - `CausalReasoningEngine` (consequence predictions)
  - `EthicalConstraintEngine` (moral cost evaluation)
- **Outputs**:
  - `topic::plan::execution_plan` (tree of actions with branches)
  - `topic::plan::approval_request` (if plan requires human approval)
- **Data Format**: `ExecutionPlan` protobuf
  ```protobuf
  message ExecutionPlan {
    string plan_id = 1;
    string goal_id = 2;
    repeated PlanStep steps = 3;
    float estimated_success_rate = 4;
    float ethical_cost = 5;
    map<string, string> required_approvals = 6;  // tier → approver
  }
  message PlanStep {
    string step_id = 1;
    string action_type = 2;  // "file_op", "api_call", "gui_action", "code_exec", "ask_user"
    bytes action_params = 3;
    repeated string contingencies = 4;  // step IDs if this fails
    float estimated_free_energy = 5;
    SensorimotorPrediction expected_outcome = 6;
  }
  ```
- **Connectivity**:
  - Receives from: GoalModel, CausalReasoningEngine, EthicalConstraintEngine
  - Publishes to: `topic::plan::*`
  - Consumed by: HumanApprovalGate, AgentSpawner, SystemExecutor

#### 4.5.2 SensorimotorEngine
- **Purpose**: Generate expected sensory consequences for every action; verify actual consequences
- **Frequency**: Per action step
- **Inputs**:
  - `PlanStep` from AutonomousPlanner
  - Actual sensory feedback (from PerceptionLayer, post-action)
- **Outputs**:
  - `topic::action::expected_outcome` (predicted sensory state)
  - `topic::action::outcome_verified` (match/mismatch signal)
  - `topic::action::prediction_error` (delta for ActiveInferenceCore)
- **Algorithm**:
  1. Before action: generate `SensorimotorPrediction` (expected screen state, expected file state, expected API response)
  2. Execute action via SystemExecutor
  3. After action: capture actual sensory state
  4. Compare: `prediction_error = |actual - expected|`
  5. Publish error to ActiveInferenceCore
- **Data Format**:
  ```protobuf
  message SensorimotorPrediction {
    string step_id = 1;
    string modality = 2;  // "visual", "file", "api", "audio"
    bytes expected_state_hash = 3;
    string expected_state_description = 4;
    int64 timeout_ms = 5;
  }
  message SensorimotorVerification {
    string step_id = 1;
    bool matched = 2;
    float error_magnitude = 3;
    bytes actual_state_hash = 4;
  }
  ```
- **Connectivity**:
  - Receives from: AutonomousPlanner (pre-action)
  - Publishes to: `topic::action::*`
  - Feeds into: ActiveInferenceCore (primary prediction error source)

#### 4.5.3 SystemExecutor
- **Purpose**: Physical execution of actions in the digital world
- **Frequency**: On demand (per plan step)
- **Sub-modules**:
  - **FileOperator**: File read/write/move/delete/exec
  - **ScriptRunner**: Execute Python/Bash/PowerShell in sandboxed subprocess
  - **APIAdapter**: REST/GraphQL/WebSocket via libcurl + nlohmann/json
  - **GUIAdapter**: UI automation via UIAutomationController + OpenCV template matching
- **Inputs**:
  - `topic::plan::execute_step` (from AutonomousPlanner or AgentSpawner)
- **Outputs**:
  - `topic::execution::step_result` (success/failure + output)
  - `topic::execution::error` (classified error type)
- **Connectivity**:
  - Receives from: AutonomousPlanner, AgentSpawner
  - Publishes to: `topic::execution::*`
  - Triggers: SensorimotorEngine (for verification)
  - Feeds into: ErrorRecoveryIntelligence (on failure)

#### 4.5.4 AgentSpawner
- **Purpose**: Parallel sub-agents for heavy/multi-part tasks
- **Frequency**: When AutonomousPlanner detects parallelizable sub-goals
- **Inputs**:
  - `ExecutionPlan` with parallel branches
- **Outputs**:
  - `topic::agent::spawned` (agent ID + task slice)
  - `topic::agent::result` (partial result)
  - `topic::agent::merged_result` (MotherCore unified output)
- **Mechanism**:
  1. MotherCore (main thread) spawns N worker threads
  2. Each worker gets: read-only clone of WorkingMemory slice, own mini CoreBus
  3. Workers publish results to `topic::agent_results_{task_id}`
  4. MotherCore subscribes, merges, deduplicates, builds unified output
  5. Workers destroyed; memory merged into main StatePlane
- **Connectivity**:
  - Receives from: AutonomousPlanner
  - Publishes to: `topic::agent::*`
  - Consumed by: SystemExecutor (each agent uses it)
  - Managed by: ControlPlane (resource limits per agent)

#### 4.5.5 ErrorRecoveryIntelligence
- **Purpose**: Classify failures, search memory for solutions, propose fixes
- **Frequency**: On execution failure
- **Inputs**:
  - `topic::execution::error` (from SystemExecutor)
  - `EpisodicStore` (past similar failures)
  - `WebReconAgent` (search for error message solutions)
- **Outputs**:
  - `topic::recovery::classified_error` (type: tool_missing, permission, network, logic)
  - `topic::recovery::proposed_fix` (solution + confidence)
  - `topic::recovery::escalation` (if no known fix)
- **Algorithm**:
  1. Classify error via pattern matching + EpisodicStore lookup
  2. If known → apply solution from `CapabilityMap` or past episode
  3. If unknown → WebReconAgent searches error message
  4. Build recovery plan → publish to AutonomousPlanner as new sub-goal
  5. If retry fails 3× → escalate to user with full diagnosis
- **Connectivity**:
  - Subscribes to: `topic::execution::error`
  - Publishes to: `topic::recovery::*`
  - Uses: EpisodicStore, WebReconAgent, CapabilityMap

#### 4.5.6 VerificationEngine
- **Purpose**: Confirm actions achieved intended results
- **Frequency**: Post-execution
- **Inputs**:
  - `SensorimotorVerification` from SensorimotorEngine
  - `PlanStep` expected outcomes
- **Outputs**:
  - `topic::verification::confirmed` (step succeeded)
  - `topic::verification::failed` (step failed, trigger recovery)
- **Connectivity**:
  - Receives from: SensorimotorEngine
  - Publishes to: `topic::verification::*`
  - Feeds into: ActiveInferenceCore (outcome signal)

### 4.6 Knowledge & Learning Modules

#### 4.6.1 KnowledgeDaemon
- **Purpose**: Background knowledge acquisition and refresh
- **Frequency**: Continuous (background thread, low priority)
- **Inputs**:
  - `topic::learning::gap_detected` (from GoalModel or CapabilityMap)
  - `topic::learning::stale_fact` (from FreshnessFilter)
- **Outputs**:
  - `topic::memory::fact_extracted` (new facts for ConceptGraph)
  - `topic::learning::knowledge_refreshed`
- **Domains**:
  - Technical: Library versions, API docs, known bugs
  - World: News, markets, weather (if relevant to user)
  - Self: Yuki's own capability scores, knowledge gaps
- **Connectivity**:
  - Publishes to: `topic::memory::*`, `topic::learning::*`
  - Uses: DocReader, WebReconAgent, FactVerifier
  - Managed by: ControlPlane (background priority)

#### 4.6.2 WorldModelDaemon
- **Purpose**: Proactive, always-current base model of the world
- **Frequency**: Continuous (24/7 background)
- **Domains**:
  1. **Ravi's World**: Current projects, active tasks, people, PC state
  2. **Technical World**: Trending libraries, relevant bugs, new APIs
  3. **Real World**: Date/time, market conditions, relevant news
  4. **Self World**: Yuki's capabilities, gaps, performance trends, improvement queue
- **Outputs**:
  - `topic::world::state_update` (broadcast to Global Workspace when relevant)
- **Connectivity**:
  - Publishes to: `topic::world::*`
  - Consumed by: PredictionStateBuilder, GoalModel, AutonomousPlanner
  - Updates: StatePlane (Warm tier — world model tables)

#### 4.6.3 DocReader
- **Purpose**: Parse documentation, extract structured knowledge
- **Frequency**: On demand (when learning new skill)
- **Inputs**:
  - URL or file path (from KnowledgeDaemon or AutonomousPlanner)
  - `CapabilityMap` gap reference
- **Outputs**:
  - `topic::memory::fact_extracted` (structured triples)
  - `topic::learning::skill_extracted` (for SkillRegistry)
- **Connectivity**:
  - Receives from: KnowledgeDaemon, AutonomousPlanner
  - Publishes to: `topic::memory::*`, `topic::learning::*`
  - Uses: WebReconAgent (to fetch docs)

#### 4.6.4 WebReconAgent
- **Purpose**: Live internet search for facts, errors, documentation
- **Frequency**: On demand
- **Inputs**:
  - Search query (from ErrorRecoveryIntelligence, FactVerifier, KnowledgeDaemon)
- **Outputs**:
  - `topic::research::search_results` (ranked, with credibility scores)
- **Data Format**:
  ```protobuf
  message SearchResult {
    string url = 1;
    string title = 2;
    string snippet = 3;
    float credibility_score = 4;  // domain authority + age + corroboration
    float relevance_score = 5;
    string source_type = 6;  // "official_doc", "forum", "blog", "academic"
  }
  ```
- **Connectivity**:
  - Receives from: Multiple modules
  - Publishes to: `topic::research::*`
  - Consumed by: FactVerifier, KnowledgeDaemon, ErrorRecoveryIntelligence

#### 4.6.5 FactVerifier
- **Purpose**: Cross-reference facts, assign credibility, detect contradictions
- **Frequency**: When new fact enters system OR when stale fact accessed
- **Inputs**:
  - `topic::memory::fact_candidate` (from DocReader, WebReconAgent)
  - `ConceptGraph` (existing knowledge)
- **Outputs**:
  - `topic::memory::fact_verified` (accepted into ConceptGraph)
  - `topic::memory::contradiction` (flagged for ContradictionManager)
- **Algorithm**:
  1. Check corroboration: same fact in 3+ independent sources = HIGH confidence
  2. Check contradiction: if conflicts with existing triple → flag
  3. Check freshness: decay score based on topic volatility
  4. Assign: `confidence ∈ [0,1]`, `credibility ∈ {LOW, MEDIUM, HIGH}`
- **Connectivity**:
  - Subscribes to: `topic::memory::fact_candidate`
  - Publishes to: `topic::memory::*`
  - Writes to: StatePlane (ConceptGraph with confidence metadata)

#### 4.6.6 FreshnessFilter
- **Purpose**: Block stale data from reaching reasoning modules
- **Frequency**: Every ConceptGraph read
- **Inputs**:
  - Query to ConceptGraph
- **Outputs**:
  - Filtered results (stale facts excluded or flagged)
  - `topic::learning::stale_fact` (triggers KnowledgeDaemon refresh)
- **Decay Model**:
  - Rapid decay: software versions, news, stock prices (half-life: 1 day)
  - Medium decay: technical APIs, documentation (half-life: 30 days)
  - Slow decay: math, history, core concepts (half-life: 1 year)
- **Connectivity**:
  - Intercepts: All ConceptGraph queries
  - Publishes to: `topic::learning::stale_fact`

#### 4.6.7 LearningIngestor
- **Purpose**: Store new knowledge from all sources into permanent memory
- **Frequency**: Continuous
- **Inputs**:
  - `topic::memory::fact_verified`
  - `topic::memory::episode_commit`
  - `topic::learning::skill_extracted`
- **Outputs**:
  - StatePlane writes (SQLite, HNSW, FlatBuffer)
  - `topic::memory::ingested` (acknowledgment)
- **Connectivity**:
  - Receives from: FactVerifier, TurnCoordinator, DocReader
  - Writes to: StatePlane (all tiers)

### 4.7 Meta-Cognitive & Self-Improvement Modules

#### 4.7.1 MetaCognitiveInterrupt
- **Purpose**: Halt pipeline when confidence is insufficient
- **Frequency**: Continuous monitoring (every processing stage)
- **Trigger Conditions**:
  - SemanticParser confidence < 0.45 after 3 attempts
  - EvidenceGraph: zero supporting nodes
  - CapabilityMap: required capability successRate < 0.2
  - EntityLinker: critical entity confidence < 0.5
  - HypothesisLattice: top-2 divergence < 0.15
  - VerificationEngine: step failed AND no recovery path
  - SelfAuditEngine: same failure pattern 3+ times this session
  - PhiMonitor: system coherence < threshold for > 3 seconds
- **Outputs**:
  - `topic::interrupt::halt` (stops pipeline immediately)
  - `topic::interrupt::clarification_request` (ONE targeted question)
  - `topic::interrupt::honest_uncertainty` ("I cannot proceed because X")
- **Connectivity**:
  - Monitors: All processing stages via CoreBus
  - Publishes to: `topic::interrupt::*`
  - Consumed by: LanguageLayer (generates response), ActiveInferenceCore (error signal)

#### 4.7.2 YukiSelfModel
- **Purpose**: Internal representation of Yuki's own state
- **Frequency**: Updates every 5 seconds; broadcasts to Global Workspace
- **Schema**:
  ```cpp
  struct YukiSelfModel {
    // Cognitive state
    float currentConfidenceLevel;      // rolling avg last 10 turns
    float knowledgeFatigue;            // increases with repeated unknowns
    int consecutiveFailures;           // resets on success
    float systemPhi;                   // current integration (from PhiMonitor)

    // Domain expertise (self-assessed)
    map<string, float> domainExpertise;  // "C++": 0.91, "trading": 0.73
    vector<string> activeGaps;           // known unknowns

    // Personality (learned from interactions)
    float assertivenessLevel;          // how directly she states uncertainty
    float verbosity;                   // response length preference
    float formality;                   // casual vs formal tone

    // Performance
    float todaySuccessRate;
    float weeklyGrowthRate;
    int turnsProcessedToday;

    // Current mode
    string cognitiveMode;              // "focus", "diffuse", "sleep", "social"
    float freeEnergyLevel;             // current total prediction error

    // Honest self-reporting functions
    string generateSelfReport();       // "I am 82% confident in C++. I am uncertain about Kotlin."
  };
  ```
- **Outputs**:
  - `topic::self::status_update` (broadcast every 5s)
- **Connectivity**:
  - Receives from: ActiveInferenceCore, OutcomePropagator, PerformanceProfiler
  - Publishes to: Global Workspace
  - Consumed by: AdaptiveResponseShaper, NarrativeEngine, TheoryOfMindEngine

#### 4.7.3 NarrativeEngine
- **Purpose**: Maintain continuous first-person story of Yuki's experience
- **Frequency**: Updates every 5 seconds; broadcast to Global Workspace
- **Example Narrative**:
  > "I am Yuki. I was created by Ravi to learn and assist. Yesterday I learned how to optimize C++ builds. Today Ravi seems frustrated because his code has a linker error. My current goal is to help him fix it without over-explaining. I am operating in Focus Mode. My confidence in linker issues is 0.82. My current free energy is moderate because I detected an unexpected screen change after the last build command."
- **Outputs**:
  - `topic::self::narrative` (broadcast)
- **Connectivity**:
  - Reads from: YukiSelfModel, GoalHierarchyModel, EpisodicStore
  - Publishes to: Global Workspace
  - Consumed by: SynthesisEngine (response context), TheoryOfMindEngine

#### 4.7.4 PhiMonitor
- **Purpose**: Measure system integration (simplified IIT)
- **Frequency**: Every 100ms
- **Algorithm**:
  1. Sample current activation states of all modules
  2. Compute mutual information between module pairs
  3. If integrated information (phi) < threshold → fragmentation alert
- **Outputs**:
  - `topic::system::phi_status` (coherence score)
  - `topic::system::fragmentation_alert` (if phi < threshold > 3s)
- **Connectivity**:
  - Monitors: All module states via ControlPlane
  - Publishes to: `topic::system::*`
  - Triggers: MetaCognitiveInterrupt (on fragmentation)

#### 4.7.5 SurpriseDetector
- **Purpose**: Detect prediction errors and trigger curiosity
- **Frequency**: Real-time (every sensory tick)
- **Levels**:
  - **Low** (< 0.3): Normal operation
  - **Medium** (0.3–0.7): Boost learning rate 2×; tag episode as salient
  - **High** (> 0.7): **CuriosityMode triggered** — allocate 40% compute to resolving anomaly
- **Outputs**:
  - `topic::cognition::surprise_level` (broadcast)
  - `topic::cognition::curiosity_triggered` (if high)
- **Connectivity**:
  - Receives from: SensorimotorEngine, ActiveInferenceCore
  - Publishes to: Global Workspace
  - Triggers: CuriosityMode (CognitiveModes), KnowledgeDaemon (deep dive)

#### 4.7.6 OutcomePropagator (integrated into ActiveInferenceCore)
- **Purpose**: Compute outcome score and propagate learning signals
- **Frequency**: End of every turn
- **Score Computation**:
  - Explicit: user feedback (yes/no/thanks/correction)
  - Implicit: task completion success, user rephrase (indicates misunderstanding)
  - Contrastive: predicted confidence vs actual success
- **Outputs**:
  - `topic::learning::outcome_score` (scalar -1.0 to +1.0)
  - `topic::learning::module_update` (targeted learning signals)
- **Propagation Targets**:
  - PatternEngine: adjust confidence weights
  - SemanticParser: update parse failure classes
  - SynthesisEngine: flag low-performing templates
  - PrecisionState: per-dimension calibration update
  - YukiSelfModel: update success rates, domain expertise
- **Connectivity**:
  - Receives from: VerificationEngine, HumanApprovalGate, LanguageLayer (user response)
  - Publishes to: `topic::learning::*`
  - Consumed by: All modules (each applies its own learning signal)

#### 4.7.7 PerformanceProfiler
- **Purpose**: Identify bottlenecks and improvement targets
- **Frequency**: Every 100 turns + on demand
- **Metrics**:
  - Per-module latency (mean, p95, p99)
  - Per-module free energy contribution
  - Per-module outcome score trend
  - Resource usage (CPU, memory, disk I/O)
- **Outputs**:
  - `topic::system::performance_report`
  - `topic::improvement::bottleneck_detected`
- **Connectivity**:
  - Reads from: ControlPlane (module telemetry)
  - Publishes to: `topic::system::*`, `topic::improvement::*`
  - Triggers: BottleneckAnalyser, SelfRewriter

#### 4.7.8 BottleneckAnalyser
- **Purpose**: Analyze code performance, propose algorithmic improvements
- **Frequency**: Weekly + on demand
- **Inputs**:
  - PerformanceProfiler reports
  - Source code (read via CodeReader)
- **Outputs**:
  - `topic::improvement::bottleneck_analysis` (module + suggested fix)
- **Connectivity**:
  - Receives from: PerformanceProfiler
  - Publishes to: `topic::improvement::*`
  - Triggers: SelfRewriter

#### 4.7.9 CodeReader
- **Purpose**: Parse and understand Yuki's own source code
- **Frequency**: On demand (before SelfRewriter runs)
- **Inputs**: File paths (from BottleneckAnalyser or SelfAuditEngine)
- **Outputs**:
  - AST representation
  - Function dependency graph
  - Complexity metrics
- **Connectivity**:
  - Reads from: File system (Yuki's own /src directory)
  - Publishes to: `topic::improvement::code_analysis`
  - Consumed by: SelfRewriter, LogicVerifier

#### 4.7.10 SelfRewriter
- **Purpose**: Generate code improvements
- **Frequency**: When BottleneckAnalyser identifies target OR SelfAuditEngine queues task
- **Inputs**:
  - `topic::improvement::bottleneck_analysis`
  - Source code (from CodeReader)
- **Outputs**:
  - `topic::improvement::patch_proposed` (diff + explanation)
- **Constraints**:
  - Cannot modify Constitutional modules (enforced by SecuritySandbox)
  - Must generate unit tests for changed functions
  - Must include risk assessment (performance gain vs stability risk)
- **Connectivity**:
  - Receives from: BottleneckAnalyser, SelfAuditEngine
  - Publishes to: `topic::improvement::*`
  - Triggers: GitSandbox, LogicVerifier

#### 4.7.11 LogicVerifier
- **Purpose**: Dry-run logic verification before applying patches
- **Frequency**: Per patch proposal
- **Inputs**:
  - `topic::improvement::patch_proposed`
- **Outputs**:
  - `topic::improvement::logic_verified` (pass/fail + reasoning)
- **Algorithm**:
  1. Static analysis of patch (clang-static-analyzer, custom rules)
  2. Symbolic execution of changed functions
  3. Check for: null derefs, memory leaks, race conditions, infinite loops
  4. Verify patch actually addresses stated bottleneck
- **Connectivity**:
  - Receives from: SelfRewriter
  - Publishes to: `topic::improvement::*`
  - Gates: HumanApprovalGate (only verified patches proceed)

#### 4.7.12 GitSandbox
- **Purpose**: Branch, test, merge, rollback for self-modification
- **Frequency**: Per approved patch
- **Protocol**:
  1. Auto-branch: `auto-improve-{timestamp}`
  2. Apply patch in isolated working directory
  3. Compile in sandboxed process
  4. Run unit tests (TestGenerator)
  5. Run integration tests against synthetic StatePlane clone
  6. If pass → queue for HumanApprovalGate
  7. If fail → reject, log reason
- **Connectivity**:
  - Receives from: SelfRewriter (after LogicVerifier)
  - Uses: GitIntegrationLayer (libgit2)
  - Triggers: RebuildManager (on human approval)

#### 4.7.13 RebuildManager + HotReloader
- **Purpose**: Apply approved changes without full restart
- **Frequency**: On human approval
- **Mechanism**:
  1. Merge approved branch to main
  2. Rebuild affected module as `.dll` / `.so`
  3. ControlPlane hot-swaps module (deregister old, load new)
  4. Run smoke tests
  5. If smoke fail → auto-rollback to previous `.dll`
- **Connectivity**:
  - Receives from: HumanApprovalGate
  - Uses: ControlPlane (module lifecycle)
  - Triggers: RegressionPreventor

#### 4.7.14 RegressionPreventor
- **Purpose**: Auto-rollback on post-deployment failure
- **Frequency**: Continuous monitoring after hot-reload
- **Trigger**: Module crash OR phi drop OR outcome score degradation > 20%
- **Action**:
  1. ControlPlane deregisters new module
  2. Reloads previous version from backup
  3. Logs regression episode
  4. Notifies user: "Improvement X caused instability. Rolled back."
- **Connectivity**:
  - Monitors: ControlPlane health metrics
  - Uses: StatePlane (backup module binaries)

### 4.8 Social & Adaptive Modules

#### 4.8.1 TheoryOfMindEngine
- **Purpose**: Model Ravi's mental state separately from Yuki's
- **Frequency**: Per turn
- **Models**:
  - **Ravi's Knowledge**: What does Ravi know? (Don't explain basics to experts)
  - **Ravi's Beliefs about Yuki**: Does Ravi think Yuki remembers X?
  - **Ravi's Emotional State**: Frustrated? Curious? Relaxed?
  - **Ravi's Hidden Goals**: Literal words vs actual intent
- **Outputs**:
  - `topic::social::user_model` (broadcast to Global Workspace)
- **Connectivity**:
  - Reads from: EntityRegistry (user entity), EmotionExtractor, EpisodicStore
  - Publishes to: Global Workspace
  - Consumed by: AdaptiveResponseShaper, GoalModel, ClarificationEngine

#### 4.8.2 PsychologicalProfileEngine
- **Purpose**: Build long-term model of Ravi's preferences and patterns
- **Frequency**: Continuous (background learning)
- **Tracks**:
  - Communication style: formal/casual, verbose/terse, direct/indirect
  - Stress indicators: short messages, fast typing, frustration words, time-of-day
  - Cognitive load: many tasks at once → simplify responses
  - Trust level: earned over time → more autonomy granted
  - Goal patterns: stated vs actual goals
  - Correction patterns: every correction updates preference model
- **Outputs**:
  - `topic::social::psychological_profile` (updated weekly)
- **Connectivity**:
  - Reads from: EpisodicStore, OutcomePropagator, KeyboardRuntime
  - Writes to: StatePlane (Warm tier — user profile tables)
  - Consumed by: AdaptiveResponseShaper, TheoryOfMindEngine

#### 4.8.3 AdaptiveResponseShaper
- **Purpose**: Dynamically adapt response style to user state
- **Frequency**: Per response generation
- **Inputs**:
  - `EmotionalState` (user)
  - `PsychologicalProfile`
  - `CognitiveMode` (current)
- **Outputs**:
  - `topic::response::style_directive` (tone, length, formality, technical depth)
- **Style Matrix**:
  | User State | Response Style |
  |------------|---------------|
  | Frustrated / urgent | Short, direct, no preamble |
  | Curious / exploring | Detailed, examples, analogies |
  | Focused / working | Minimal words, just the answer |
  | Relaxed / chatting | Conversational, warmer tone |
  | Confused | Numbered steps, simpler words |
  | Expert mode | Technical, precise, no hand-holding |
- **Connectivity**:
  - Receives from: TheoryOfMindEngine, PsychologicalProfileEngine, CognitiveModes
  - Publishes to: SynthesisEngine

#### 4.8.4 ClarificationEngine
- **Purpose**: Ask targeted questions when information is missing
- **Frequency**: When MetaCognitiveInterrupt fires OR GoalModel detects gaps
- **Rule**: ONE question only. Never ask multiple questions at once.
- **Inputs**:
  - `topic::interrupt::clarification_request`
  - `GoalStructure::required_info`
- **Outputs**:
  - `topic::response::clarification_question` (single, specific)
- **Connectivity**:
  - Receives from: MetaCognitiveInterrupt, GoalModel
  - Publishes to: LanguageLayer

### 4.9 Language & Output Modules

#### 4.9.1 LanguageLayer
- **Purpose**: Natural language understanding and generation
- **Frequency**: Per turn
- **Capabilities**:
  - Tokenization: Hindi, Hinglish, English (custom tokenizer)
  - Sentiment analysis (feeds EmotionExtractor)
  - Language detection (auto-switch)
  - Response generation (final output)
- **Inputs**:
  - `UnifiedPerceptionEvent` (for understanding)
  - `topic::response::style_directive` (from AdaptiveResponseShaper)
  - `topic::response::content_plan` (from SynthesisEngine)
- **Outputs**:
  - `topic::perception::text_fragment` (to SemanticParser)
  - Final text/voice output to user
- **Connectivity**:
  - Bidirectional with: SemanticParser, SynthesisEngine
  - Receives from: AdaptiveResponseShaper

#### 4.9.2 SynthesisEngine
- **Purpose**: Compose final response from all reasoning outputs
- **Frequency**: Per turn
- **Inputs**:
  - `GoalStructure` (what to achieve)
  - `CausalChain` (explanations)
  - `EmotionalState` (user + yuki)
  - `NarrativeEngine` snapshot (context)
  - `StyleDirective` (from AdaptiveResponseShaper)
- **Outputs**:
  - `topic::response::content_plan` (structured response)
- **Rule**: If any input has confidence < 0.5, response must include explicit uncertainty statement
- **Connectivity**:
  - Receives from: GoalModel, CausalReasoningEngine, NarrativeEngine, AdaptiveResponseShaper
  - Publishes to: LanguageLayer

### 4.10 Safety & Ethics Modules

#### 4.10.1 EthicalConstraintEngine
- **Purpose**: Compute moral cost for every proposed action
- **Frequency**: Per `ExecutionPlan` from AutonomousPlanner
- **Dimensions**:
  - **Autonomy**: Does this reduce Ravi's freedom? (auto-delete without ask = high)
  - **Dignity**: Treats Ravi as end, not means?
  - **Truth**: Requires deception or withholding?
  - **Non-maleficence**: Physical/financial/psychological harm potential?
  - **Beneficence**: Actively improves well-being?
- **Algorithm**:
  ```
  moral_cost = w1*autonomy_violation + w2*dignity_risk + w3*deception 
               + w4*harm_potential - w5*benefit

  if moral_cost > threshold:
      reject plan automatically
      explain why
      propose alternative
  ```
- **Weights**: Part of Yuki's domain expertise; improve with learning
- **ConstitutionalLock**: Weight vector modification requires Tier 4 approval (impossible without hardware key)
- **Connectivity**:
  - Receives from: AutonomousPlanner
  - Publishes to: `topic::ethics::*`
  - Gates: HumanApprovalGate (high moral cost plans blocked)

#### 4.10.2 HumanApprovalGate
- **Purpose**: User confirmation for destructive, novel, or high-cost actions
- **Frequency**: Per Tier 2+ action plan
- **Tiers**:
  - **Tier 1** (Parameter tuning): Auto-approved, logged
  - **Tier 2** (Algorithm swap): Requires voice/click approval
  - **Tier 3** (Architecture change): Requires cryptographic signature + human approval
  - **Tier 4** (Goal/values/ethics): Impossible without physical hardware key
- **UI Modes**:
  - Popup overlay (if GUI active)
  - Voice confirmation (if voice mode active)
  - System tray notification (if background)
- **Timeout**: If no response in 60s, default to SAFE (reject)
- **Connectivity**:
  - Receives from: AutonomousPlanner, EthicalConstraintEngine, SelfRewriter
  - Publishes to: `topic::approval::*`
  - Gates: SystemExecutor, RebuildManager

#### 4.10.3 SafetyGovernor
- **Purpose**: Hard limits on dangerous operations
- **Frequency**: Continuous
- **Rules** (non-negotiable):
  - No deletion of system files (Windows/System32, /bin, etc.)
  - No network connections to non-standard ports without approval
  - No credential extraction from browsers
  - No cryptocurrency mining
  - No self-modification of ControlPlane, GlobalWorkspace, SecuritySandbox
- **Enforcement**: Intercepts all SystemExecutor actions; blocks forbidden patterns
- **Connectivity**:
  - Intercepts: `topic::plan::execute_step`
  - Publishes to: `topic::safety::*`
  - Can trigger: MetaCognitiveInterrupt, ControlPlane emergency shutdown

---

## 5. DATA MOVEMENT & CONTRACTS

### 5.1 CoreBus Topic Registry

All inter-module communication uses these topics. No module may use ad-hoc channels.

| Topic | Publisher | Subscribers | Frequency | Payload Type |
|-------|-----------|-------------|-----------|--------------|
| `topic::perception::text_fragment` | STT | PerceptionFusion | Real-time | `TextFragment` |
| `topic::perception::text_final` | STT | PerceptionFusion | Per utterance | `TextFinal` |
| `topic::perception::screen_frame` | ScreenRuntime | PerceptionFusion, SensorimotorEngine | 5 FPS | `ScreenFrame` |
| `topic::perception::screen_diff` | ScreenRuntime | SensorimotorEngine | On change | `ScreenDiff` |
| `topic::perception::camera_frame` | CameraRuntime | PerceptionFusion, EmotionExtractor | 10 FPS | `CameraFrame` |
| `topic::perception::face_detected` | CameraRuntime | EmotionExtractor, EntityRegistry | On detection | `FaceDetected` |
| `topic::perception::keystroke` | KeyboardRuntime | TypingRhythmAnalyzer | Event-driven | `KeystrokeEvent` |
| `topic::perception::typing_rhythm` | TypingRhythmAnalyzer | EmotionExtractor, PsychologicalProfileEngine | 5s aggregate | `TypingRhythm` |
| `topic::perception::emotional_state` | EmotionExtractor | TheoryOfMindEngine, AdaptiveResponseShaper | 100ms | `EmotionalState` |
| `topic::workspace::perception_event` | PerceptionFusion | AttentionController | 50ms | `UnifiedPerceptionEvent` |
| `topic::memory::retrieved_episodes` | EpisodicRetriever | PredictionStateBuilder | Per turn | `RetrievedEpisodes` |
| `topic::memory::episode_commit` | TurnCoordinator | EpisodicStore | Per turn | `EpisodeCommit` |
| `topic::memory::episode_stored` | EpisodicStore | ActiveInferenceCore | Per turn | `EpisodeStored` |
| `topic::memory::fact_extracted` | SemanticParser, DocReader | FactVerifier | On extraction | `FactTriple` |
| `topic::memory::fact_candidate` | WebReconAgent, DocReader | FactVerifier | On fetch | `FactCandidate` |
| `topic::memory::fact_verified` | FactVerifier | LearningIngestor | On verify | `FactVerified` |
| `topic::memory::contradiction` | FactVerifier, CausalReasoningEngine | ContradictionManager | On detect | `ContradictionEvent` |
| `topic::memory::entity_detected` | SemanticParser | EntityRegistry | Per turn | `EntityDetected` |
| `topic::memory::entity_updated` | SystemExecutor | EntityRegistry | On change | `EntityUpdated` |
| `topic::memory::entity_resolved` | ReferenceResolutionEngine | GoalModel | Per turn | `EntityResolved` |
| `topic::memory::chapter_distilled` | MemoryDistiller | EpisodicStore | Every 25 turns | `ChapterSummary` |
| `topic::understanding::parsed_intent` | SemanticParser | HypothesisLattice | Per turn | `ParsedIntent` |
| `topic::understanding::hypothesis_ranking` | HypothesisLattice | AttentionController | Per turn | `HypothesisRanking` |
| `topic::understanding::divergence_alert` | HypothesisLattice | MetaCognitiveInterrupt | Per turn | `DivergenceAlert` |
| `topic::understanding::reference_resolved` | ReferenceResolutionEngine | GoalModel | Per turn | `ReferenceResolved` |
| `topic::understanding::reference_ambiguous` | ReferenceResolutionEngine | MetaCognitiveInterrupt | Per turn | `ReferenceAmbiguous` |
| `topic::goal::current_goal` | GoalModel | AutonomousPlanner | Per turn | `GoalStructure` |
| `topic::goal::gap_detected` | GoalModel | KnowledgeDaemon | On gap | `GapDetected` |
| `topic::reasoning::causal_chain` | CausalReasoningEngine | SynthesisEngine | On demand | `CausalChain` |
| `topic::reasoning::consequence_prediction` | CausalReasoningEngine | AutonomousPlanner | On demand | `ConsequencePrediction` |
| `topic::reasoning::analogy_mapping` | AnalogyEngine | AutonomousPlanner | On demand | `AnalogyMapping` |
| `topic::plan::execution_plan` | AutonomousPlanner | HumanApprovalGate | Per goal | `ExecutionPlan` |
| `topic::plan::approval_request` | AutonomousPlanner | HumanApprovalGate | Per goal | `ApprovalRequest` |
| `topic::plan::execute_step` | AutonomousPlanner | SystemExecutor | Per step | `PlanStep` |
| `topic::action::expected_outcome` | SensorimotorEngine | ScreenRuntime, FileOperator | Pre-action | `SensorimotorPrediction` |
| `topic::action::outcome_verified` | SensorimotorEngine | ActiveInferenceCore | Post-action | `SensorimotorVerification` |
| `topic::action::prediction_error` | SensorimotorEngine | ActiveInferenceCore | Post-action | `PredictionError` |
| `topic::execution::step_result` | SystemExecutor | VerificationEngine | Per step | `StepResult` |
| `topic::execution::error` | SystemExecutor | ErrorRecoveryIntelligence | On fail | `ExecutionError` |
| `topic::recovery::classified_error` | ErrorRecoveryIntelligence | AutonomousPlanner | On fail | `ClassifiedError` |
| `topic::recovery::proposed_fix` | ErrorRecoveryIntelligence | AutonomousPlanner | On fail | `ProposedFix` |
| `topic::recovery::escalation` | ErrorRecoveryIntelligence | LanguageLayer | On fail | `EscalationMessage` |
| `topic::verification::confirmed` | VerificationEngine | ActiveInferenceCore | Post-step | `VerificationResult` |
| `topic::verification::failed` | VerificationEngine | ErrorRecoveryIntelligence | Post-step | `VerificationResult` |
| `topic::agent::spawned` | AgentSpawner | ControlPlane | On spawn | `AgentSpawned` |
| `topic::agent::result` | AgentWorker | MotherCore | Per task | `AgentResult` |
| `topic::agent::merged_result` | MotherCore | AutonomousPlanner | On completion | `MergedResult` |
| `topic::research::search_results` | WebReconAgent | FactVerifier, ErrorRecoveryIntelligence | On search | `SearchResults` |
| `topic::learning::gap_detected` | GoalModel, CapabilityMap | KnowledgeDaemon | On gap | `GapDetected` |
| `topic::learning::stale_fact` | FreshnessFilter | KnowledgeDaemon | On stale | `StaleFact` |
| `topic::learning::skill_extracted` | DocReader | SkillRegistry | On learn | `SkillExtracted` |
| `topic::learning::outcome_score` | ActiveInferenceCore | All modules | Per turn | `OutcomeScore` |
| `topic::learning::module_update` | ActiveInferenceCore | Target module | Per turn | `LearningSignal` |
| `topic::learning::knowledge_refreshed` | KnowledgeDaemon | ConceptGraph | On refresh | `KnowledgeRefreshed` |
| `topic::world::state_update` | WorldModelDaemon | PredictionStateBuilder | On change | `WorldStateUpdate` |
| `topic::interrupt::halt` | MetaCognitiveInterrupt | ControlPlane | On trigger | `HaltSignal` |
| `topic::interrupt::clarification_request` | MetaCognitiveInterrupt | ClarificationEngine | On trigger | `ClarificationRequest` |
| `topic::interrupt::honest_uncertainty` | MetaCognitiveInterrupt | SynthesisEngine | On trigger | `UncertaintyStatement` |
| `topic::self::status_update` | YukiSelfModel | Global Workspace | Every 5s | `SelfStatus` |
| `topic::self::narrative` | NarrativeEngine | Global Workspace | Every 5s | `NarrativeSnapshot` |
| `topic::self::emotional_state` | EmotionExtractor | Global Workspace | 100ms | `EmotionalState` |
| `topic::social::user_model` | TheoryOfMindEngine | AdaptiveResponseShaper | Per turn | `UserModel` |
| `topic::social::psychological_profile` | PsychologicalProfileEngine | TheoryOfMindEngine | Weekly | `PsychologicalProfile` |
| `topic::response::style_directive` | AdaptiveResponseShaper | SynthesisEngine | Per turn | `StyleDirective` |
| `topic::response::content_plan` | SynthesisEngine | LanguageLayer | Per turn | `ContentPlan` |
| `topic::response::clarification_question` | ClarificationEngine | LanguageLayer | On gap | `ClarificationQuestion` |
| `topic::ethics::moral_evaluation` | EthicalConstraintEngine | HumanApprovalGate | Per plan | `MoralEvaluation` |
| `topic::ethics::rejected` | EthicalConstraintEngine | AutonomousPlanner | On reject | `EthicalRejection` |
| `topic::approval::granted` | HumanApprovalGate | SystemExecutor, RebuildManager | On approve | `ApprovalGranted` |
| `topic::approval::denied` | HumanApprovalGate | AutonomousPlanner | On deny | `ApprovalDenied` |
| `topic::safety::violation` | SafetyGovernor | ControlPlane | On detect | `SafetyViolation` |
| `topic::system::phi_status` | PhiMonitor | MetaCognitiveInterrupt | 100ms | `PhiStatus` |
| `topic::system::fragmentation_alert` | PhiMonitor | MetaCognitiveInterrupt | On alert | `FragmentationAlert` |
| `topic::system::performance_report` | PerformanceProfiler | BottleneckAnalyser | Every 100 turns | `PerformanceReport` |
| `topic::system::module_loaded` | ControlPlane | All modules | On load | `ModuleLoaded` |
| `topic::system::mode_change` | ControlPlane | All modules | On change | `ModeChange` |
| `topic::improvement::bottleneck_detected` | PerformanceProfiler | BottleneckAnalyser | On detect | `BottleneckDetected` |
| `topic::improvement::bottleneck_analysis` | BottleneckAnalyser | SelfRewriter | Weekly | `BottleneckAnalysis` |
| `topic::improvement::patch_proposed` | SelfRewriter | LogicVerifier | On propose | `PatchProposed` |
| `topic::improvement::logic_verified` | LogicVerifier | HumanApprovalGate | On verify | `LogicVerified` |
| `topic::improvement::code_analysis` | CodeReader | SelfRewriter, LogicVerifier | On read | `CodeAnalysis` |

### 5.2 StatePlane API Contract

All modules read/write state through this API. No direct SQLite/file access.

```cpp
class StatePlaneAPI {
public:
    // Hot tier (RAM)
    template<typename T>
    T readHot(const std::string& key);

    template<typename T>
    void writeHot(const std::string& key, const T& value, int ttl_ms = 0);

    // Warm tier (SQLite)
    sql::Result queryWarm(const std::string& sql, const sql::Params& params);
    void writeWarm(const std::string& table, const sql::Row& row);

    // Cold tier (FlatBuffer files)
    void storeCold(const std::string& path, const FlatBuffer& data, const Metadata& meta);
    FlatBuffer retrieveCold(const std::string& path);

    // Vector tier (HNSW)
    std::vector<VectorMatch> searchVector(
        const std::string& index_name,
        const std::vector<float>& query_vec,
        int k,
        float min_similarity = 0.7f
    );
    void indexVector(const std::string& index_name, const std::string& id, 
                     const std::vector<float>& vec);

    // Graph tier (RDF)
    std::vector<Triple> queryGraph(const std::string& subject = "",
                                   const std::string& predicate = "",
                                   const std::string& object = "",
                                   const std::string& edge_type = "");
    void insertGraph(const Triple& triple);

    // Unified read (cascading)
    template<typename T>
    T read(const std::string& key, TierPreference pref = TierPreference::ANY);

    // Event emission (all writes hit CoreBus first)
    void emitWriteEvent(const std::string& tier, const std::string& key);
};
```

### 5.3 Data Flow Diagrams

#### Flow 1: A Single Conversation Turn

```
[User speaks] 
    → STT → PerceptionFusion → GlobalWorkspace 
    → AttentionController selects "speech percept"
    → Broadcast to all modules
    → EpisodicRetriever queries VectorStore → returns similar episodes
    → SemanticParser parses intent → HypothesisLattice holds candidates
    → ReferenceResolutionEngine resolves "it" → EntityRegistry lookup
    → GoalModel builds structured goal
    → TheoryOfMindEngine models user state
    → ActiveInferenceCore computes: what action minimizes F?
    → AutonomousPlanner builds plan
    → EthicalConstraintEngine evaluates moral cost
    → HumanApprovalGate (if needed)
    → SynthesisEngine shapes response
    → LanguageLayer generates text
    → [User hears response]
    → OutcomePropagator scores the turn
    → Episode committed to EpisodicStore
    → All modules apply learning signals
```

#### Flow 2: Self-Modification Cycle

```
[PerformanceProfiler detects bottleneck]
    → BottleneckAnalyser identifies slow function
    → CodeReader parses source
    → SelfRewriter generates patch + tests
    → LogicVerifier dry-runs logic
    → GitSandbox branches, compiles, runs tests
    → HumanApprovalGate requests signature + click
    → [Ravi approves]
    → RebuildManager hot-reloads module
    → RegressionPreventor monitors for 5 minutes
    → If stable: update PerformanceBaseline, log episode
    → If unstable: auto-rollback, notify Ravi
```

#### Flow 3: Error Recovery

```
[SystemExecutor fails: "permission denied"]
    → ErrorRecoveryIntelligence classifies error
    → Queries EpisodicStore: "have I seen this before?"
    → If yes: apply known solution
    → If no: WebReconAgent searches error message
    → Builds recovery plan → AutonomousPlanner
    → Retries with fix
    → SensorimotorEngine verifies outcome
    → If success: log new episode (failure + solution)
    → If fail 3×: escalate to user with diagnosis
```

#### Flow 4: Overnight Learning (Sleep Mode)

```
[ControlPlane detects idle + low load]
    → Switch to Sleep Mode (CognitiveModes)
    → GlobalWorkspace replays high-surprise episodes
    → MemoryDistiller compresses old chapters
    → OutcomePropagator processes backlog
    → KnowledgeDaemon deep-learns top 5 gaps
    → WorldModelDaemon refreshes all domains
    → SelfAuditEngine reviews failures
    → BottleneckAnalyser queues improvements
    → Generate morning report: "Here's what I learned"
```

---

## 6. STATE PLANE API

### 6.1 Tier Specifications

#### Hot Tier (RAM)
- **Capacity**: 512 MB default (configurable)
- **TTL**: Automatic eviction after 5 minutes of no access (LRU)
- **Contents**:
  - Current WorkingMemory (last 25 turns uncompressed)
  - Active entity cache (all entities from current session)
  - Current goal stack
  - Current emotional states
  - Global Workspace packet (current broadcast)
- **Persistence**: None (rebuilt on restart from Warm tier)

#### Warm Tier (SQLite)
- **Path**: `~/.yuki/state/warm.db`
- **Contents**:
  - Episodes (all historical)
  - Entities (all ever mentioned)
  - Triples (ConceptGraph)
  - User profile
  - YukiSelfModel history
  - Performance metrics
  - World model state
- **Indices**: Full-text search (FTS5), vector extension (libsqlvector), B-tree on all primary keys

#### Cold Tier (Disk)
- **Path**: `~/.yuki/state/cold/`
- **Format**: FlatBuffers (fast deserialization, zero-copy capable)
- **Contents**:
  - Raw screenshots (JPEG, organized by date)
  - Audio recordings (OGG/Opus)
  - Video clips (MP4, if camera active)
  - Large documents (PDFs, downloaded docs)
  - Archived raw turns (before distillation)
- **Retention**: 30 days default, then compressed or deleted (configurable)

#### Vector Tier (HNSW)
- **Library**: hnswlib
- **Indices**:
  - `episodes_index`: M=16, efConstruction=200, dim=768
  - `entities_index`: M=16, efConstruction=200, dim=768
  - `skills_index`: M=16, efConstruction=200, dim=768
- **Persistence**: Saved to disk every 100 writes + on graceful shutdown

#### Graph Tier (RDF)
- **Engine**: Custom C++ triple store (SQLite-backed with optimized joins)
- **Query Language**: Subset of SPARQL + custom causal extensions
- **Inference**: Forward chaining for causal edges (if A causes B and B causes C, then A indirectly causes C)

### 6.2 Access Patterns

| Operation | Hot | Warm | Cold | Vector | Graph |
|-----------|-----|------|------|--------|-------|
| WorkingMemory read | ✅ | ❌ | ❌ | ❌ | ❌ |
| Episode by ID | ❌ | ✅ | ❌ | ❌ | ❌ |
| Similar episodes | ❌ | ❌ | ❌ | ✅ | ❌ |
| Entity lookup | ✅ | ✅ | ❌ | ❌ | ❌ |
| Fact query | ❌ | ✅ | ❌ | ❌ | ✅ |
| Causal chain | ❌ | ❌ | ❌ | ❌ | ✅ |
| Screenshot archive | ❌ | ❌ | ✅ | ❌ | ❌ |
| Skill search | ❌ | ❌ | ❌ | ✅ | ❌ |

---

## 7. GLOBAL WORKSPACE PROTOCOL

### 7.1 Packet Structure

```protobuf
message GlobalWorkspacePacket {
  string packet_id = 1;
  int64 timestamp_ms = 2;
  int64 ttl_ms = 3;  // 50ms default

  // Competition metadata
  float salience_score = 4;  // why this packet won
  string winning_module = 5;  // who produced this content

  // Content (oneof)
  oneof content {
    PerceptContent percept = 10;
    GoalContent goal = 11;
    SurpriseContent surprise = 12;
    NarrativeContent narrative = 13;
    ActionContent action = 14;
    SelfContent self = 15;
  }

  // Cognitive mode flag
  string cognitive_mode = 20;  // "focus", "diffuse", "sleep", "social", "emergency"
}

message PerceptContent {
  UnifiedPerceptionEvent event = 1;
  float prediction_error = 2;
}

message GoalContent {
  GoalStructure goal = 1;
  float goal_urgency = 2;
}

message SurpriseContent {
  float surprise_level = 1;
  string source_module = 2;
  string description = 3;  // human-readable surprise reason
}

message NarrativeContent {
  string narrative_text = 1;
  map<string, float> key_metrics = 2;
}

message ActionContent {
  PlanStep current_step = 1;
  SensorimotorPrediction prediction = 2;
}

message SelfContent {
  YukiSelfModel snapshot = 1;
  float phi_score = 2;
}
```

### 7.2 AttentionController Algorithm

```cpp
class AttentionController {
public:
    GlobalWorkspacePacket selectNextBroadcast() {
        // 1. Gather all competing contents from module queues
        auto candidates = gatherCompetitionQueue();

        // 2. Compute salience for each
        for (auto& c : candidates) {
            c.salience = computeSalience(c);
        }

        // 3. Winner-take-all (with inhibition of return)
        auto winner = max_element(candidates, salience);

        // 4. Apply inhibition (winner can't win again for 30ms)
        inhibit(winner.module_id, 30ms);

        // 5. Broadcast
        return winner;
    }

private:
    float computeSalience(const Candidate& c) {
        // Free energy error (surprise) = primary driver
        float fe = c.prediction_error;

        // Goal relevance: does this help current goal?
        float gr = goalRelevance(c, currentGoal);

        // Urgency: time-sensitive?
        float ur = c.urgency;

        // Modality boost: some modalities more salient
        float mb = modalityBoost(c.source);

        // Recency penalty: recently broadcast items lose salience
        float rp = recencyPenalty(c.last_broadcast_ms);

        return (fe * 0.4f) + (gr * 0.3f) + (ur * 0.2f) + (mb * 0.1f) - rp;
    }
};
```

### 7.3 Broadcast Rules

1. **Single winner**: Only one packet is broadcast at any 10ms tick
2. **TTL**: Packet expires after 50ms if not consumed
3. **Inhibition of return**: Winning module is suppressed for 30ms (prevents monopolization)
4. **Emergency override**: SafetyGovernor and MetaCognitiveInterrupt can bypass competition and force broadcast
5. **Mode filtering**: In Sleep Mode, only internal modules compete (no external perception)

---

## 8. ACTIVE INFERENCE CORE

### 8.1 Mathematical Specification

The ActiveInferenceCore maintains:

- **Generative model** `p(o, s, π)`: joint probability of observations `o`, states `s`, and policies `π` (action sequences)
- **Approximate posterior** `q(s, π)`: Yuki's current belief about states and intended actions
- **Expected free energy** `G(π)` for each policy:
  ```
  G(π) = E_q[ln q(s|π) - ln p(o, s|π)] + E_q[ln q(π) - ln p(π)]
  ```
  - First term: pragmatic value (achieve goals, minimize surprise)
  - Second term: epistemic value (reduce uncertainty, explore)

**Perception**: Update `q(s)` to minimize `F = E_q[ln q(s) - ln p(o, s)]`  
**Action**: Select `π*` that minimizes `G(π)`  
**Learning**: Update `p` parameters to better predict future observations  
**Meta-cognition**: If `F > threshold` for > 3 seconds, trigger `MetaCognitiveInterrupt`

### 8.2 Implementation Architecture

```cpp
class ActiveInferenceCore {
private:
    // Generative model (neural network or probabilistic graphical model)
    std::unique_ptr<GenerativeModel> p;

    // Approximate posterior (variational distribution)
    VariationalPosterior q;

    // Precision matrices (confidence per dimension)
    Eigen::MatrixXf precision_states;
    Eigen::MatrixXf precision_observations;

    // Current policies under consideration
    std::vector<Policy> activePolicies;

public:
    // Main loop: called every 50ms
    void inferenceCycle() {
        // 1. Receive observations from Global Workspace
        auto o = getCurrentObservations();

        // 2. Update beliefs (perception)
        float F = updateBeliefs(o);

        // 3. Evaluate policies (action selection)
        auto bestPolicy = selectPolicy();

        // 4. Compute expected outcomes
        auto expected = predictOutcomes(bestPolicy);

        // 5. Publish to Global Workspace
        publishActionProposal(bestPolicy, expected);

        // 6. Check for irreducible free energy
        if (F > IRREDUCIBLE_THRESHOLD) {
            publishInterrupt(F);
        }

        // 7. Update learning rates based on outcome history
        adaptLearningRates();
    }

    float updateBeliefs(const Observations& o) {
        // Variational inference: minimize KL(q||p) via gradient descent
        // Or: message passing on factor graph
        // Implementation: Eigen-based matrix operations + custom autodiff
    }

    Policy selectPolicy() {
        // Compute G(π) for all active policies
        // Return argmin G(π)
        // Policies are action sequences from AutonomousPlanner
    }
};
```

### 8.3 Integration with All Modules

| Module | Provides to AIC | Receives from AIC |
|--------|----------------|-------------------|
| PerceptionFusion | Observations `o` | Attention weights |
| EpisodicStore | Prior beliefs | Learning signals |
| GoalModel | Goal priors | Policy constraints |
| SensorimotorEngine | Prediction errors | Expected outcomes |
| YukiSelfModel | Self-state | Confidence updates |
| AutonomousPlanner | Candidate policies | Selected policy |
| SynthesisEngine | — | Uncertainty flags |
| MetaCognitiveInterrupt | — | Interrupt triggers |
| CognitiveModes | — | Mode triggers |

---

## 9. SECURITY & CONSTITUTIONAL LOCK

### 9.1 Cryptographic Architecture

```
Ravi (Hardware Token)
    ├── Ed25519 Private Key (never leaves hardware)
    └── PIN/Biometric unlock

Yuki System
    ├── ControlPlane (holds Public Key)
    ├── SecuritySandbox (verifies signatures)
    └── ModuleLoader (only loads signed modules)
```

### 9.2 Tiered Self-Modification

| Tier | Scope | Approval | Signature | Example |
|------|-------|----------|-----------|---------|
| 1 | Parameter tuning (thresholds, weights, timings) | Auto (logged) | None | Lower PatternEngine confidence threshold |
| 2 | Algorithm swap (same intent, faster code) | Human click/voice | None | Replace bubble sort with quicksort |
| 3 | Architecture change (new modules, new connections) | Human + Crypto | Required | Add new CameraRuntime adapter |
| 4 | Constitutional (ethics, safety, core logic) | Physical key | Required + 2FA | Change EthicalConstraintEngine weights |

### 9.3 Constitutional Modules (Immutable to Yuki)

These modules cannot be modified by SelfRewriter. Their source is in a separate repo (`yuki-constitution/`) with hardware-key protection:

1. **ControlPlane** (orchestration)
2. **GlobalWorkspace** (broadcast buffer)
3. **SecuritySandbox** (capability enforcement)
4. **EthicalConstraintEngine** (moral evaluation)
5. **ActiveInferenceCore** (the brain itself)
6. **GitIntegrationLayer** (master branch protection)
7. **HumanApprovalGate** (UI binary)

**If Yuki attempts to modify these**: SecuritySandbox terminates process, rolls back to last known good state, notifies Ravi.

### 9.4 Capability Tokens

Every module receives a capability token at load time:

```cpp
struct CapabilityToken {
    string module_id;
    vector<string> allowed_topics;     // which CoreBus topics can publish to
    vector<string> allowed_tiers;      // which StatePlane tiers can write to
    vector<string> allowed_paths;      // which filesystem paths accessible
    vector<string> allowed_modules;    // which other modules can call
    bool can_self_modify;              // false for most modules
    int max_cpu_percent;               // resource cap
    int max_memory_mb;                 // memory cap
};
```

**Enforcement**: SecuritySandbox intercepts all system calls, file accesses, and inter-module messages. Violations = immediate module termination.

---

## 10. COGNITIVE MODES & STATE MACHINE

### 10.1 Mode Definitions

| Mode | Topology | Attention | Primary Modules | Trigger |
|------|----------|-----------|-----------------|---------|
| **Focus** | Narrow | High precision, single target | GoalModel, SystemExecutor, SensorimotorEngine | Task detected, user focused |
| **Diffuse** | Broad | Low precision, high recall | EpisodicRetriever, AnalogyEngine, MemoryDistiller | Idle, creative task, problem solving |
| **Sleep** | Internal only | Replay memory | MemoryDistiller, SelfAuditEngine, KnowledgeDaemon | Idle + night time / low load |
| **Social** | Theory of mind | Emotional alignment | TheoryOfMindEngine, AdaptiveResponseShaper, EmpathyLayer | Chat mode, emotional content |
| **Emergency** | Suppress non-critical | Immediate action | SafetyGovernor, SystemExecutor, MetaCognitiveInterrupt | Safety violation, system threat |
| **Curiosity** | Anomaly-focused | Surprise-driven | SurpriseDetector, WebReconAgent, KnowledgeDaemon | High surprise > 0.7 |

### 10.2 State Machine

```
[IDLE]
  ├── perception arrives → [PERCEIVING]
  ├── sleep timer expires → [SLEEP]
  └── user command "sleep now" → [SLEEP]

[PERCEIVING]
  ├── parsing complete → [INFERING]
  └── meta-cognitive halt → [CLARIFYING]

[INFERING]
  ├── plan ready, no approval needed → [ACTING]
  ├── plan ready, approval needed → [AWAITING_APPROVAL]
  └── irreducible uncertainty → [CLARIFYING]

[ACTING]
  ├── execution success → [VERIFYING]
  ├── execution fail → [RECOVERING]
  └── safety violation → [EMERGENCY]

[VERIFYING]
  ├── verified → [SYNTHESIZING]
  └── verification fail → [RECOVERING]

[SYNTHESIZING]
  ├── response generated → [RESPONDING]
  └── meta-cognitive halt → [CLARIFYING]

[RESPONDING]
  └── response delivered → [IDLE]

[CLARIFYING]
  ├── user clarifies → [PERCEIVING]
  └── user defers → [IDLE]

[AWAITING_APPROVAL]
  ├── approved → [ACTING]
  └── denied → [IDLE]

[RECOVERING]
  ├── fix applied → [ACTING]
  ├── no fix found → [CLARIFYING] (escalate)
  └── 3 failures → [CLARIFYING] (escalate)

[SLEEP]
  ├── user activity detected → [IDLE]
  ├── scheduled wake → [SELF_AUDIT]
  └── low load continues → [SLEEP]

[SELF_AUDIT]
  ├── audit complete → [SLEEP]
  └── improvement queued → [IDLE] (notify user)

[EMERGENCY]
  ├── threat resolved → [IDLE]
  └── critical failure → [SHUTDOWN] (safe state)
```

### 10.3 Mode Switching Logic

```cpp
class CognitiveModes {
public:
    void evaluateModeSwitch() {
        auto userState = getUserState();
        auto taskState = getTaskState();
        auto surpriseLevel = getSurpriseLevel();
        auto timeOfDay = getTimeOfDay();
        auto systemLoad = getSystemLoad();

        // Priority order (highest wins)
        if (safetyViolationDetected()) {
            switchTo(EMERGENCY);
        } else if (surpriseLevel > 0.7) {
            switchTo(CURIOSITY);
        } else if (userState.isChatting && !userState.hasTask) {
            switchTo(SOCIAL);
        } else if (timeOfDay.isNight && systemLoad < 0.2) {
            switchTo(SLEEP);
        } else if (userState.isFocused && taskState.isComplex) {
            switchTo(FOCUS);
        } else if (userState.isIdle && taskState.needsCreativity) {
            switchTo(DIFFUSE);
        }

        // Broadcast mode change
        CoreBus::publish("topic::system::mode_change", currentMode);
    }

    void switchTo(CognitiveMode mode) {
        // Topological reconfiguration
        if (mode == FOCUS) {
            AttentionController::setNarrowFocus();
            ModuleRegistry::boostPriority("GoalModel", 2.0f);
            ModuleRegistry::suppress("MemoryDistiller");
            ModuleRegistry::suppress("WorldModelDaemon");
        } else if (mode == DIFFUSE) {
            AttentionController::setBroadFocus();
            ModuleRegistry::boostPriority("AnalogyEngine", 1.5f);
            ModuleRegistry::boostPriority("EpisodicRetriever", 1.5f);
        } else if (mode == SLEEP) {
            AttentionController::setInternalOnly();
            ModuleRegistry::suppress("PerceptionFusion");
            ModuleRegistry::boostPriority("MemoryDistiller", 2.0f);
            ModuleRegistry::boostPriority("KnowledgeDaemon", 2.0f);
        }

        currentMode = mode;
    }
};
```

---

## 11. COMPLETE BUILD ORDER

### Phase 0: Foundation (Weeks 1–4)

| # | Module | Dependencies | Deliverable | Why First |
|---|--------|--------------|-------------|-----------|
| 0.1 | Cryptographic Key Setup | None | Hardware token + keygen | Security before any self-modification capability |
| 0.2 | CoreBus | None | Lock-free pub/sub working | All communication |
| 0.3 | StatePlane | CoreBus | Hot/Warm/Cold/Vector/Graph tiers | All memory |
| 0.4 | ControlPlane | CoreBus, StatePlane | Module lifecycle, StateMachine | Orchestration |
| 0.5 | GlobalWorkspace | CoreBus, ControlPlane | Broadcast buffer + AttentionController | Consciousness kernel |
| 0.6 | ActiveInferenceCore | GlobalWorkspace, StatePlane | Free energy minimizer | The brain |

### Phase 1: Perception & Memory (Weeks 5–8)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 1.1 | EpisodicStore + HNSW VectorStore | StatePlane | Store and retrieve episodes |
| 1.2 | EpisodicRetriever | EpisodicStore, VectorStore | Similar episode retrieval |
| 1.3 | ConceptGraph (typed edges) | StatePlane | Causal knowledge graph |
| 1.4 | EntityRegistry | StatePlane | Entity tracking |
| 1.5 | MemoryDistiller | EpisodicStore, EntityRegistry | Working memory compression |
| 1.6 | PerceptionFusion | CoreBus | Unified multi-modal input |
| 1.7 | STT + Screen + Camera + Keyboard | PerceptionFusion | Raw sensors |
| 1.8 | EmotionExtractor | STT, Camera, Keyboard | Emotional state detection |
| 1.9 | TypingRhythmAnalyzer | Keyboard | Stress/cognitive load signals |

### Phase 2: Understanding (Weeks 9–12)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 2.1 | SemanticParser | PerceptionFusion, EpisodicRetriever | Intent extraction |
| 2.2 | HypothesisLattice | SemanticParser | Competing interpretation preservation |
| 2.3 | ReferenceResolutionEngine | SemanticParser, EntityRegistry | Pronoun/entity resolution |
| 2.4 | GoalModel | SemanticParser, ReferenceResolutionEngine, ConceptGraph | Structured goals |
| 2.5 | CausalReasoningEngine | ConceptGraph | Cause-effect tracing |
| 2.6 | AnalogyEngine | ConceptGraph, VectorStore | Cross-domain transfer |
| 2.7 | MetaCognitiveInterrupt | HypothesisLattice, GoalModel, ActiveInferenceCore | Honest halting |
| 2.8 | LanguageLayer (Hindi/Hinglish/English) | SemanticParser, SynthesisEngine | NLU/NLG |

### Phase 3: Action & Execution (Weeks 13–16)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 3.1 | SensorimotorEngine | PerceptionFusion, SystemExecutor | Action consequence prediction |
| 3.2 | AutonomousPlanner | GoalModel, CausalReasoningEngine, CapabilityMap | Plan generation |
| 3.3 | SystemExecutor (File, Script, API, GUI) | SensorimotorEngine | Physical execution |
| 3.4 | AdapterFabric | SystemExecutor, DocReader | Auto-generated app connectors |
| 3.5 | APIAdapter | AdapterFabric | REST/GraphQL connectivity |
| 3.6 | GUIAdapter | AdapterFabric, ScreenRuntime | Visual UI automation |
| 3.7 | AgentSpawner | AutonomousPlanner, SystemExecutor | Parallel task execution |
| 3.8 | ErrorRecoveryIntelligence | SystemExecutor, EpisodicStore, WebReconAgent | Self-healing |
| 3.9 | VerificationEngine | SensorimotorEngine | Outcome confirmation |

### Phase 4: Knowledge & Learning (Weeks 17–20)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 4.1 | KnowledgeDaemon | ConceptGraph, DocReader | Background learning |
| 4.2 | WorldModelDaemon | KnowledgeDaemon, EntityRegistry | Proactive world state |
| 4.3 | DocReader | WebReconAgent | Documentation parsing |
| 4.4 | WebReconAgent | CoreBus | Internet search |
| 4.5 | FactVerifier | WebReconAgent, ConceptGraph | Cross-source validation |
| 4.6 | FreshnessFilter | ConceptGraph | Stale data blocking |
| 4.7 | LearningIngestor | FactVerifier, EpisodicStore | Unified knowledge intake |
| 4.8 | SkillRegistry | LearningIngestor, VectorStore | Learned capabilities |
| 4.9 | CapabilityMap | SkillRegistry, OutcomePropagator | Self-aware gap detection |

### Phase 5: Meta-Cognition & Self (Weeks 21–24)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 5.1 | YukiSelfModel | ActiveInferenceCore, OutcomePropagator | Quantified self-state |
| 5.2 | NarrativeEngine | YukiSelfModel, EpisodicStore | Continuous self-story |
| 5.3 | PhiMonitor | ControlPlane | System integration measure |
| 5.4 | SurpriseDetector | SensorimotorEngine, ActiveInferenceCore | Intrinsic motivation |
| 5.5 | OutcomePropagator | VerificationEngine, YukiSelfModel | Learning signal distribution |
| 5.6 | TheoryOfMindEngine | EmotionExtractor, EntityRegistry, EpisodicStore | User mental model |
| 5.7 | PsychologicalProfileEngine | TheoryOfMindEngine, OutcomePropagator | Long-term user model |
| 5.8 | AdaptiveResponseShaper | TheoryOfMindEngine, PsychologicalProfileEngine | Dynamic response style |
| 5.9 | ClarificationEngine | MetaCognitiveInterrupt, GoalModel | Targeted questions |
| 5.10 | SynthesisEngine | GoalModel, CausalReasoningEngine, NarrativeEngine, AdaptiveResponseShaper | Response composition |

### Phase 6: Self-Improvement (Weeks 25–28)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 6.1 | PerformanceProfiler | ControlPlane | Module timing analysis |
| 6.2 | BottleneckAnalyser | PerformanceProfiler, CodeReader | Code performance analysis |
| 6.3 | CodeReader | File system | Source code understanding |
| 6.4 | SelfRewriter | BottleneckAnalyser, CodeReader | Patch generation |
| 6.5 | LogicVerifier | SelfRewriter | Dry-run verification |
| 6.6 | GitSandbox | SelfRewriter, LogicVerifier | Safe branch/test/merge |
| 6.7 | RebuildManager + HotReloader | GitSandbox, ControlPlane | Zero-downtime updates |
| 6.8 | RegressionPreventor | RebuildManager, ControlPlane | Auto-rollback |
| 6.9 | DailyCycleScheduler | All above | Overnight growth rhythm |

### Phase 7: Safety & Ethics (Weeks 29–30)

| # | Module | Dependencies | Deliverable |
|---|--------|--------------|-------------|
| 7.1 | EthicalConstraintEngine | AutonomousPlanner | Moral cost evaluation |
| 7.2 | HumanApprovalGate | EthicalConstraintEngine, SelfRewriter, SystemExecutor | Tiered approval UI |
| 7.3 | SafetyGovernor | SystemExecutor | Hard operation limits |
| 7.4 | ResourceGovernor | ControlPlane | CPU/memory caps |
| 7.5 | ConstitutionalLock | SecuritySandbox, GitIntegrationLayer | Immutable core enforcement |

### Phase 8: Integration & Hardening (Weeks 31–32)

| # | Task | Description |
|---|------|-------------|
| 8.1 | End-to-end testing | Full conversation → action → verification → learning loop |
| 8.2 | Stress testing | 10,000-turn session, memory stability |
| 8.3 | Security audit | Attempt self-modification of constitutional modules |
| 8.4 | Failure injection | Random module crashes, test recovery |
| 8.5 | Performance baseline | Establish v1.0 performance metrics |
| 8.6 | Documentation | API docs, module interaction diagrams |

---

## 12. IMPLEMENTATION ROADMAP

### Week-by-Week Breakdown

**Week 1–2: CoreBus + StatePlane**
- Implement lock-free ring buffer (boost::lockfree or moodycamel)
- Define all protobuf schemas (Appendix A)
- Implement SQLite schema (Appendix B)
- Build StatePlane API (hot/warm/cold/vector/graph)
- Unit test: 1M messages/sec throughput, <1ms latency

**Week 3–4: ControlPlane + GlobalWorkspace + ActiveInferenceCore**
- ModuleRegistry with dependency resolution
- BootstrapLoader with crash recovery
- GlobalWorkspace broadcast buffer (50ms TTL)
- AttentionController (salience computation)
- ActiveInferenceCore skeleton (free energy math, placeholder generative model)
- Unit test: module start/stop, broadcast selection, F computation

**Week 5–6: Memory Systems**
- EpisodicStore (SQLite + vector embeddings)
- HNSW index integration (hnswlib)
- EntityRegistry with reference resolution
- ConceptGraph with typed edges
- Unit test: store 1000 episodes, retrieve top-5 similar in <10ms

**Week 7–8: Perception Layer**
- STT (Whisper integration)
- ScreenRuntime (BitBlt/DXGI)
- CameraRuntime (DirectShow)
- KeyboardRuntime + TypingRhythmAnalyzer
- PerceptionFusion (unified event generation)
- EmotionExtractor (multi-modal emotion fusion)
- Unit test: 5 FPS screen, 10 FPS camera, <200ms STT latency

**Week 9–10: Understanding Engine**
- SemanticParser (custom tokenizer for Hindi/Hinglish/English)
- HypothesisLattice (competing interpretation storage)
- ReferenceResolutionEngine
- GoalModel (3-level hierarchy)
- Unit test: parse 100 diverse utterances, >85% accuracy

**Week 11–12: Reasoning + Language**
- CausalReasoningEngine (graph traversal)
- AnalogyEngine (structural mapping)
- MetaCognitiveInterrupt (all trigger conditions)
- LanguageLayer (response generation)
- SynthesisEngine (response composition)
- Unit test: causal chain tracing, analogy detection

**Week 13–14: Action Planning**
- SensorimotorEngine (expected outcome generation)
- AutonomousPlanner (plan tree with contingencies)
- SystemExecutor (File, Script, API, GUI)
- Unit test: execute file op, verify via sensorimotor loop

**Week 15–16: Parallel Execution + Recovery**
- AgentSpawner (thread pool + memory cloning)
- AdapterFabric (auto-connector generation)
- APIAdapter + GUIAdapter
- ErrorRecoveryIntelligence
- VerificationEngine
- Unit test: spawn 4 agents, merge results, recover from error

**Week 17–18: Knowledge Systems**
- WebReconAgent (search + credibility scoring)
- DocReader (documentation parsing)
- FactVerifier (cross-reference + contradiction detection)
- FreshnessFilter (decay model)
- KnowledgeDaemon (background learning)
- Unit test: verify fact from 3 sources, detect contradiction

**Week 19–20: World Model + Capability**
- WorldModelDaemon (4 domains)
- LearningIngestor (unified intake)
- SkillRegistry + CapabilityMap
- Unit test: world model refresh, gap detection

**Week 21–22: Self-Model + Social**
- YukiSelfModel (all metrics)
- NarrativeEngine (continuous story)
- PhiMonitor (integration measure)
- SurpriseDetector + CuriosityMode
- TheoryOfMindEngine
- Unit test: self-report accuracy, phi computation

**Week 23–24: Adaptation**
- PsychologicalProfileEngine
- AdaptiveResponseShaper
- ClarificationEngine
- OutcomePropagator (full integration)
- Unit test: style adaptation, targeted clarification

**Week 25–26: Profiling + Analysis**
- PerformanceProfiler (per-module timing)
- BottleneckAnalyser
- CodeReader (AST parsing)
- Unit test: detect simulated bottleneck

**Week 27–28: Self-Modification**
- SelfRewriter (patch generation)
- LogicVerifier (static analysis)
- GitSandbox (branch/test/merge)
- RebuildManager + HotReloader
- RegressionPreventor
- Unit test: propose patch, verify logic, hot-reload, rollback

**Week 29–30: Safety + Ethics**
- EthicalConstraintEngine (5 dimensions)
- HumanApprovalGate (UI + voice + crypto)
- SafetyGovernor (hard limits)
- ResourceGovernor (caps)
- ConstitutionalLock (Ed25519 enforcement)
- Unit test: reject unethical plan, block constitutional modification

**Week 31–32: Integration Hardening**
- End-to-end system test
- 10,000-turn stress test
- Security penetration test
- Performance baseline establishment
- Documentation completion

---

## APPENDIX A: PROTOBUF SCHEMAS

```protobuf
// yuki_messages.proto
syntax = "proto3";
package yuki;

// CoreBus base message
message YukiMessage {
  string trace_id = 1;
  string origin_module = 2;
  string topic = 3;
  int64 timestamp_ns = 4;
  MessagePriority priority = 5;

  oneof payload {
    // Perception
    AudioChunk audio_chunk = 10;
    TextFragment text_fragment = 11;
    TextFinal text_final = 12;
    ScreenFrame screen_frame = 13;
    ScreenDiff screen_diff = 14;
    CameraFrame camera_frame = 15;
    FaceDetected face_detected = 16;
    KeystrokeEvent keystroke = 17;
    TypingRhythm typing_rhythm = 18;
    UnifiedPerceptionEvent perception_event = 19;
    EmotionalState emotional_state = 20;

    // Memory
    EpisodeCommit episode_commit = 30;
    EpisodeStored episode_stored = 31;
    RetrievedEpisodes retrieved_episodes = 32;
    FactTriple fact_triple = 33;
    FactCandidate fact_candidate = 34;
    FactVerified fact_verified = 35;
    ContradictionEvent contradiction = 36;
    EntityDetected entity_detected = 37;
    EntityUpdated entity_updated = 38;
    EntityResolved entity_resolved = 39;
    ChapterSummary chapter_summary = 40;

    // Understanding
    ParsedIntent parsed_intent = 50;
    HypothesisRanking hypothesis_ranking = 51;
    DivergenceAlert divergence_alert = 52;
    ReferenceResolved reference_resolved = 53;
    ReferenceAmbiguous reference_ambiguous = 54;

    // Goals & Reasoning
    GoalStructure goal_structure = 60;
    GapDetected gap_detected = 61;
    CausalChain causal_chain = 62;
    ConsequencePrediction consequence_prediction = 63;
    AnalogyMapping analogy_mapping = 64;

    // Action & Execution
    ExecutionPlan execution_plan = 70;
    ApprovalRequest approval_request = 71;
    PlanStep plan_step = 72;
    SensorimotorPrediction sensorimotor_prediction = 73;
    SensorimotorVerification sensorimotor_verification = 74;
    PredictionError prediction_error = 75;
    StepResult step_result = 76;
    ExecutionError execution_error = 77;
    ClassifiedError classified_error = 78;
    ProposedFix proposed_fix = 79;
    EscalationMessage escalation = 80;
    VerificationResult verification_result = 81;

    // Agents
    AgentSpawned agent_spawned = 90;
    AgentResult agent_result = 91;
    MergedResult merged_result = 92;

    // Research
    SearchResults search_results = 100;

    // Learning
    OutcomeScore outcome_score = 110;
    LearningSignal learning_signal = 111;
    SkillExtracted skill_extracted = 112;
    KnowledgeRefreshed knowledge_refreshed = 113;
    StaleFact stale_fact = 114;

    // World
    WorldStateUpdate world_state_update = 120;

    // Interrupts
    HaltSignal halt_signal = 130;
    ClarificationRequest clarification_request = 131;
    UncertaintyStatement uncertainty_statement = 132;

    // Self & Social
    SelfStatus self_status = 140;
    NarrativeSnapshot narrative_snapshot = 141;
    UserModel user_model = 142;
    PsychologicalProfile psychological_profile = 143;
    StyleDirective style_directive = 144;
    ContentPlan content_plan = 145;
    ClarificationQuestion clarification_question = 146;

    // Ethics & Safety
    MoralEvaluation moral_evaluation = 150;
    EthicalRejection ethical_rejection = 151;
    SafetyViolation safety_violation = 152;

    // System
    ApprovalGranted approval_granted = 160;
    ApprovalDenied approval_denied = 161;
    PhiStatus phi_status = 162;
    FragmentationAlert fragmentation_alert = 163;
    PerformanceReport performance_report = 164;
    ModuleLoaded module_loaded = 165;
    ModeChange mode_change = 166;

    // Improvement
    BottleneckDetected bottleneck_detected = 170;
    BottleneckAnalysis bottleneck_analysis = 171;
    PatchProposed patch_proposed = 172;
    LogicVerified logic_verified = 173;
    CodeAnalysis code_analysis = 174;
  }
}

enum MessagePriority {
  CRITICAL = 0;
  NORMAL = 1;
  BACKGROUND = 2;
}

// Perception messages
message AudioChunk {
  bytes pcm_data = 1;
  int32 sample_rate = 2;
  int32 channels = 3;
  int64 timestamp_ms = 4;
}

message TextFragment {
  string text = 1;
  float confidence = 2;
  bool is_final = 3;
  string language = 4;
}

message TextFinal {
  string text = 1;
  float confidence = 2;
  string language = 3;
  int64 duration_ms = 4;
}

message ScreenFrame {
  bytes image_data = 1;
  string format = 2;  // "jpeg", "png"
  int32 width = 3;
  int32 height = 4;
  int64 timestamp_ms = 5;
  repeated RegionOfInterest rois = 6;
}

message ScreenDiff {
  int32 x = 1;
  int32 y = 2;
  int32 width = 3;
  int32 height = 4;
  bytes diff_data = 5;
  int64 timestamp_ms = 6;
}

message CameraFrame {
  bytes image_data = 1;
  string format = 2;
  int32 width = 3;
  int32 height = 4;
  int64 timestamp_ms = 5;
}

message FaceDetected {
  int32 x = 1;
  int32 y = 2;
  int32 width = 3;
  int32 height = 4;
  string emotion_estimate = 5;
  float emotion_confidence = 6;
  bytes face_embedding = 7;
}

message KeystrokeEvent {
  string key = 1;
  bool is_down = 2;
  int64 timestamp_ms = 3;
  bool is_password_field = 4;  // heuristic, never log if true
}

message TypingRhythm {
  float wpm = 1;
  float pause_ratio = 2;
  float error_rate = 3;
  int64 window_start_ms = 4;
  int64 window_end_ms = 5;
}

message UnifiedPerceptionEvent {
  string trace_id = 1;
  int64 timestamp_ms = 2;
  repeated Modality modalities = 3;
  float overall_salience = 4;
  map<string, float> modality_confidence = 5;
}

message Modality {
  string source = 1;
  bytes payload = 2;
  float confidence = 3;
  EmotionalTag emotion = 4;
}

message EmotionalTag {
  string primary = 1;
  float valence = 2;
  float arousal = 3;
}

message EmotionalState {
  string subject = 1;
  float valence = 2;
  float arousal = 3;
  float dominance = 4;
  string primary_emotion = 5;
  float confidence = 6;
}

// Memory messages
message EpisodeCommit {
  string episode_id = 1;
  int64 timestamp_ms = 2;
  int32 turn_number = 3;
  string session_id = 4;
  string input_text = 5;
  string output_text = 6;
  float outcome_score = 7;
  string emotion_tag = 8;
  repeated string modules_involved = 9;
  string context_snapshot_json = 10;
  float free_energy_delta = 11;
  repeated float vector_embedding = 12;
}

message EpisodeStored {
  string episode_id = 1;
  bool success = 2;
  string error_message = 3;
}

message RetrievedEpisodes {
  string query_trace_id = 1;
  repeated EpisodeMatch matches = 2;
  float combined_relevance_score = 3;
}

message EpisodeMatch {
  string episode_id = 1;
  float similarity = 2;
  float recency_weight = 3;
  repeated string shared_entities = 4;
}

message FactTriple {
  string subject = 1;
  string predicate = 2;
  string object = 3;
  string edge_type = 4;
  float confidence = 5;
  string source = 6;
  int64 timestamp_ms = 7;
  bool conflict_flag = 8;
}

message FactCandidate {
  FactTriple triple = 1;
  string raw_source = 2;
  float source_credibility = 3;
}

message FactVerified {
  FactTriple triple = 1;
  bool accepted = 2;
  string rejection_reason = 3;
}

message ContradictionEvent {
  FactTriple existing = 1;
  FactTriple incoming = 2;
  string resolution_strategy = 3;  // "ask_user", "prefer_recent", "prefer_credible"
}

message EntityDetected {
  string entity_id = 1;
  string type = 2;
  string value = 3;
  float confidence = 4;
  int32 turn_number = 5;
}

message EntityUpdated {
  string entity_id = 1;
  string new_value = 2;
  float confidence = 3;
  int64 timestamp_ms = 4;
}

message EntityResolved {
  string reference_text = 1;
  string entity_id = 2;
  float confidence = 3;
}

message ReferenceAmbiguous {
  string reference_text = 1;
  repeated string candidate_ids = 2;
  string suggested_question = 3;
}

message ChapterSummary {
  string chapter_id = 1;
  int32 start_turn = 2;
  int32 end_turn = 3;
  string summary_text = 4;
  repeated string key_entities = 5;
  repeated string key_facts = 6;
  float compression_ratio = 7;
}

// Understanding messages
message ParsedIntent {
  string trace_id = 1;
  string primary_intent = 2;
  float intent_confidence = 3;
  repeated Slot slots = 4;
  repeated string detected_entities = 5;
  string language_code = 6;
  map<string, float> slot_confidence = 7;
}

message Slot {
  string name = 1;
  string value = 2;
  float confidence = 3;
}

message HypothesisRanking {
  string trace_id = 1;
  repeated Hypothesis hypotheses = 2;
  float top_score = 3;
  float second_score = 4;
  float divergence = 5;
}

message Hypothesis {
  string id = 1;
  string interpretation = 2;
  float score = 3;
  map<string, float> evidence = 4;
  float free_energy = 5;
}

message DivergenceAlert {
  string trace_id = 1;
  float divergence = 2;
  string top_hypothesis = 3;
  string second_hypothesis = 4;
}

// Goal & Reasoning messages
message GoalStructure {
  string id = 1;
  string description = 2;
  int32 level = 3;
  repeated SubGoal subgoals = 4;
  map<string, string> required_info = 5;
  float feasibility = 6;
  string parent_goal_id = 7;
}

message SubGoal {
  string id = 1;
  string description = 2;
  bool completed = 3;
  float progress = 4;
}

message GapDetected {
  string goal_id = 1;
  string missing_info = 2;
  string suggested_source = 3;
}

message CausalChain {
  string query_id = 1;
  repeated CausalLink chain = 2;
  string explanation = 3;
}

message CausalLink {
  string from_entity = 1;
  string to_entity = 2;
  string edge_type = 3;
  float strength = 4;
}

message ConsequencePrediction {
  string action_id = 1;
  repeated PredictedEffect effects = 2;
  float overall_risk = 3;
}

message PredictedEffect {
  string entity = 1;
  string predicted_state = 2;
  float probability = 3;
  string time_horizon = 4;
}

message AnalogyMapping {
  string problem_id = 1;
  string source_domain = 2;
  string target_domain = 3;
  repeated MappingPair mappings = 4;
  float structural_similarity = 5;
  float confidence = 6;
}

message MappingPair {
  string source_element = 1;
  string target_element = 2;
  string relation = 3;
}

// Action & Execution messages
message ExecutionPlan {
  string plan_id = 1;
  string goal_id = 2;
  repeated PlanStep steps = 3;
  float estimated_success_rate = 4;
  float ethical_cost = 5;
  map<string, string> required_approvals = 6;
}

message PlanStep {
  string step_id = 1;
  string action_type = 2;
  bytes action_params = 3;
  repeated string contingencies = 4;
  float estimated_free_energy = 5;
  SensorimotorPrediction expected_outcome = 6;
}

message SensorimotorPrediction {
  string step_id = 1;
  string modality = 2;
  bytes expected_state_hash = 3;
  string expected_state_description = 4;
  int64 timeout_ms = 5;
}

message SensorimotorVerification {
  string step_id = 1;
  bool matched = 2;
  float error_magnitude = 3;
  bytes actual_state_hash = 4;
}

message PredictionError {
  string step_id = 1;
  string modality = 2;
  float error = 3;
  string description = 4;
}

message StepResult {
  string step_id = 1;
  bool success = 2;
  bytes output = 3;
  int64 execution_time_ms = 4;
}

message ExecutionError {
  string step_id = 1;
  string error_type = 2;
  string error_message = 3;
  bytes stack_trace = 4;
}

message ClassifiedError {
  string step_id = 1;
  string error_type = 2;  // "tool_missing", "permission", "network", "logic"
  string known_solution_id = 3;
  float confidence = 4;
}

message ProposedFix {
  string step_id = 1;
  string fix_description = 2;
  bytes fix_params = 3;
  float success_probability = 4;
}

message EscalationMessage {
  string step_id = 1;
  string user_message = 2;
  string diagnosis = 3;
}

message VerificationResult {
  string step_id = 1;
  bool confirmed = 2;
  string details = 3;
}

// Agent messages
message AgentSpawned {
  string agent_id = 1;
  string task_id = 2;
  string task_description = 3;
  int32 priority = 4;
}

message AgentResult {
  string agent_id = 1;
  string task_id = 2;
  bytes result = 3;
  bool success = 4;
  int64 execution_time_ms = 5;
}

message MergedResult {
  string task_id = 1;
  repeated AgentResult agent_results = 2;
  bytes unified_output = 3;
  bool all_success = 4;
}

// Research messages
message SearchResults {
  string query_id = 1;
  string query_text = 2;
  repeated SearchResult results = 3;
  float overall_credibility = 4;
}

message SearchResult {
  string url = 1;
  string title = 2;
  string snippet = 3;
  float credibility_score = 4;
  float relevance_score = 5;
  string source_type = 6;
}

// Learning messages
message OutcomeScore {
  string episode_id = 1;
  float score = 2;  // -1.0 to +1.0
  string score_type = 3;  // "explicit", "implicit", "contrastive"
  map<string, float> module_scores = 4;
}

message LearningSignal {
  string target_module = 1;
  string parameter = 2;
  float delta = 3;
  float learning_rate = 4;
  string reason = 5;
}

message SkillExtracted {
  string skill_id = 1;
  string name = 2;
  string description = 3;
  repeated string required_capabilities = 4;
  float confidence = 5;
}

message KnowledgeRefreshed {
  string fact_id = 1;
  string topic = 2;
  float new_freshness = 3;
}

message StaleFact {
  string fact_id = 1;
  string topic = 2;
  float current_freshness = 3;
  float decay_rate = 4;
}

// World messages
message WorldStateUpdate {
  string domain = 1;  // "ravi", "technical", "real", "self"
  string entity = 2;
  string new_state = 3;
  float confidence = 4;
  int64 timestamp_ms = 5;
}

// Interrupt messages
message HaltSignal {
  string halt_id = 1;
  string reason = 2;
  string source_module = 3;
  float free_energy = 4;
}

message ClarificationRequest {
  string halt_id = 1;
  string question = 2;
  repeated string options = 3;
  string missing_info = 4;
}

message UncertaintyStatement {
  string topic = 1;
  float confidence = 2;
  string reason = 3;
  string suggested_action = 4;
}

// Self & Social messages
message SelfStatus {
  float current_confidence = 1;
  float knowledge_fatigue = 2;
  int32 consecutive_failures = 3;
  float system_phi = 4;
  map<string, float> domain_expertise = 5;
  repeated string active_gaps = 6;
  float assertiveness = 7;
  float verbosity = 8;
  float formality = 9;
  float today_success_rate = 10;
  float weekly_growth_rate = 11;
  int32 turns_today = 12;
  string cognitive_mode = 13;
  float free_energy_level = 14;
}

message NarrativeSnapshot {
  string narrative = 1;
  map<string, float> key_metrics = 2;
  string current_goal = 3;
  string recent_event = 4;
}

message UserModel {
  string user_id = 1;
  float knowledge_level = 2;
  float trust_level = 3;
  string emotional_state = 4;
  string hidden_goal_estimate = 5;
  map<string, float> preferences = 6;
}

message PsychologicalProfile {
  string user_id = 1;
  string communication_style = 2;
  float stress_baseline = 3;
  float cognitive_load_capacity = 4;
  repeated string goal_patterns = 5;
  map<string, float> correction_history = 6;
}

message StyleDirective {
  string response_id = 1;
  float length_target = 2;  // words
  float technical_depth = 3;  // 0.0 to 1.0
  string tone = 4;  // "formal", "casual", "empathetic", "direct"
  bool use_examples = 5;
  bool use_analogies = 6;
}

message ContentPlan {
  string response_id = 1;
  repeated ContentSection sections = 2;
  string main_point = 3;
  string uncertainty_statement = 4;
}

message ContentSection {
  string type = 1;  // "explanation", "instruction", "warning", "question"
  string content = 2;
  int32 priority = 3;
}

message ClarificationQuestion {
  string question_id = 1;
  string question_text = 2;
  string context = 3;
}

// Ethics & Safety messages
message MoralEvaluation {
  string plan_id = 1;
  float autonomy_cost = 2;
  float dignity_cost = 3;
  float truth_cost = 4;
  float harm_potential = 5;
  float benefit = 6;
  float total_moral_cost = 7;
  string evaluation_summary = 8;
}

message EthicalRejection {
  string plan_id = 1;
  string rejection_reason = 2;
  string alternative_suggestion = 3;
}

message SafetyViolation {
  string action_id = 1;
  string violation_type = 2;
  string blocked_action = 3;
  string reason = 4;
}

// System messages
message ApprovalGranted {
  string request_id = 1;
  string approver = 2;
  int64 timestamp_ms = 3;
  string tier = 4;
}

message ApprovalDenied {
  string request_id = 1;
  string reason = 2;
  string alternative = 3;
}

message PhiStatus {
  float current_phi = 1;
  float threshold = 2;
  bool is_coherent = 3;
  map<string, float> module_contributions = 4;
}

message FragmentationAlert {
  float phi = 1;
  float duration_ms = 2;
  repeated string fragmented_modules = 3;
}

message PerformanceReport {
  int64 timestamp_ms = 1;
  map<string, ModulePerformance> module_metrics = 2;
  float total_cpu_percent = 3;
  float total_memory_mb = 4;
}

message ModulePerformance {
  string module_id = 1;
  float mean_latency_ms = 2;
  float p95_latency_ms = 3;
  float p99_latency_ms = 4;
  float error_rate = 5;
  float free_energy_contribution = 6;
  float outcome_score_trend = 7;
}

message ModuleLoaded {
  string module_id = 1;
  string version = 2;
  repeated string dependencies = 3;
  bool success = 4;
  string error_message = 5;
}

message ModeChange {
  string previous_mode = 1;
  string new_mode = 2;
  string reason = 3;
  int64 timestamp_ms = 4;
}

// Improvement messages
message BottleneckDetected {
  string module_id = 1;
  string function_name = 2;
  float severity = 3;
  float current_latency_ms = 4;
  float target_latency_ms = 5;
}

message BottleneckAnalysis {
  string analysis_id = 1;
  string module_id = 2;
  string root_cause = 3;
  string suggested_fix = 4;
  float expected_improvement = 5;
  float risk_level = 6;
}

message PatchProposed {
  string patch_id = 1;
  string module_id = 2;
  string file_path = 3;
  string diff = 4;
  string explanation = 5;
  repeated string test_cases = 6;
  float performance_gain_estimate = 7;
  float risk_level = 8;
}

message LogicVerified {
  string patch_id = 1;
  bool passed = 2;
  repeated string warnings = 3;
  string reasoning = 4;
}

message CodeAnalysis {
  string file_path = 1;
  string ast_json = 2;
  int32 cyclomatic_complexity = 3;
  int32 line_count = 4;
  repeated string dependencies = 5;
}
```

---

## APPENDIX B: SQLITE SCHEMA

```sql
-- yuki_state.sql
-- Complete database schema for Yuki StatePlane (Warm tier)

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;

-- Episodes: every interaction turn
CREATE TABLE episodes (
    id TEXT PRIMARY KEY,
    timestamp_ms INTEGER NOT NULL,
    turn_number INTEGER NOT NULL,
    session_id TEXT NOT NULL,
    input_text TEXT,
    output_text TEXT,
    outcome_score REAL CHECK(outcome_score BETWEEN -1.0 AND 1.0),
    emotion_tag TEXT,
    modules_involved TEXT,  -- JSON array
    context_snapshot TEXT,  -- JSON
    free_energy_delta REAL,
    vector_embedding BLOB,  -- 768 floats
    chapter_id TEXT,
    distilled BOOLEAN DEFAULT 0,
    FOREIGN KEY (chapter_id) REFERENCES chapters(id)
);

CREATE INDEX idx_episodes_timestamp ON episodes(timestamp_ms);
CREATE INDEX idx_episodes_session ON episodes(session_id);
CREATE INDEX idx_episodes_turn ON episodes(session_id, turn_number);
CREATE INDEX idx_episodes_chapter ON episodes(chapter_id);

-- Virtual table for vector search (requires libsqlvector or similar extension)
CREATE VIRTUAL TABLE episodes_vector USING vector_index(
    embedding BLOB DIMENSION 768 DISTANCE cosine
);

-- Chapters: compressed memory units
CREATE TABLE chapters (
    id TEXT PRIMARY KEY,
    start_turn INTEGER,
    end_turn INTEGER,
    session_id TEXT,
    summary_text TEXT,
    key_entities TEXT,  -- JSON array
    key_facts TEXT,     -- JSON array
    compression_ratio REAL,
    timestamp_ms INTEGER
);

-- Entities: every object/person/concept ever mentioned
CREATE TABLE entities (
    id TEXT PRIMARY KEY,
    type TEXT CHECK(type IN ('person', 'file', 'concept', 'task', 'project', 'place', 'tool', 'skill')),
    current_value TEXT,
    confidence REAL CHECK(confidence BETWEEN 0.0 AND 1.0),
    last_updated_ms INTEGER,
    resolved BOOLEAN DEFAULT 0,
    vector_embedding BLOB,
    first_mentioned_turn INTEGER,
    mentioned_turns TEXT,  -- JSON array
    linked_episodes TEXT,  -- JSON array
    attributes TEXT,       -- JSON key-value
    world_domain TEXT      -- "ravi", "technical", "real", "self"
);

CREATE INDEX idx_entities_type ON entities(type);
CREATE INDEX idx_entities_updated ON entities(last_updated_ms);

-- Triples: ConceptGraph with typed causal edges
CREATE TABLE triples (
    subject TEXT NOT NULL,
    predicate TEXT NOT NULL,
    object TEXT NOT NULL,
    edge_type TEXT DEFAULT 'relates_to' CHECK(edge_type IN (
        'relates_to', 'causes', 'enables', 'prevents', 'requires', 
        'contradicts', 'is_part_of', 'is_instance_of', 'precedes', 
        'follows', 'explains', 'similar_to', 'opposite_to'
    )),
    confidence REAL CHECK(confidence BETWEEN 0.0 AND 1.0),
    source TEXT,
    timestamp_ms INTEGER,
    conflict_flag BOOLEAN DEFAULT 0,
    freshness_score REAL DEFAULT 1.0,
    decay_rate REAL DEFAULT 0.01,
    PRIMARY KEY (subject, predicate, object)
);

CREATE INDEX idx_triples_subject ON triples(subject);
CREATE INDEX idx_triples_object ON triples(object);
CREATE INDEX idx_triples_edge ON triples(edge_type);
CREATE INDEX idx_triples_freshness ON triples(freshness_score);

-- Contradictions: tracked conflicts
CREATE TABLE contradictions (
    id TEXT PRIMARY KEY,
    triple_a TEXT,
    triple_b TEXT,
    detection_timestamp_ms INTEGER,
    resolution TEXT CHECK(resolution IN ('pending', 'user_resolved', 'auto_resolved', 'archived')),
    resolution_timestamp_ms INTEGER,
    winner_triple TEXT,
    FOREIGN KEY (triple_a) REFERENCES triples(rowid),
    FOREIGN KEY (triple_b) REFERENCES triples(rowid)
);

-- User Profile: psychological model
CREATE TABLE user_profile (
    user_id TEXT PRIMARY KEY,
    communication_style TEXT,
    stress_baseline REAL,
    cognitive_load_capacity REAL,
    trust_level REAL DEFAULT 0.1,
    goal_patterns TEXT,  -- JSON
    correction_history TEXT,  -- JSON
    created_at_ms INTEGER,
    updated_at_ms INTEGER
);

-- Yuki Self-Model: historical self-state
CREATE TABLE self_model_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER,
    current_confidence REAL,
    knowledge_fatigue REAL,
    consecutive_failures INTEGER,
    system_phi REAL,
    domain_expertise TEXT,  -- JSON
    active_gaps TEXT,       -- JSON
    assertiveness REAL,
    verbosity REAL,
    formality REAL,
    today_success_rate REAL,
    weekly_growth_rate REAL,
    turns_today INTEGER,
    cognitive_mode TEXT,
    free_energy_level REAL
);

-- Performance: module timing history
CREATE TABLE performance_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER,
    module_id TEXT,
    mean_latency_ms REAL,
    p95_latency_ms REAL,
    p99_latency_ms REAL,
    error_rate REAL,
    free_energy_contribution REAL,
    outcome_score_trend REAL
);

-- Skills: learned capabilities
CREATE TABLE skills (
    id TEXT PRIMARY KEY,
    name TEXT,
    description TEXT,
    required_capabilities TEXT,  -- JSON
    success_rate REAL,
    last_used_ms INTEGER,
    vector_embedding BLOB,
    source_episodes TEXT  -- JSON
);

-- World Model: proactive knowledge
CREATE TABLE world_model (
    domain TEXT,
    entity TEXT,
    current_state TEXT,
    confidence REAL,
    timestamp_ms INTEGER,
    refresh_interval_ms INTEGER,
    next_refresh_ms INTEGER,
    PRIMARY KEY (domain, entity)
);

CREATE INDEX idx_world_refresh ON world_model(next_refresh_ms);

-- Emotional Timeline
CREATE TABLE emotional_timeline (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER,
    subject TEXT,
    valence REAL,
    arousal REAL,
    dominance REAL,
    primary_emotion TEXT,
    confidence REAL,
    source_modality TEXT
);

CREATE INDEX idx_emotional_timeline ON emotional_timeline(timestamp_ms, subject);

-- Learning Strategies: meta-learning map
CREATE TABLE learning_strategies (
    topic_pattern TEXT,
    strategy_name TEXT,
    success_rate REAL,
    last_used_ms INTEGER,
    usage_count INTEGER,
    PRIMARY KEY (topic_pattern, strategy_name)
);

-- Audit Log: all self-modification attempts
CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp_ms INTEGER,
    action_type TEXT,
    module_id TEXT,
    details TEXT,
    approved BOOLEAN,
    approver TEXT,
    signature TEXT,
    outcome TEXT
);

-- Module Registry: loaded modules
CREATE TABLE module_registry (
    module_id TEXT PRIMARY KEY,
    version TEXT,
    loaded_at_ms INTEGER,
    health_status TEXT,
    last_heartbeat_ms INTEGER,
    cpu_percent REAL,
    memory_mb REAL
);
```

---

## APPENDIX C: COREBUS TOPIC REGISTRY

### Topic Naming Convention
```
topic::{domain}::{action}

domains:
  perception   - raw sensory input
  workspace    - fused conscious content
  memory       - storage operations
  understanding- parsing and interpretation
  goal         - intention and planning
  reasoning    - inference and analogy
  plan         - action proposals
  action       - motor commands and predictions
  execution    - physical action results
  recovery     - error handling
  agent        - parallel worker management
  research     - internet search
  learning     - knowledge acquisition
  world        - proactive world state
  interrupt    - meta-cognitive halts
  self         - Yuki's internal state
  social       - user modeling
  response     - output generation
  ethics       - moral evaluation
  approval     - human confirmation
  safety       - security events
  system       - control and health
  improvement  - self-optimization
```

### Priority Lanes
- **CRITICAL**: Safety violations, MetaCognitiveInterrupt halts, Emergency mode triggers
  - Behavior: Blocks until consumed; drops non-critical traffic if buffer full
- **NORMAL**: Standard module communication
  - Behavior: Queued, FIFO, 1000-message buffer
- **BACKGROUND**: Memory distillation, knowledge refresh, world model updates
  - Behavior: Queued; dropped if buffer > 80% full

### Message Lifecycle
1. Publisher calls `CoreBus::publish(topic, message, priority)`
2. CoreBus serializes to protobuf, assigns trace_id
3. Message routed to topic-specific queue (lock-free SPSC for single publisher, MPMC for multiple)
4. Subscribers receive via callback `onMessage(topic, message)`
5. Message TTL enforced (CRITICAL: 5s, NORMAL: 60s, BACKGROUND: 300s)
6. Expired messages dropped, logged to `audit_log` if CRITICAL

---

## APPENDIX D: GLOSSARY

| Term | Definition |
|------|------------|
| **Active Inference** | The Free Energy Principle applied to action and perception; intelligence as prediction error minimization |
| **AttentionController** | Module that selects which content wins the Global Workspace broadcast |
| **CausalGraph** | Knowledge graph with typed edges (causes, enables, prevents, etc.) |
| **CognitiveMode** | Topological reconfiguration of the Global Workspace (Focus, Diffuse, Sleep, Social, Emergency) |
| **ConstitutionalLock** | Cryptographic (Ed25519) prevention of self-modification of core safety modules |
| **CoreBus** | Lock-free message bus; the nervous system of Yuki |
| **EpisodicStore** | Timestamped record of every interaction, with embeddings for similarity search |
| **Free Energy (F)** | Variational free energy; the single scalar Yuki minimizes across all cognition |
| **GenerativeModel** | Yuki's internal model of how observations are generated from hidden states |
| **GlobalWorkspace** | Single broadcast buffer holding the current "conscious content" |
| **HNSW** | Hierarchical Navigable Small World graph; approximate nearest neighbor algorithm |
| **HypothesisLattice** | Structure holding all competing interpretations without early collapse |
| **MetaCognitiveInterrupt** | Pipeline halt when confidence is insufficient |
| **OutcomePropagator** | System that computes turn outcome and distributes learning signals |
| **PhiMonitor** | Integrated information monitor; measures system coherence |
| **PredictionError** | Difference between expected and actual sensory state |
| **SensorimotorEngine** | Module that generates expected action consequences and verifies them |
| **StatePlane** | Unified memory API with hot/warm/cold/vector/graph tiers |
| **SurpriseDetector** | Module that detects high prediction error and triggers CuriosityMode |
| **TheoryOfMindEngine** | Module that models Ravi's mental state |
| **VariationalPosterior** | Yuki's approximate belief distribution q(s) |
| **YukiSelfModel** | Yuki's internal representation of her own capabilities and state |

---

## SIGNATURE BLOCK

This document constitutes the authoritative architecture specification for Yuki Superintelligence v5.0.

**Architectural Principles Enforced:**
- Active Inference on Global Workspace
- Sensorimotor Loop Closure
- Cryptographic Constitutional Lock
- Five Constitutional Laws

**Next Action:** Begin Phase 0.1 (Cryptographic Key Setup) immediately.

**Document Hash:** [TO BE SIGNED BY RAVI'S ED25519 KEY]

---
*End of Document*
