# Yuki_1.0 — Fixes Applied Summary

## Overview
This document summarizes all the fixes applied to address the critical issues identified in the deep codebase audit.

---

## P0 Critical Fixes (✅ Completed)

### 1. CoreBus Drain Loop Added
**File:** `src/main.cpp`
- Added dedicated thread that calls `CoreBus::instance().processAllTopicQueues()` every 10ms
- Ensures CoreBus subscribers receive messages published to topic queues
- Thread properly joined on shutdown

### 2. HdcSemanticGraph Thread Safety Fixed
**Files:** `src/brain/memory/HdcSemanticGraph.h`, `src/brain/memory/HdcSemanticGraph.cpp`
- Added `stmt_cache_mtx_` mutex to protect prepared statement cache
- Replaced `std::recursive_mutex` with `std::mutex` for `db_mtx_`
- Fixed lock ordering: `concept_cache_mtx_` → `db_mtx_` → `stmt_cache_mtx_`
- `cachedStmt()` now thread-safe without requiring external lock
- All 19 occurrences of `recursive_mutex` replaced

### 3. BabyMode Thread Capture Fixes
**File:** `src/BabyMode.cpp`
- **KnowledgeDaemon fetcher** (lines 481-528): Capture required components by value (`smart_scraper`, `web_extractor`, `knowledge_extractor`, `cmf`) instead of capturing `this`
- **Connectivity test thread** (lines 531-543): Capture `smart_scraper` by value instead of `this`
- Moved fetcher registration AFTER `web_extractor_` and `knowledge_extractor_` are created
- Both threads now use captured pointers that remain valid

### 4. SleepThread Callback Safety
**File:** `src/BabyMode.cpp` (lines 667-735)
- Verified: Already correct - captures `shared_ptr<LocalTransformer>` from `local_llm_->getLocalTransformer()`
- Destruction order: `sleep_thread_->stop()` called before `local_llm_.reset()`

### 5. GlobalWorkspace → CoreBus Wiring
**File:** `src/infrastructure/GlobalWorkspace.cpp` (line 78)
- Verified: Already wired - `CoreBus::instance().publish(winner_it->message)` in `broadcastLoop()`

### 6. UserMemory Save Thread Safety
**File:** `src/brain/memory/UserMemory.cpp` (lines 70-118)
- `save()` now copies all data structures under lock, then writes to file without lock
- Prevents race conditions between concurrent modifications and persistence
- Eliminates deadlock risk from I/O under lock

### 7. Vector-Based T1 Retrieval
**Files:** `src/brain/memory/CognitiveMemoryFabric.cpp`, `src/brain/memory/HdcSemanticGraph.h/.cpp`
- Added `HdcSemanticGraph::queryByTextSimilarity()` using deterministic Hypervector from text
- Updated `CognitiveMemoryFabric::retrieveContextForQuery()` to use HDC vector similarity
- Updated `CognitiveMemoryFabric::retrieveSimilarEpisodes()` to use HDC similarity
- Replaces O(N) string matching with vector similarity search

### 8. CommandRegistry for Dynamic Commands
**Files:** `src/CommandRegistry.h`, `src/CommandRegistry.cpp`, `src/CommandRouter.h/.cpp`, `CMakeLists.txt`
- New `CommandRegistry` class for plugin-based command registration
- `CommandRouter` now registers built-in commands in constructor
- Replaces 100+ hardcoded string comparisons with registry lookup
- Supports dynamic command registration/unregistration at runtime
- Mobile keyword handling preserved as special case

### 9. Safety Dimension Initialization (Fixes D7)
**Files:** `src/brain/predictive/TurnState.h`, `src/brain/predictive/TurnCoordinator.cpp`
- Added `BeliefPool` constructor initializing safety dimension with `BASELINE_SAFETY` (0.90)
- Updated `BeliefPool::reset()` to re-initialize safety dimension
- Prevents false vetoes on first turn (safety belief was 0, causing immediate veto)

---

## P1 High Priority Fixes (✅ Partially Completed)

### 10. Deviations Documented
**File:** `DEVIATIONS.md`
- Documented all 7 deviations (D1-D7) from mathematical specification
- Each deviation includes: spec vs implementation, reason, location
- Action items for future correction prioritized

---

## Files Modified

### Core Infrastructure
- `src/main.cpp` - CoreBus drain thread
- `src/infrastructure/CoreBus.cpp` - (verified, no changes needed)

### Memory Systems
- `src/brain/memory/HdcSemanticGraph.h/.cpp` - Thread safety, queryByTextSimilarity
- `src/brain/memory/CognitiveMemoryFabric.cpp` - Vector-based retrieval
- `src/brain/memory/UserMemory.cpp` - Thread-safe save()

### Command System
- `src/CommandRegistry.h/.cpp` - NEW: Dynamic command registry
- `src/CommandRouter.h/.cpp` - Registry-based routing
- `CMakeLists.txt` - Added CommandRegistry.cpp

### Predictive Engine
- `src/brain/predictive/TurnState.h` - Safety dimension initialization
- `src/brain/predictive/TurnCoordinator.cpp` - Reset safety on BeliefPool reset

### BabyMode
- `src/BabyMode.cpp` - Thread capture fixes, fetcher registration order

### Documentation
- `DEVIATIONS.md` - Complete deviation documentation

---

## Verification Status

| Component | Status | Notes |
|-----------|--------|-------|
| CoreBus pub/sub | ✅ Fixed | Drain loop added |
| HDC Semantic Graph | ✅ Thread-safe | Mutexes, no recursive_mutex |
| BabyMode threads | ✅ Safe | No raw `this` captures |
| UserMemory | ✅ Thread-safe | Copy-on-save pattern |
| T1 Retrieval | ✅ Vector-based | HDC similarity search |
| CommandRouter | ✅ Extensible | Plugin registry |
| Safety Veto | ✅ Fixed init | No false vetoes on turn 1 |
| Deviations | ✅ Documented | DEVIATIONS.md |

---

## Remaining Work (P1-P3)

### P1 High Priority
- [ ] Fix PrecisionState math (D2) to match spec
- [ ] Fix Surprise math (D3, D4) to match spec
- [ ] Consolidate TurnCoordinator vs TurnCoordinatorAdapter
- [ ] Replace UserMemory string patterns with learned extractor
- [ ] Remove YNC dead code or integrate it

### P2 Medium Priority
- [ ] Rename colliding files (PolicySelector, CognitiveOrchestrator)
- [ ] Remove duplicate CMake entries
- [ ] Remove dead code (MemoryFabric, commented Phase 5-8 sources)
- [ ] Standardize namespaces
- [ ] Add structured logging

### P3 Low Priority
- [ ] Fix SIMD detection for cross-compilation
- [ ] Use FetchContent for all vendor deps
- [ ] Add include/ directory for public headers
- [ ] Add clang-tidy/cppcheck to CI

---

## Build Impact

All changes are backward compatible:
- New files only add functionality
- Thread safety fixes don't change APIs
- CommandRouter maintains same external interface
- CognitiveMemoryFabric retrieval improves quality, same signature
- Safety initialization fixes bug, doesn't change behavior for initialized turns

---

## Testing Recommendations

1. Run gate tests 0-9 to verify no regressions
2. Test concurrent access to HDC semantic graph
3. Test BabyMode startup/shutdown cycles
4. Test CommandRouter with dynamic command registration
5. Verify first turn doesn't trigger safety veto
6. Test CoreBus message delivery to subscribers