# YUKI 1.0 — COMPLETE PROJECT ARCHITECTURE & FLOW MAPPING REPORT

This report provides a detailed, comprehensive mapping of the Yuki 1.0 codebase. It details the directory structure, the initialization and boot flow, the real-time perception pipeline, the cognitive reasoning engine, active inference subsystems, and background consolidation mechanisms.

---

## 1. High-Level Architectural Flow

Below is the complete cognitive pipeline of Yuki, mapping how raw environmental signals flow from sensors to execution and response, interacting with memory stores and active inference loops.

```mermaid
flowchart TD
    subgraph Input Perception (Real-time Runtimes)
        AudioInput["AudioInputRuntime (waveIn PCM)"]
        CameraInput["CameraRuntime (DirectShow)"]
        ScreenInput["ScreenRuntime (BitBlt Screens/Procs)"]
    end

    subgraph Signal Conditioning Layer (SCL)
        SCL["SignalConditioningLayer"]
        Filter["ArtifactFilter (Noise cleaning)"]
        Norm["SignalNormalizer (Calibration)"]
        Align["TemporalAligner & ChangeDetector"]
    end

    subgraph Messaging & Routing
        PE["PerceptionEvent (Normalized Payload)"]
        GW["GlobalWorkspace (Lock-free CoreBus)"]
        Router["CommandRouter"]
    end

    subgraph Execution Subsystems (Action)
        SysExec["SystemExecutor / ScriptRunner"]
        FileOp["FileOperator / ToolExecutor"]
        UIAuto["UIAutomationController"]
    end

    subgraph Cognitive Core (BabyMode)
        Baby["BabyMode Gateway"]
        TC["TurnCoordinator (Streams Coordinator)"]
        E1["E1FastStream (Symbolic & Reflexes)"]
        E2["E2SemanticStream (Associative Context)"]
        E3["E3DeepStream (Generative & Policy Search)"]
    end

    subgraph Active Inference Engine (VSE)
        VSE["VariationalStateEstimator"]
        Belief["BeliefState (24 Factorized States)"]
        FE["FreeEnergyCalculator"]
        GenModel["GenerativeModel (Bootstrap Data)"]
        Policy["PolicySelector (Active Inference)"]
    end

    subgraph Memory Fabric (CMF)
        CMF["CognitiveMemoryFabric"]
        HdcGraph["HdcSemanticGraph (HDC Hypervectors)"]
        Episodic["EpisodicStore (SQLite Turns)"]
        UserMem["UserMemory (Personal Facts)"]
    end

    subgraph Output Generation (Synthesis)
        Synthesizer["SynthesisEngine"]
        LLM["LocalLLM (Ollama Wrapper)"]
        Mouth["AudioOutputRuntime (SAPI Speaks)"]
        Visuals["Win32 Shell & DetailView UI"]
    end

    %% Input to SCL
    AudioInput --> SCL
    CameraInput --> SCL
    ScreenInput --> SCL
    SCL --> Filter
    Filter --> Norm
    Norm --> Align
    Align --> PE

    %% Perception to Routing
    PE --> GW
    GW --> Router
    Router -->|If Action Command| SysExec
    Router -->|If Action Command| FileOp
    Router -->|If Action Command| UIAuto
    Router -->|If Chat/Observation| Baby

    %% Reasoning Pipeline
    Baby --> TC
    TC --> E1
    TC --> E2
    TC --> E3

    %% Stream Connections
    E2 <--> MemoryFabric[(Memory Fabric CMF)]
    E3 <--> ActiveInference[(Active Inference VSE)]

    %% Memory Details
    MemoryFabric === CMF
    CMF --- HdcGraph
    CMF --- Episodic
    CMF --- UserMem

    %% Active Inference Details
    ActiveInference === VSE
    VSE --- Belief
    VSE --- FE
    VSE --- GenModel
    VSE --- Policy

    %% Action & Response Output
    TC --> Synthesizer
    Synthesizer --> LLM
    LLM --> Mouth
    LLM --> Visuals
    SysExec --> GW
```

---

## 2. Directory Mapping & File Layout

Yuki contains **286 source files** (totaling **60,847 LOC**), **17 unit test files**, **6 scripts**, and system documentation. Below is the comprehensive listing of directories and files.

### 2.1 Root Config & Support Files
*   [`CMakeLists.txt`](file:///d:/Yuki_1.0/CMakeLists.txt) (19,556 bytes): Defines compiler options (C++17, MSVC flags, GDI+, SAPI, Winsock libs) and builds target `yuki.exe` and test suites.
*   [`build_and_log.ps1`](file:///d:/Yuki_1.0/build_and_log.ps1) (1,464 bytes): Compiles Yuki in Release, runs tests, and logs outcome to `status.md`.
*   [`build_and_run.ps1`](file:///d:/Yuki_1.0/build_and_run.ps1) (2,039 bytes): Automates the clean rebuild and runs the core executable.
*   [`log_status.ps1`](file:///d:/Yuki_1.0/log_status.ps1) (3,022 bytes): Checks CTest validation rules and writes status gates to markdown.
*   [`status.md`](file:///d:/Yuki_1.0/status.md) (8,903 bytes): Diagnostic register indicating passing and failing development gates.

---

### 2.2 Core Application & Presence UI (`src/`)
Located at [`src/`](file:///d:/Yuki_1.0/src). Manages the main thread lifecycle and GDI+ rendering views.

*   [`main.cpp`](file:///d:/Yuki_1.0/src/main.cpp) (19,971 bytes | 483 LOC): Orchestrates system boot, database connection, module registration, thread creation, SCL binding, STT callbacks, SAPI setup, terminal polling loop, and graceful shutdown.
*   [`BabyMode.cpp`](file:///d:/Yuki_1.0/src/BabyMode.cpp) / [`BabyMode.h`](file:///d:/Yuki_1.0/src/BabyMode.h): Core gateway wrapper. Owns sensory runtimes, delegates text processing to the turn engine, and coordinates physical subsystem toggles.
*   [`PresenceShell.cpp`](file:///d:/Yuki_1.0/src/PresenceShell.cpp) / [`PresenceShell.h`](file:///d:/Yuki_1.0/src/PresenceShell.h): Win32 native transparent overlay. Renders system stats, subsystem buttons (toggling Ear, Mouth, Cam, Screen), and handles keyboard hooks.
*   [`DetailView.cpp`](file:///d:/Yuki_1.0/src/DetailView.cpp) / [`DetailView.h`](file:///d:/Yuki_1.0/src/DetailView.h): Sidebar containing processing history, terminal traces, and detailed logs.
*   [`AvatarBody.cpp`](file:///d:/Yuki_1.0/src/AvatarBody.cpp) / [`AvatarBody.h`](file:///d:/Yuki_1.0/src/AvatarBody.h): Tracks animation states (SPEAKING, LISTENING, THINKING, IDLE) and drives face shapes.
*   [`AvatarRenderer.cpp`](file:///d:/Yuki_1.0/src/AvatarRenderer.cpp) / [`AvatarRenderer.h`](file:///d:/Yuki_1.0/src/AvatarRenderer.h): Native GDI+ drawing engine implementing animations and transparent gradients.
*   [`SubsystemControl.cpp`](file:///d:/Yuki_1.0/src/SubsystemControl.cpp) / [`SubsystemControl.h`](file:///d:/Yuki_1.0/src/SubsystemControl.h): Gathers thread status queries and routes toggle request signals.
*   [`CommandRouter.cpp`](file:///d:/Yuki_1.0/src/CommandRouter.cpp) / [`CommandRouter.h`](file:///d:/Yuki_1.0/src/CommandRouter.h): Filters system command prompts (e.g., execution directives) from conversational observations.
*   [`IntentScorer.cpp`](file:///d:/Yuki_1.0/src/IntentScorer.cpp) / [`IntentScorer.h`](file:///d:/Yuki_1.0/src/IntentScorer.h) and [`ResponseEngine.cpp`](file:///d:/Yuki_1.0/src/ResponseEngine.cpp) / [`ResponseEngine.h`](file:///d:/Yuki_1.0/src/ResponseEngine.h): Part of the fallback neural spine; scores intent mapping and handles templates.
*   [`AutoSensor.cpp`](file:///d:/Yuki_1.0/src/AutoSensor.cpp) / [`AutoSensor.h`](file:///d:/Yuki_1.0/src/AutoSensor.h): Simple utility that boots all active hardware on application launch.
*   [`YukiUtils.cpp`](file:///d:/Yuki_1.0/src/YukiUtils.cpp) / [`YukiUtils.h`](file:///d:/Yuki_1.0/src/YukiUtils.h): Feature flags loader and execution checkpoint tracer.

---

### 2.3 Sensor Perception & Input modality (`src/input/`)
Located at [`src/input/`](file:///d:/Yuki_1.0/src/input). Manages low-level Windows SDK capture APIs.

*   [`AudioInputRuntime.cpp`](file:///d:/Yuki_1.0/src/input/AudioInputRuntime.cpp) / [`AudioInputRuntime.h`](file:///d:/Yuki_1.0/src/input/AudioInputRuntime.h): Windows waveIn double-buffered audio capture capture thread. Falls back to silent noise simulation.
*   [`AudioOutputRuntime.cpp`](file:///d:/Yuki_1.0/src/input/AudioOutputRuntime.cpp) / [`AudioOutputRuntime.h`](file:///d:/Yuki_1.0/src/input/AudioOutputRuntime.h): Windows Speech API (SAPI) speaker thread wrapper; provides text-to-speech spoken output.
*   [`CameraRuntime.cpp`](file:///d:/Yuki_1.0/src/input/CameraRuntime.cpp) / [`CameraRuntime.h`](file:///d:/Yuki_1.0/src/input/CameraRuntime.h): DirectShow video stream background listener; feeds basic luminance shifts into perception.
*   [`ScreenRuntime.cpp`](file:///d:/Yuki_1.0/src/input/ScreenRuntime.cpp) / [`ScreenRuntime.h`](file:///d:/Yuki_1.0/src/input/ScreenRuntime.h): GDI BitBlt desktop capture that computes pixel grid hashes and scrapes active foreground process metrics.
*   [`SpeechSystem.cpp`](file:///d:/Yuki_1.0/src/input/SpeechSystem.cpp) / [`SpeechSystem.h`](file:///d:/Yuki_1.0/src/input/SpeechSystem.h): Coordinates Whisper API pipelines and routes transcript buffers.
*   [`Ear.cpp`](file:///d:/Yuki_1.0/src/input/Ear.cpp) / [`Ear.h`](file:///d:/Yuki_1.0/src/input/Ear.h) and [`Mouth.cpp`](file:///d:/Yuki_1.0/src/input/Mouth.cpp) / [`Mouth.h`](file:///d:/Yuki_1.0/src/input/Mouth.h): Gated interfaces to input and output audio threads.
*   [`PerceptionLayer.cpp`](file:///d:/Yuki_1.0/src/input/PerceptionLayer.cpp) / [`PerceptionLayer.h`](file:///d:/Yuki_1.0/src/input/PerceptionLayer.h): Normalizes raw sensor matrices into standard payloads.

#### 2.3.1 Signal Conditioning Sub-module (`src/input/conditioning/`)
*   [`SignalConditioningLayer.cpp`](file:///d:/Yuki_1.0/src/input/conditioning/SignalConditioningLayer.cpp) / [`SignalConditioningLayer.h`](file:///d:/Yuki_1.0/src/input/conditioning/SignalConditioningLayer.h): Master thread coordinator for signal normalization and alignment.
*   [`ArtifactFilter.cpp`](file:///d:/Yuki_1.0/src/input/conditioning/ArtifactFilter.cpp) / [`ArtifactFilter.h`](file:///d:/Yuki_1.0/src/input/conditioning/ArtifactFilter.h): Low-pass frequency cleaning and sensory error suppression.
*   [`SignalNormalizer.cpp`](file:///d:/Yuki_1.0/src/input/conditioning/SignalNormalizer.cpp) / [`SignalNormalizer.h`](file:///d:/Yuki_1.0/src/input/conditioning/SignalNormalizer.h): Map readings to standard zero-mean values using [`SensorCalibrationProfile.h`](file:///d:/Yuki_1.0/src/input/conditioning/SensorCalibrationProfile.h).
*   [`ChangeDetector.cpp`](file:///d:/Yuki_1.0/src/input/conditioning/ChangeDetector.cpp) / [`ChangeDetector.h`](file:///d:/Yuki_1.0/src/input/conditioning/ChangeDetector.h) and [`TemporalAligner.cpp`](file:///d:/Yuki_1.0/src/input/conditioning/TemporalAligner.cpp) / [`TemporalAligner.h`](file:///d:/Yuki_1.0/src/input/conditioning/TemporalAligner.h): Aligns multi-modal sensory frame offsets to capture shift events.

#### 2.3.2 Perception Encoding Sub-module (`src/input/encoding/`)
*   [`ObservationEncoder.cpp`](file:///d:/Yuki_1.0/src/input/encoding/ObservationEncoder.cpp) / [`ObservationEncoder.h`](file:///d:/Yuki_1.0/src/input/encoding/ObservationEncoder.h): Packs visual, raster, audio, and contextual features into a unified vector representation [`SensoryObservation.h`](file:///d:/Yuki_1.0/src/input/encoding/SensoryObservation.h).
*   [`AudioDSP.cpp`](file:///d:/Yuki_1.0/src/input/encoding/AudioDSP.cpp) / [`AudioDSP.h`](file:///d:/Yuki_1.0/src/input/encoding/AudioDSP.h): Raw spectral band mapping, power estimation, and silence checks.
*   [`VisualEncoder.cpp`](file:///d:/Yuki_1.0/src/input/encoding/VisualEncoder.cpp) / [`VisualEncoder.h`](file:///d:/Yuki_1.0/src/input/encoding/VisualEncoder.h): Resizes desktop screens and isolates motion metrics.
*   [`TextEncoder.cpp`](file:///d:/Yuki_1.0/src/input/encoding/TextEncoder.cpp) / [`TextEncoder.h`](file:///d:/Yuki_1.0/src/input/encoding/TextEncoder.h): Generates heuristic text vector projections.
*   [`MultiModalFusionGate.cpp`](file:///d:/Yuki_1.0/src/input/encoding/MultiModalFusionGate.cpp) / [`MultiModalFusionGate.h`](file:///d:/Yuki_1.0/src/input/encoding/MultiModalFusionGate.h): Cross-sensor weighting network that gates salient shifts.

---

### 2.4 Cognitive Operations & Tool Execution (`src/brain/`)
Located at [`src/brain/`](file:///d:/Yuki_1.0/src/brain). Houses logic routing, scraping, and script execution tools.

*   [`ToolExecutor.cpp`](file:///d:/Yuki_1.0/src/brain/ToolExecutor.cpp) / [`ToolExecutor.h`](file:///d:/Yuki_1.0/src/brain/ToolExecutor.h): Implements local system access (registry changes, settings queries) under strict permission gates.
*   [`SystemExecutor.cpp`](file:///d:/Yuki_1.0/src/brain/SystemExecutor.cpp) / [`SystemExecutor.h`](file:///d:/Yuki_1.0/src/brain/SystemExecutor.h): Executes terminal commands via Windows subprocess spawning.
*   [`ScriptRunner.cpp`](file:///d:/Yuki_1.0/src/brain/ScriptRunner.cpp) / [`ScriptRunner.h`](file:///d:/Yuki_1.0/src/brain/ScriptRunner.h): Spawns Python, PowerShell, or command-line scripts and monitors timeouts.
*   [`FileOperator.cpp`](file:///d:/Yuki_1.0/src/brain/FileOperator.cpp) / [`FileOperator.h`](file:///d:/Yuki_1.0/src/brain/FileOperator.h): Safe wrapper for directory manipulation and local text edits.
*   [`SmartScraper.cpp`](file:///d:/Yuki_1.0/src/brain/SmartScraper.cpp) / [`SmartScraper.h`](file:///d:/Yuki_1.0/src/brain/SmartScraper.h): HTTP web search engine using libcurl, passing source text to raw regex filters.
*   [`KnowledgeExtractor.cpp`](file:///d:/Yuki_1.0/src/brain/KnowledgeExtractor.cpp) / [`KnowledgeExtractor.h`](file:///d:/Yuki_1.0/src/brain/KnowledgeExtractor.h): Parses web HTML, removing layout noise to output structural text.
*   [`DocReader.cpp`](file:///d:/Yuki_1.0/src/brain/DocReader.cpp) / [`DocReader.h`](file:///d:/Yuki_1.0/src/brain/DocReader.h): Standalone local text parser extracting document paragraphs.
*   [`EntityProcessor.cpp`](file:///d:/Yuki_1.0/src/brain/EntityProcessor.cpp) / [`EntityProcessor.h`](file:///d:/Yuki_1.0/src/brain/EntityProcessor.h): Scans input text for proper nouns, cross-checking known relationships.
*   [`MobileServer.cpp`](file:///d:/Yuki_1.0/src/brain/MobileServer.cpp) / [`MobileServer.h`](file:///d:/Yuki_1.0/src/brain/MobileServer.h): Asynchronous Winsock HTTP server running on port `8765` for mobile UI status updates.
*   [`SafetyGovernor.cpp`](file:///d:/Yuki_1.0/src/brain/SafetyGovernor.cpp) / [`SafetyGovernor.h`](file:///d:/Yuki_1.0/src/brain/SafetyGovernor.h): Evaluates executing tasks against security policies.
*   [`VerificationEngine.cpp`](file:///d:/Yuki_1.0/src/brain/VerificationEngine.cpp) / [`VerificationEngine.h`](file:///d:/Yuki_1.0/src/brain/VerificationEngine.h): Compares expected sensory states with reality after an action is run.

---

### 2.5 Inference Layer (`src/brain/inference/`)
Located at [`src/brain/inference/`](file:///d:/Yuki_1.0/src/brain/inference). Responsible for active inference (Free Energy minimization).

*   [`VariationalStateEstimator.cpp`](file:///d:/Yuki_1.0/src/brain/inference/VariationalStateEstimator.cpp) / [`VariationalStateEstimator.h`](file:///d:/Yuki_1.0/src/brain/inference/VariationalStateEstimator.h): Cognitive state tracking engine. Computes Bayesian variational updates across system factors.
*   [`GenerativeModel.cpp`](file:///d:/Yuki_1.0/src/brain/inference/GenerativeModel.cpp) / [`GenerativeModel.h`](file:///d:/Yuki_1.0/src/brain/inference/GenerativeModel.h): Models state transitions $P(\text{Observation} \mid \text{State})$ and expected metrics.
*   [`BeliefState.cpp`](file:///d:/Yuki_1.0/src/brain/inference/BeliefState.cpp) / [`BeliefState.h`](file:///d:/Yuki_1.0/src/brain/inference/BeliefState.h): Holds the posterior belief distribution over 24 system status states.
*   [`FreeEnergyCalculator.cpp`](file:///d:/Yuki_1.0/src/brain/inference/FreeEnergyCalculator.cpp) / [`FreeEnergyCalculator.h`](file:///d:/Yuki_1.0/src/brain/inference/FreeEnergyCalculator.h): Calculates expected and variational Free Energy values.
*   [`PolicySelector.cpp`](file:///d:/Yuki_1.0/src/brain/inference/PolicySelector.cpp) / [`PolicySelector.h`](file:///d:/Yuki_1.0/src/brain/inference/PolicySelector.h): Picks actions that minimize expected Free Energy.
*   [`PrecisionEngine.cpp`](file:///d:/Yuki_1.0/src/brain/inference/PrecisionEngine.cpp) / [`PrecisionEngine.h`](file:///d:/Yuki_1.0/src/brain/inference/PrecisionEngine.h): Computes sensory precision (confidence weights) per input channel.
*   [`VseBootstrapTrainer.cpp`](file:///d:/Yuki_1.0/src/brain/inference/VseBootstrapTrainer.cpp) / [`VseBootstrapTrainer.h`](file:///d:/Yuki_1.0/src/brain/inference/VseBootstrapTrainer.h): Seeds the generative model with synthetic examples to bootstrap active inference.

---

### 2.6 Language Processing (`src/brain/language/`)
Located at [`src/brain/language/`](file:///d:/Yuki_1.0/src/brain/language). Coordinates communication with external LLM engines.

*   [`LocalLLM.cpp`](file:///d:/Yuki_1.0/src/brain/language/LocalLLM.cpp) / [`LocalLLM.h`](file:///d:/Yuki_1.0/src/brain/language/LocalLLM.h): Connects via Winsock2 sockets to a local Ollama HTTP endpoint (port `11434`) to parse and generate conversational responses.

---

### 2.7 Self-Learning & Scrapers (`src/brain/learning/`)
Located at [`src/brain/learning/`](file:///d:/Yuki_1.0/src/brain/learning). Focuses on background knowledge ingestion.

*   [`KnowledgeDaemon.cpp`](file:///d:/Yuki_1.0/src/brain/learning/KnowledgeDaemon.cpp) / [`KnowledgeDaemon.h`](file:///d:/Yuki_1.0/src/brain/learning/KnowledgeDaemon.h): Spawns background tasks that follow web links to automatically learn facts.
*   [`LearningIngestor.cpp`](file:///d:/Yuki_1.0/src/brain/learning/LearningIngestor.cpp) / [`LearningIngestor.h`](file:///d:/Yuki_1.0/src/brain/learning/LearningIngestor.h): Standardizes unstructured raw texts and schedules DB updates.
*   [`EmbeddingEngine.cpp`](file:///d:/Yuki_1.0/src/brain/learning/EmbeddingEngine.cpp) / [`EmbeddingEngine.h`](file:///d:/Yuki_1.0/src/brain/learning/EmbeddingEngine.h): Sends text to web APIs or local models to obtain dense vector representations.
*   [`MassCurriculumLoader.cpp`](file:///d:/Yuki_1.0/src/brain/learning/MassCurriculumLoader.cpp) / [`MassCurriculumLoader.h`](file:///d:/Yuki_1.0/src/brain/learning/MassCurriculumLoader.h): Seeds the semantic graphs with foundational definitions on initial launch.
*   [`BackgroundLearningEngine.cpp`](file:///d:/Yuki_1.0/src/brain/learning/BackgroundLearningEngine.cpp) / [`BackgroundLearningEngine.h`](file:///d:/Yuki_1.0/src/brain/learning/BackgroundLearningEngine.h): Manages passive processing queues for background tasks.

---

### 2.8 Memory Architectures (`src/brain/memory/`)
Located at [`src/brain/memory/`](file:///d:/Yuki_1.0/src/brain/memory). The cognitive memory fabric (CMF) subsystem.

*   [`CognitiveMemoryFabric.cpp`](file:///d:/Yuki_1.0/src/brain/memory/CognitiveMemoryFabric.cpp) / [`CognitiveMemoryFabric.h`](file:///d:/Yuki_1.0/src/brain/memory/CognitiveMemoryFabric.h): The primary interface coordinate wrapper for memory systems.
*   [`HdcSemanticGraph.cpp`](file:///d:/Yuki_1.0/src/brain/memory/HdcSemanticGraph.cpp) / [`HdcSemanticGraph.h`](file:///d:/Yuki_1.0/src/brain/memory/HdcSemanticGraph.h): Stores semantic concept nodes and links using high-dimensional computing (HDC) hypervectors in SQLite.
*   [`EpisodicStore.cpp`](file:///d:/Yuki_1.0/src/brain/memory/EpisodicStore.cpp) / [`EpisodicStore.h`](file:///d:/Yuki_1.0/src/brain/memory/EpisodicStore.h): Records raw conversational dialogue history and contextual vectors.
*   [`UserMemory.cpp`](file:///d:/Yuki_1.0/src/brain/memory/UserMemory.cpp) / [`UserMemory.h`](file:///d:/Yuki_1.0/src/brain/memory/UserMemory.h): Manages user-specific relationships and personal facts in the database.
*   [`SparseDistributedMemory.cpp`](file:///d:/Yuki_1.0/src/brain/memory/SparseDistributedMemory.cpp) / [`SparseDistributedMemory.h`](file:///d:/Yuki_1.0/src/brain/memory/SparseDistributedMemory.h): Emplements Kanerva's SDM using bitwise operations and saturating counters.
*   [`Hypervector.cpp`](file:///d:/Yuki_1.0/src/brain/memory/Hypervector.cpp) / [`Hypervector.h`](file:///d:/Yuki_1.0/src/brain/memory/Hypervector.h): Low-level 1024-bit vector math optimized for Hamming distance scoring.
*   [`HypervectorEncoder.cpp`](file:///d:/Yuki_1.0/src/brain/memory/HypervectorEncoder.cpp) / [`HypervectorEncoder.h`](file:///d:/Yuki_1.0/src/brain/memory/HypervectorEncoder.h): Encodes textual character triggers into sparse hypervector points.
*   [`LocalitySensitiveHash.cpp`](file:///d:/Yuki_1.0/src/brain/memory/LocalitySensitiveHash.cpp) / [`LocalitySensitiveHash.h`](file:///d:/Yuki_1.0/src/brain/memory/LocalitySensitiveHash.h): Maps hypervectors into compact buckets for fast retrieval.
*   [`ArchiveWriter.cpp`](file:///d:/Yuki_1.0/src/brain/memory/ArchiveWriter.cpp) / [`ArchiveWriter.h`](file:///d:/Yuki_1.0/src/brain/memory/ArchiveWriter.h): Serializes memory segments to a custom `.yuk` binary archive structure.
*   [`ColumnarArchiveFormat.cpp`](file:///d:/Yuki_1.0/src/brain/memory/ColumnarArchiveFormat.cpp) / [`ColumnarArchiveFormat.h`](file:///d:/Yuki_1.0/src/brain/memory/ColumnarArchiveFormat.h): Handles binary formatting and compression for long-term memory dumps.
*   [`MerkleDAG.cpp`](file:///d:/Yuki_1.0/src/brain/memory/MerkleDAG.cpp) / [`MerkleDAG.h`](file:///d:/Yuki_1.0/src/brain/memory/MerkleDAG.h): Content-addressed memory graph structure powered by SHA-256 integrity proofs.
*   [`DifferentialMemoryController.cpp`](file:///d:/Yuki_1.0/src/brain/memory/DifferentialMemoryController.cpp) / [`DifferentialMemoryController.h`](file:///d:/Yuki_1.0/src/brain/memory/DifferentialMemoryController.h) and [`ProceduralStore.cpp`](file:///d:/Yuki_1.0/src/brain/memory/ProceduralStore.cpp) / [`ProceduralStore.h`](file:///d:/Yuki_1.0/src/brain/memory/ProceduralStore.h): Manages neural weights and execution actions via [`TinyMLP.h`](file:///d:/Yuki_1.0/src/brain/memory/TinyMLP.h).
*   [`MemoryDistiller.cpp`](file:///d:/Yuki_1.0/src/brain/memory/MemoryDistiller.cpp) / [`MemoryDistiller.h`](file:///d:/Yuki_1.0/src/brain/memory/MemoryDistiller.h): Simplifies episodic sequences into semantic nodes.
*   [`ActiveInferenceRetrieval.cpp`](file:///d:/Yuki_1.0/src/brain/memory/ActiveInferenceRetrieval.cpp) / [`ActiveInferenceRetrieval.h`](file:///d:/Yuki_1.0/src/brain/memory/ActiveInferenceRetrieval.h): Fetches memory records that minimize KL divergence via [`InformationGainEngine.h`](file:///d:/Yuki_1.0/src/brain/memory/InformationGainEngine.h).
*   [`PromotionMetrics.cpp`](file:///d:/Yuki_1.0/src/brain/memory/PromotionMetrics.cpp) / [`PromotionMetrics.h`](file:///d:/Yuki_1.0/src/brain/memory/PromotionMetrics.h): Adjusts memory consolidation rates based on recall frequency and decay.

---

### 2.9 Turn Engine Subsystem (`src/brain/predictive/`)
Located at [`src/brain/predictive/`](file:///d:/Yuki_1.0/src/brain/predictive). Handles the reasoning and execution pipeline.

*   [`predictive_turn_engine.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/predictive_turn_engine.cpp) / [`predictive_turn_engine.h`](file:///d:/Yuki_1.0/src/brain/predictive/predictive_turn_engine.h): Orchestrates the turn lifecycle: `[RESOLVE]` intent, `[SHAPE]` safety/profile constraints, and `[CONTEST]` candidate selection.
*   [`stream_workers.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/stream_workers.cpp) / [`stream_workers.h`](file:///d:/Yuki_1.0/src/brain/predictive/stream_workers.h): Contains stream implementations:
    *   **`E1FastStream`**: Immediate response reflex for greetings and system interrupts.
    *   **`E2SemanticStream`**: Associative context retrieval from episodic/semantic databases.
    *   **`E3DeepStream`**: Active inference execution loop, running policy simulations.
*   [`sqlite_memory_store.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/sqlite_memory_store.cpp) / [`sqlite_memory_store.h`](file:///d:/Yuki_1.0/src/brain/predictive/sqlite_memory_store.h): Implements backing memory interfaces mapping to SQL tables.
*   [`IntentResponseRouter.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/IntentResponseRouter.cpp) / [`IntentResponseRouter.h`](file:///d:/Yuki_1.0/src/brain/predictive/IntentResponseRouter.h): Connects classified intent classes to LLM system templates.
*   [`salience_gate.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/salience_gate.cpp): Measures context changes to bypass heavy evaluation steps.
*   [`response_shaper.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/response_shaper.cpp): Adjusts character style parameters based on VSE beliefs.
*   [`tool_adapter.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/tool_adapter.cpp) / [`tool_adapter.h`](file:///d:/Yuki_1.0/src/brain/predictive/tool_adapter.h): Maps output action calls to system tools.

---

### 2.10 Cognitive Reasoning Engines (`src/brain/reasoning/`)
Located at [`src/brain/reasoning/`](file:///d:/Yuki_1.0/src/brain/reasoning). Handles NLU, decomposition, planning, and response composition.

*   [`InputResolution.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/InputResolution.cpp) / [`InputResolution.h`](file:///d:/Yuki_1.0/src/brain/reasoning/InputResolution.h): Resolves ambiguous commands and sets clarification triggers.
*   [`SemanticParser.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/SemanticParser.cpp) / [`SemanticParser.h`](file:///d:/Yuki_1.0/src/brain/reasoning/SemanticParser.h): Extracts structured variables (actions, targets, values) from input text.
*   [`TaskSystem.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/TaskSystem.cpp) / [`TaskSystem.h`](file:///d:/Yuki_1.0/src/brain/reasoning/TaskSystem.h): Decomposes goals into sequential sub-tasks.
*   [`GoalModel.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/GoalModel.cpp) / [`GoalModel.h`](file:///d:/Yuki_1.0/src/brain/reasoning/GoalModel.h): Represents task progress and monitors active sub-goals.
*   [`EvidenceSystem.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/EvidenceSystem.cpp) / [`EvidenceSystem.h`](file:///d:/Yuki_1.0/src/brain/reasoning/EvidenceSystem.h): Aggregates facts and checks for contradictions before executing plans.
*   [`PatternEngine.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/PatternEngine.cpp) / [`PatternEngine.h`](file:///d:/Yuki_1.0/src/brain/reasoning/PatternEngine.h): Evaluates raw sensory events to classify current interaction contexts.
*   [`SynthesisEngine.cpp`](file:///d:/Yuki_1.0/src/brain/reasoning/SynthesisEngine.cpp) / [`SynthesisEngine.h`](file:///d:/Yuki_1.0/src/brain/reasoning/SynthesisEngine.h): Formulates final responses based on gathered evidence.

---

### 2.11 Memory Consolidation & Sleep (`src/brain/sleep/`)
Located at [`src/brain/sleep/`](file:///d:/Yuki_1.0/src/brain/sleep). Processes offline data when the system is idle.

*   [`SleepThread.cpp`](file:///d:/Yuki_1.0/src/brain/sleep/SleepThread.cpp) / [`SleepThread.h`](file:///d:/Yuki_1.0/src/brain/sleep/SleepThread.h): Background consolidation thread. Spawns when the user goes idle to process memory structures.
*   [`SleepConsolidator.cpp`](file:///d:/Yuki_1.0/src/brain/sleep/SleepConsolidator.cpp) / [`SleepConsolidator.h`](file:///d:/Yuki_1.0/src/brain/sleep/SleepConsolidator.h): Handles memory transitions from episodic to HDC semantic graph storage.
*   [`CounterfactualReplayEngine.cpp`](file:///d:/Yuki_1.0/src/brain/sleep/CounterfactualReplayEngine.cpp) / [`CounterfactualReplayEngine.h`](file:///d:/Yuki_1.0/src/brain/sleep/CounterfactualReplayEngine.h): Simulates alternative outcomes of executed plans to update state probabilities.

---

### 2.12 Infrastructure & System Bus (`src/infrastructure/`)
Located at [`src/infrastructure/`](file:///d:/Yuki_1.0/src/infrastructure). The core communication bus.

*   [`CoreBus.cpp`](file:///d:/Yuki_1.0/src/infrastructure/CoreBus.h): Pub/sub broker routing messages across cognitive modules.
*   [`GlobalWorkspace.cpp`](file:///d:/Yuki_1.0/src/infrastructure/GlobalWorkspace.h): Lock-free central memory queue for inter-thread communication.
*   [`ControlPlane.cpp`](file:///d:/Yuki_1.0/src/infrastructure/ControlPlane.h): Monitors thread execution status and handles transitions (IDLE, PROCESSING, SLEEPING).
*   [`ModuleRegistry.cpp`](file:///d:/Yuki_1.0/src/infrastructure/ModuleRegistry.h): Registers active modules, dependencies, and message topic subscriptions.

---

### 2.13 Database Adapters & Scrapling (`src/vendor/` & `src/scrapling/`)
*   [`sqlite3.c`](file:///d:/Yuki_1.0/src/vendor/sqlite/sqlite3.c) (255,000+ LOC): Backing SQLite database engine.
*   [`scrapling_client.cpp`](file:///d:/Yuki_1.0/src/scrapling/scrapling_client.cpp) (450 LOC): Interface wrapper connecting to Python web scrapers.

---

## 3. Data Flow & Execution Pipelines

### 3.1 Boot Sequence
1.  **Main Initialization**: [`main.cpp`](file:///d:/Yuki_1.0/src/main.cpp) starts, registers Win32 window classes, and connects to [`DatabaseManager`](file:///d:/Yuki_1.0/src/brain/database/DatabaseManager.cpp).
2.  **Infrastructure Initialization**: [`ControlPlane`](file:///d:/Yuki_1.0/src/infrastructure/ControlPlane.cpp) and [`GlobalWorkspace`](file:///d:/Yuki_1.0/src/infrastructure/GlobalWorkspace.cpp) boot up.
3.  **Reflex Ingestion**: [`BabyMode`](file:///d:/Yuki_1.0/src/BabyMode.cpp) triggers [`MassCurriculumLoader`](file:///d:/Yuki_1.0/src/brain/learning/MassCurriculumLoader.cpp) to populate default concepts if the database is empty.
4.  **Module Binding**: Cognitive modules (VSE, CMF, NeuralSpine, SleepThread) register subscriptions with the [`ModuleRegistry`](file:///d:/Yuki_1.0/src/infrastructure/ModuleRegistry.cpp).
5.  **Turn Engine Configuration**: [`TurnCoordinator`](file:///d:/Yuki_1.0/src/brain/predictive/predictive_turn_engine.cpp) initializes stream workers (`E1FastStream`, `E2SemanticStream`, `E3DeepStream`).
6.  **Sensor Binding**: [`SignalConditioningLayer`](file:///d:/Yuki_1.0/src/input/conditioning/SignalConditioningLayer.cpp) binds hardware capture runtimes (`AudioInputRuntime`, `CameraRuntime`, `ScreenRuntime`).
7.  **UI Launch**: Background thread spawns GDI+ windows for [`PresenceShell`](file:///d:/Yuki_1.0/src/PresenceShell.cpp), [`DetailView`](file:///d:/Yuki_1.0/src/DetailView.cpp), and [`AvatarBody`](file:///d:/Yuki_1.0/src/AvatarBody.cpp).
8.  **Ready Signal**: Once audio input (mic) and output (speaker) streams verify, [`BabyMode::announceReady()`](file:///d:/Yuki_1.0/src/BabyMode.cpp) prints a boot greeting and speaks via SAPI.

---

### 3.2 The Turn Loop (Perception → Response)
1.  **Sensor Capture**: Runtimes stream raw events (GDI desktop captures, waveIn buffers) to the [`SignalConditioningLayer`](file:///d:/Yuki_1.0/src/input/conditioning/SignalConditioningLayer.cpp).
2.  **Conditioning & Alignment**: [`ArtifactFilter`](file:///d:/Yuki_1.0/src/input/conditioning/ArtifactFilter.cpp) removes noise, [`SignalNormalizer`](file:///d:/Yuki_1.0/src/input/conditioning/SignalNormalizer.cpp) scales parameters, and [`TemporalAligner`](file:///d:/Yuki_1.0/src/input/conditioning/TemporalAligner.cpp) aligns sensory offsets.
3.  **Event Submission**: SCL submits the event to the [`UnifiedPerceptionLayer`](file:///d:/Yuki_1.0/src/input/PerceptionLayer.cpp).
4.  **Routing**: [`CommandRouter`](file:///d:/Yuki_1.0/src/CommandRouter.cpp) determines if the event is a command or text input.
5.  **Cognitive Evaluation**: [`BabyMode`](file:///d:/Yuki_1.0/src/BabyMode.cpp) forwards the input to the [`TurnCoordinator`](file:///d:/Yuki_1.0/src/brain/predictive/predictive_turn_engine.cpp).
6.  **Stream Evaluation**:
    *   **`E1FastStream`**: Scans for exact keyword triggers or system interrupts.
    *   **`E2SemanticStream`**: Performs semantic search against [`HdcSemanticGraph`](file:///d:/Yuki_1.0/src/brain/memory/HdcSemanticGraph.cpp) and [`UserMemory`](file:///d:/Yuki_1.0/src/brain/memory/UserMemory.cpp).
    *   **`E3DeepStream`**: Updates belief matrices in [`VariationalStateEstimator`](file:///d:/Yuki_1.0/src/brain/inference/VariationalStateEstimator.cpp) and evaluates plans using [`TaskSystem`](file:///d:/Yuki_1.0/src/brain/reasoning/TaskSystem.cpp).
7.  **Turn Lifecycle**:
    *   **`[RESOLVE]`**: Parses meaning via [`SemanticParser`](file:///d:/Yuki_1.0/src/brain/reasoning/SemanticParser.cpp) and checks if clarification is needed.
    *   **`[SHAPE]`**: Applies constraints from [`SafetyGovernor`](file:///d:/Yuki_1.0/src/brain/SafetyGovernor.cpp).
    *   **`[CONTEST]`**: Evaluates responses and selects the highest-scoring candidate.
8.  **Output Execution**: Composed responses are spoken via SAPI ([`AudioOutputRuntime`](file:///d:/Yuki_1.0/src/input/AudioOutputRuntime.cpp)) and displayed in GDI+ UI views.

---

### 3.3 Memory Consolidation Flow (Sleep Cycle)
1.  **Idle Detection**: [`ControlPlane`](file:///d:/Yuki_1.0/src/infrastructure/ControlPlane.cpp) transitions system to `SLEEPING` when no input is received for 30 seconds.
2.  **Consolidation Boot**: [`SleepThread`](file:///d:/Yuki_1.0/src/brain/sleep/SleepThread.cpp) activates.
3.  **HDC Consolidation**: [`SleepConsolidator`](file:///d:/Yuki_1.0/src/brain/sleep/SleepConsolidator.cpp) reads the day's events from [`EpisodicStore`](file:///d:/Yuki_1.0/src/brain/memory/EpisodicStore.cpp), extracts key concept nodes, and binds them to SQLite semantic graphs.
4.  **Replay simulations**: [`CounterfactualReplayEngine`](file:///d:/Yuki_1.0/src/brain/sleep/CounterfactualReplayEngine.cpp) runs simulations of failed tasks to refine VSE belief state parameters.
5.  **Long Term Promotion**: Promotes high-frequency episodic patterns to [`UserMemory`](file:///d:/Yuki_1.0/src/brain/memory/UserMemory.cpp).
6.  **Archiving**: [`ArchiveWriter`](file:///d:/Yuki_1.0/src/brain/memory/ArchiveWriter.cpp) packages consolidated nodes into long-term `.yuk` files.

---

## 4. Current Stubs & System Bugs

The following stubs and bugs prevent Yuki from functioning properly:

> [!WARNING]
> **Bug #1: Clarification Loop Never Exits**
> In [`predictive_turn_engine.cpp`](file:///d:/Yuki_1.0/src/brain/predictive/predictive_turn_engine.cpp)'s `[SHAPE]` phase, the system frequently gets stuck in a loop, asking the user for clarification even when the intent confidence score is high (e.g., >90%).

> [!WARNING]
> **Bug #2: KnowledgeDaemon Relevance Pollution**
> [`KnowledgeDaemon.cpp`](file:///d:/Yuki_1.0/src/brain/learning/KnowledgeDaemon.cpp) lacks semantic context filtering. It follows random Wikipedia links, polluting the database with irrelevant facts.

> [!WARNING]
> **Bug #3: SleepThread Inactivity**
> [`SleepThread.cpp`](file:///d:/Yuki_1.0/src/brain/sleep/SleepThread.cpp)'s consolidation loop is largely a placeholder. It executes without traversing the semantic graph, leaving episodic memories unconsolidated.

> [!IMPORTANT]
> **Executor Stubs**
> Core action execution files such as [`SystemExecutor.cpp`](file:///d:/Yuki_1.0/src/brain/SystemExecutor.cpp), [`ScriptRunner.cpp`](file:///d:/Yuki_1.0/src/brain/ScriptRunner.cpp), and [`FileOperator.cpp`](file:///d:/Yuki_1.0/src/brain/FileOperator.cpp) contain basic templates and do not execute actual system commands.
