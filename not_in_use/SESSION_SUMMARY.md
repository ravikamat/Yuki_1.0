# YUKI v1.0 — SESSION SUMMARY
**Date:** 2026-06-06 | **Session Duration:** ~7 hours | **Status:** Phase 0 In Progress

---

## 1. SESSION GOAL
Implement Phase 0: Fix critical clarification death spiral + integrate local LLM (Qwen3:1.7b via Ollama) for neural response generation. Remove ALL heuristics/hardcoded responses.

---

## 2. WHAT WAS COMPLETED

### 2.1 Root Cause Analysis (DONE)
- **Clarification Death Spiral identified:** `entity_ok` check fails for ALL inputs (even "hi") because `entity_mass=0.25` < threshold `0.65`, forcing `clarify_during.push_back("entity")`
- `shape_response()` converts ANY `clarify_during` into `requires_clarification=1`, blocking LLM path
- `phatic_score` field name mismatch (`hs.phatic` vs `hs.greeting`) prevents feature-based bypass
- Contested dimensions loop uses unboosted `pool_.belief_mass()` even after local intent_mass boost
- `shape_response()` has early-return branches (sections 1-3) that return BEFORE reaching LLM code in section 4

### 2.2 LocalLLM Class Implemented (DONE — Socket-based)
- **File:** `src/brain/language/LocalLLM.h` + `.cpp`
- Uses Winsock2 (ws2_32.lib) instead of WinHTTP (which failed on user's machine)
- Methods: `isAvailable()`, `generate()`, `chat()`, `buildPrompt()`
- NO `tryTier1()` — heuristic-free, neural-only
- JSON parsing for Ollama `/api/generate` and `/api/chat` endpoints
- Process fallback detection if HTTP fails

### 2.3 TurnCoordinator Modifications (PARTIALLY DONE)
- Added `LocalLLM*` member + `setLocalLLM()` setter
- Removed `const` from `shape_response()` signature
- **CRITICAL BUG REMAINS:** LLM code placed in "section 4" of `shape_response()`, but sections 1-3 (`!can_act`, `!clarify_before`, `!clarify_during`) all `return r` early — LLM NEVER RUNS when `can_act=0`

### 2.4 BabyMode Wiring (DONE)
- `local_llm_` unique_ptr member added
- Instantiation in constructor with availability check
- Cleanup in destructor
- CMakeLists.txt updated with `src/brain/language/LocalLLM.cpp` + `ws2_32.lib`

### 2.5 VSE Bootstrap Trainer (GENERATED, NOT YET INTEGRATED)
- **File:** `src/brain/inference/VseBootstrapTrainer.h` + `.cpp`
- 160 synthetic training examples across 8 intent classes
- Feature extraction from text → dense observation vector (24 dims)
- Calls `GenerativeModel::updateMapping()` with EMA learning
- Designed to raise `prec.intent` from 0.07 toward 0.5+
- **NOT YET WIRED** into BabyMode startup

---

## 3. CURRENT BUILD STATUS

| Component | Status |
|-----------|--------|
| MSVC Build | ✅ Clean, zero warnings |
| Tests | ✅ 13/13 passing (predictive_turn_engine) |
| Ollama Detection | ✅ Confirmed: `qwen3:1.7b` at `127.0.0.1:11434` |
| LLM Generation | ⚠️ Intermittent (timeout on cold start) |
| TTS | 🔴 OFF (intentional — user disabled) |
| Response Output | ⚠️ Truncated by ResponseShaper; system prompt leaks |

---

## 4. CRITICAL ISSUES REMAINING

### P0 — Blocking Conversation Flow
| # | Issue | Evidence | Root Cause |
|---|-------|----------|------------|
| 1 | **LLM code unreachable when `can_act=0`** | `[SHAPE] LLM response: 196 tokens` → `[BABY] can_act=0 requires_clarification:1` | `shape_response()` sections 1-3 return before section 4 |
| 2 | **Response truncation** | `"English grammar consists of syntax...punctuat"` | `ResponseShaper::apply()` hard-cuts at ~60 chars |
| 3 | **System prompt leaking** | `"Could you clarify what you mean by 'the user's intent is unclear'"` | System instruction prepended to user prompt instead of Ollama `system` field |
| 4 | **LLM connection intermittent** | `[LocalLLM] generate() failed: Empty HTTP response` | Socket timeout too aggressive for CPU cold-start |
| 5 | **Mass curriculum poisoning** | Every turn: `[mass_curriculum] English grammar...` | AIR retrieval returns lowest-quality data; no relevance filter |

### P1 — Model Training
| # | Issue | Evidence | Root Cause |
|---|-------|----------|------------|
| 6 | **VSE generative model untrained** | `prec.intent: 0.07` (should be 0.5+) | Bootstrapped priors only; no labeled training data |
| 7 | **Intent mass never stabilizes** | `0.40 → 0.58 → 0.69 → 0.40` oscillation | No learning signal; surprise accumulates |
| 8 | **Surprise death spiral** | `0.89 → 2.73` unbounded | Untrained model → wrong predictions → high surprise → more uncertainty |

### P2 — Architecture (From Audit Report)
| # | Issue | Severity | Status |
|---|-------|----------|--------|
| 9 | **Execution layer missing** | 🔴 CRITICAL | SystemExecutor (62-line stub), FileOperator, ScriptRunner |
| 10 | **KnowledgeDaemon learns garbage** | 🔴 CRITICAL | No relevance filter; stores random Wikipedia facts |
| 11 | **Memory systems isolated** | 🟠 HIGH | SemanticGraph, HdcSemanticGraph, VectorStore, SDM don't talk |
| 12 | **SleepThread non-functional** | 🟠 HIGH | `visited=0 nodes=0 edges=190` every epoch |
| 13 | **Error handling missing** | 🟠 HIGH | Zero try/catch; silent failures everywhere |
| 14 | **~80% codebase is stubs** | 🔴 CRITICAL | 195+ files with placeholder code |

---

## 5. FILES GENERATED (Ready for Integration)

| File | Path | Status |
|------|------|--------|
| LocalLLM.h | `src/brain/language/LocalLLM.h` | ✅ Ready |
| LocalLLM.cpp | `src/brain/language/LocalLLM.cpp` | ✅ Ready |
| VseBootstrapTrainer.h | `src/brain/inference/VseBootstrapTrainer.h` | ✅ Ready |
| VseBootstrapTrainer.cpp | `src/brain/inference/VseBootstrapTrainer.cpp` | ✅ Ready |
| YUKI_PHASE0_PATCH.md | Patch doc | ⚠️ Partially applied |
| SHAPE_RESPONSE_FIX.md | Patch doc | ❌ NOT applied |
| SYSTEM_PROMPT_FIX.md | Patch doc | ❌ NOT applied |
| VSE_INTEGRATION_PATCH.md | Patch doc | ❌ NOT applied |

---

## 6. WHAT NEEDS TO BE DONE NEXT

### IMMEDIATE (Next Session — P0 Blocking Fixes)

**6.1 Fix `shape_response()` — Move LLM Before Early Returns**
- Move LLM generation code to TOP of `shape_response()`, before `!can_act` check
- When `can_act=0` but LLM succeeds, use LLM response anyway (LLM naturally asks for clarification)
- When LLM fails, fall back to existing template logic
- **File:** `src/brain/predictive/predictive_turn_engine.cpp`

**6.2 Fix Response Truncation — Skip ResponseShaper for LLM Text**
- When `used_llm=true`, set `r.response_text = base` directly
- Only call `ResponseShaper::apply()` for knowledge-fallback paths
- **File:** `src/brain/predictive/predictive_turn_engine.cpp`

**6.3 Fix System Prompt Leaking — Use Ollama `system` Field**
- In `LocalLLM::generate()`, split prompt into `system` + `prompt` JSON fields
- Don't prepend instructions to user input
- **File:** `src/brain/language/LocalLLM.cpp`

**6.4 Fix LLM Timeout — Increase to 60s + Add Retry**
- Cold-start on CPU takes 20-30s for first inference
- Add 1 retry with exponential backoff
- **File:** `src/brain/language/LocalLLM.cpp`

**6.5 Filter Mass Curriculum from AIR Context**
- In `shape_response()`, skip retrieved context lines starting with `[mass_curriculum]`
- Or: fix AIR retrieval to exclude mass curriculum tags
- **File:** `src/brain/predictive/predictive_turn_engine.cpp`

### SHORT-TERM (Following Sessions — P1 Training)

**6.6 Integrate VseBootstrapTrainer**
- Wire into BabyMode constructor after VSE instantiation
- Call `injectAll()` once at startup
- Verify `prec.intent` rises from 0.07
- **Files:** `src/BabyMode.h`, `src/BabyMode.cpp`, `CMakeLists.txt`

**6.7 Verify EMA Learning from Real Conversation**
- After bootstrap, each turn should call `updateMapping()` with real observation + MAP intent
- Monitor `prec.intent` over 50+ turns
- **File:** `src/brain/inference/VariationalStateEstimator.cpp`

**6.8 Cap Surprise Accumulation**
- Add `std::min(surprise, SURPRISE_MAX)` to prevent unbounded growth
- Or: improve generative model so surprise naturally stays low
- **File:** `src/brain/predictive/predictive_turn_engine.cpp`

### MEDIUM-TERM (Weeks — P2 Architecture)

**6.9 Implement Execution Layer**
- Complete SystemExecutor, FileOperator, ScriptRunner
- Wire into TaskSystem output
- **Files:** `src/brain/SystemExecutor.cpp`, `src/brain/FileOperator.cpp`, `src/brain/ScriptRunner.cpp`

**6.10 Fix KnowledgeDaemon Relevance Filtering**
- Add semantic similarity check before storing facts
- Use HdcSemanticGraph to verify relevance to conversation topics
- **Files:** `src/brain/learning/KnowledgeDaemon.cpp`, `src/brain/SmartScraper.cpp`

**6.11 Fix SleepThread Consolidation**
- Debug why `visited=0` despite `edges=190`
- Ensure episodic → semantic → procedural promotion works
- **File:** `src/brain/sleep/SleepThread.cpp`

**6.12 Add Error Handling Framework**
- Implement `Result<T,E>` type
- Wrap I/O operations in try/catch
- Add structured logging
- **New files:** `src/core/Result.h`, `src/core/Logger.h`

---

## 7. GEMINI PROMPT FOR NEXT SESSION

Below is the complete prompt to paste into Gemini for the next session. It includes all P0 fixes with exact file paths, ADD/REPLACE/REMOVE markers, and wire logic.

---
