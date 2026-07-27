# YUKI v1.0 — Duplicate & Architectural Drift Report

> **Generated:** 2026-07-28  
> **Authority:** Cross-referenced against `yuki_flow.md` and `CMakeLists.txt`

---

## Section A: Duplicate & Suspicious Entries Analysis

### 1. Exact Duplicate File Registrations in `CMakeLists.txt`
These exact paths are declared multiple times in `YUKI_CORE_SOURCES`:

```cmake
# Duplicate Entry 1:
Line 167: src/brain/language/LocalLLM.cpp
Line 216: src/brain/language/LocalLLM.cpp

# Duplicate Entry 2:
Line 258: src/brain/self/SelfModel.cpp
Line 286: src/brain/self/SelfModel.cpp
```
*Impact:* Harmless to final linking in MSVC, but causes build log noise and redundant object lookup during library archiving.

---

### 2. Identical Filename Collisions Across Directories
These files share the exact same basename across different directory trees:

```
Collision Group 1: PolicySelector
- src/brain/inference/PolicySelector.cpp (Friston Active Inference Free Energy minimization)
- src/brain/policy/PolicySelector.cpp (Executive behavior strategy selection)

Collision Group 2: CognitiveOrchestrator
- src/brain/system/CognitiveOrchestrator.cpp (System-wide pipeline coordinator)
- src/brain/ync/CognitiveOrchestrator.cpp (Spiking Neuromorphic Core scheduler)
```
*Impact:* High cognitive load for developers and potential linker symbol collisions if class namespaces are omitted.

---

### 3. Conceptual Overlap & Migration Leftovers

| Subsystem | File A | File B | Drift Status |
|:---|:---|:---|:---|
| **Language Generation** | `src/brain/language/SentenceMaker.cpp` | `src/brain/language/GrammarEngine.cpp` | Y2K template generation vs PCFG probabilistic grammar |
| **Language Processing** | `src/brain/language/EnglishLanguageEngine.cpp` | `src/brain/reasoning/SemanticParser.cpp` | Rule-based parser vs formal semantic frame parser |
| **Memory Fabric** | `src/brain/memory/MemoryFabric.cpp` | `src/brain/memory/CognitiveMemoryFabric.cpp` | M3 static memory fabric vs M7 PACL dynamic memory fabric |
| **Tool Execution** | `src/brain/ToolExecutor.cpp` | `src/brain/action/core/ActionExecutor.cpp` | Legacy tool runner vs M4 transactional rollback executor |

---

## Section B: Architecture Layer Mapping

Every folder in `CMakeLists.txt` maps to YUKI's cognitive digital organism architecture:

```mermaid
graph TD
    Subgraph Perception ["1. Input & Perception Layer"]
        src_input["src/input/"]
        src_input_conditioning["src/input/conditioning/"]
        src_input_encoding["src/input/encoding/"]
    end

    Subgraph Language ["2. Symbolic & Natural Language Layer"]
        src_brain_language["src/brain/language/"]
    end

    Subgraph Memory ["3. Multi-Store Cognitive Memory"]
        src_brain_memory["src/brain/memory/"]
        src_brain_database["src/brain/database/"]
    end

    Subgraph Learning ["4. Self-Learning & SNN Core"]
        src_brain_learning["src/brain/learning/"]
        src_brain_learning_neural["src/brain/learning/neural/"]
        src_brain_ync["src/brain/ync/"]
    end

    Subgraph Reasoning ["5. Causal & Formal Reasoning"]
        src_brain_reasoning["src/brain/reasoning/"]
        src_brain_logic["src/brain/logic/"]
        src_brain_causality["src/brain/causality/"]
        src_brain_causal["src/brain/causal/"]
        src_brain_world["src/brain/world/"]
    end

    Subgraph Planning ["6. Planning & Action Layer"]
        src_brain_planning["src/brain/planning/"]
        src_brain_action_core["src/brain/action/core/"]
        src_brain_action_tools["src/brain/action/tools/"]
        src_brain_capability["src/brain/capability/"]
    end

    Subgraph Metacognition ["7. Metacognition & Self-Model"]
        src_brain_inference["src/brain/inference/"]
        src_brain_metacognition["src/brain/metacognition/"]
        src_brain_self["src/brain/self/"]
        src_brain_organism["src/brain/organism/"]
    end

    Subgraph Safety ["8. Safety & Integrity Layer"]
        src_brain_security["src/brain/security/"]
        src_brain_ethics["src/brain/ethics/"]
    end

    Subgraph Research ["9. Research & Discovery"]
        src_brain_research["src/brain/research/"]
        src_brain_research_tools["src/brain/research/tools/"]
    end

    Subgraph Infrastructure ["10. Real-Time Infrastructure"]
        src_infrastructure["src/infrastructure/"]
        src_brain_core_event["src/brain/core/event/"]
        src_brain_core_io["src/brain/core/io/"]
    end
```

---

## Section C: Refactoring & Cleanup Action Plan

1. **Clean `CMakeLists.txt` Source Array:** Remove redundant duplicate lines (2 lines saved).
2. **Rename Overlapping Files:**
   - Rename `src/brain/policy/PolicySelector.cpp` → `ExecutivePolicySelector.cpp`.
   - Rename `src/brain/ync/CognitiveOrchestrator.cpp` → `YncOrchestrator.cpp`.
3. **Consolidate Memory Fabric:** Deprecate `MemoryFabric.cpp` in favor of `CognitiveMemoryFabric.cpp`.
