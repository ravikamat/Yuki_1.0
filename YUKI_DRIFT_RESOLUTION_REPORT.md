# YUKI Architecture Drift Resolution Report
Generated: 2026-07-28
Authority: YUKI_BUILD_AUDIT.md, YUKI_DUPLICATE_AND_DRIFT_REPORT.md, yuki_flow.md

## 1. Language Stack Overlap

| File | Role | Disposition |
|---|---|---|
| LocalLLM.cpp | Primary inference backend | KEEP — active in YUKI_CORE_SOURCES |
| SentenceMaker.cpp | Interface-layer sentence API | BRIDGE — retain until chat layer migrates |
| SentenceBuilder.cpp | Earlier construction iteration | LEGACY — retain for regression coverage |
| EnglishLanguageEngine.cpp | Broad language facade | LEGACY — retain; may fold into LocalLLM later |
| GrammarEngine.cpp | Explicit grammar rules | DEPRECATE-LATER — evaluate after LocalLLM grammar coverage proven |

## 2. Memory Stack Overlap

| File | Role | Disposition |
|---|---|---|
| MemoryFabric.cpp | Canonical memory substrate | KEEP — primary |
| CognitiveMemoryFabric.cpp | Cognitive-layer wrapper | BRIDGE — retain while cognitive modules depend on it |
| ContextMemory.cpp | Short-term context buffer | LEGACY — audit for redundancy with MemoryFabric |

## 3. Execution Stack Overlap

| File | Role | Disposition |
|---|---|---|
| ActionExecutor.cpp (action/core) | Canonical action execution | KEEP — primary |
| ToolExecutor.cpp | Tool-call translation layer | BRIDGE — competence-gated adapter per yuki_flow.md |

## Rename Log

- `src/brain/policy/PolicySelector.cpp` → `ExecutivePolicySelector.cpp`
- `src/brain/ync/CognitiveOrchestrator.cpp` → `YncOrchestrator.cpp`

## Build Integrity

- Duplicate registrations removed: LocalLLM.cpp, SelfModel.cpp
- Zero behavioral change expected.
- All renames propagated to CMake, includes, forward declarations, and tests.
