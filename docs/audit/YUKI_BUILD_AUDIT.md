# YUKI v1.0 — CMake Build & Component Quality Audit

> **Generated:** 2026-07-28  
> **Source:** `CMakeLists.txt` Comprehensive Analysis  
> **Target:** `yuki_core` Static Library & Executable Targets

---

## 1. Build Summary

- **Main Build Targets:**
  - `yuki_core` (Static Library containing 168 compilation units compiled ONCE)
  - `yuki` (Main executable target linked against `yuki_core`)
  - **121 CTest Executable Targets** (linked against `yuki_core` to prevent redundant source recompilation)
- **C++ Standard:** **C++20** (`set(CMAKE_CXX_STANDARD 20)` with `CMAKE_CXX_STANDARD_REQUIRED ON`)
- **External Dependencies:**
  - `Whisper.cpp` (v1.5.4 via CMake `FetchContent`)
  - `HNSWLib` (v0.8.0 via CMake `FetchContent`)
  - `libcurl` (System / `find_package(CURL REQUIRED)`)
  - `Boost` (System / `find_package(Boost REQUIRED COMPONENTS system thread)`)
  - `nlohmann_json` (System / `find_package(nlohmann_json CONFIG REQUIRED)`)
  - `SQLite3` (Embedded / `src/vendor/sqlite/sqlite3.c`)
  - `Moodycamel` (Embedded header / `src/vendor/moodycamel/`)
- **Architecture Model:** **Hybrid Local-Autonomous Digital Organism**
  - **Self-Contained Cognitive Substrate:** 100% C++ native execution for Active Inference, Judea Pearl Causal Graph, SNN Spiking Neural Core, HDC Sparse Distributed Memory, and HTN Planner.
  - **Hybrid Integration:** External fetching for Whisper speech transcription and local Ollama / ONNX LLM connection.

---

## 2. Component Classification Matrix

| Bucket | Count | Examples / Primary Subsystems |
|:---|:---:|:---|
| **Production Core** | 88 | `NeuralSpine.cpp`, `VariationalStateEstimator.cpp`, `CausalGraph.cpp`, `HtnPlanner.cpp`, `NeuromorphicSimulator.cpp`, `GlobalWorkspace.cpp`, `EventLoopCore.cpp` |
| **Production Support** | 42 | `ConfigManager.cpp`, `DatabaseManager.cpp`, `UniversalCache.cpp`, `Logger.cpp`, `PathNormalizer.cpp`, `SecuritySandbox.cpp` |
| **Research / Experimental** | 18 | `SelfPlayEngine.cpp`, `VariationalAutoencoder.cpp`, `ConceptBlender.cpp`, `CreativeSearch.cpp`, `MetaphorEngine.cpp`, `CodeSynthesisAgent.cpp` |
| **Test Executables** | 121 | `tests/` (22 active tests) and `not_in_use/test_files/` (67 historical tests + gap suites) |
| **Vendor / External** | 4 | `sqlite3.c`, `whisper.cpp`, `hnswlib`, `moodycamel` |
| **Legacy / Unclear** | 16 | Y2K porting leftovers (`SentenceMaker.cpp`, `SentenceBuilder.cpp`, `EnglishLanguageEngine.cpp`, `tools_y2k`) |

---

## 3. Risk Audit & Findings

### ⚠️ Risk 1: Duplicate Source Registrations in `CMakeLists.txt`
1. `src/brain/language/LocalLLM.cpp` is listed at **Line 167 AND Line 216**.
2. `src/brain/self/SelfModel.cpp` is listed at **Line 258 AND Line 286**.

### ⚠️ Risk 2: Filename Naming Collisions (Same Basename, Different Folders)
1. `PolicySelector.cpp`:
   - `src/brain/inference/PolicySelector.cpp` (Active Inference Friston policy selection)
   - `src/brain/policy/PolicySelector.cpp` (Executive action policy router)
2. `CognitiveOrchestrator.cpp`:
   - `src/brain/system/CognitiveOrchestrator.cpp` (System-wide cognitive module orchestrator)
   - `src/brain/ync/CognitiveOrchestrator.cpp` (YUKI Neuromorphic Core SNN orchestrator)

### ⚠️ Risk 3: Functional Overlap & Legacy Coexistence
- **Language Subsystem:** `SentenceMaker.cpp` / `SentenceBuilder.cpp` / `EnglishLanguageEngine.cpp` (Y2K rules) coexist with `GrammarEngine.cpp` (PCFG) and `LocalLLM.cpp`.
- **Memory Subsystem:** `MemoryFabric.cpp` vs `CognitiveMemoryFabric.cpp` vs `ContextMemory.cpp`.

---

## 4. Strategic Recommendations

1. **Build Hygiene:**
   - Remove duplicate source lines (`LocalLLM.cpp` line 216 and `SelfModel.cpp` line 286) from `CMakeLists.txt`.
2. **Disambiguate Naming Collisions:**
   - Rename `src/brain/policy/PolicySelector.cpp` to `ExecutivePolicySelector.cpp` to distinguish it from Friston `VsePolicySelector.cpp`.
   - Rename `src/brain/ync/CognitiveOrchestrator.cpp` to `YncOrchestrator.cpp`.
3. **Consolidate Test Folders:**
   - Move active tests in `not_in_use/test_files/` into `tests/unit/` and rename `not_in_use/test_files/` to `tests/legacy/`.
