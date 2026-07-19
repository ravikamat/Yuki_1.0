# YUKI v1.0 — Project State & Roadmap
> Consolidated from session handoff | Last updated: 2026-07-19

---

## Table of Contents
1. [Quick Reference](#quick-reference)
2. [Active Issues](#active-issues)
3. [Feature Roadmap](#feature-roadmap)
4. [Architecture Overview](#architecture-overview)
5. [Build Status](#build-status)
6. [Session History](#session-history)

---

## Quick Reference

| Item | Status |
|------|--------|
| MSVC Build | Clean, zero warnings |
| Tests | 17/17 passing (4 AIR + 13 TurnCoordinator) |
| Runtime | Live — perceives, remembers, infers, retrieves, dreams, learns |
| CMF T1–T3 | Fully wired |
| AIR Retrieval | Live |
| DMC Consolidation | Active in sleep thread |
| 9-Topic Curriculum | Loaded |
| PresenceShell | Glass-acrylic UI with cognitive thinking strip |

---

## Active Issues

> Sorted by severity. Each issue includes status, risk level, and proposed fix.

### 🔴 P0 — Screen Viewing Crash
- **Description:** Build exits immediately when screen viewing starts.
- **Status:** NOT YET INVESTIGATED
- **Risk:** HIGH — unhandled exception in screen capture path
- **Repro:** Start screen viewing → crash
- **Proposed Fix:** Capture stack trace, audit screen capture pipeline, add exception guards
- **ETA:** 1–2 days

### 🟡 P1 — Sleep Thread Epochs Too Frequent
- **Description:** Sleep epochs fire every ~7 seconds despite `idle_threshold=30s`.
- **Status:** Known, not fixed
- **Risk:** MEDIUM — DMC consolidation gets 0 outcomes (needs 10). Not enough data accumulated between epochs.
- **Proposed Fix:** Increase `idle_threshold` or batch more data before sleep triggers
- **ETA:** 1 day

### 🟡 P1 — STT Retry Failures
- **Description:** Log shows `"STT retry 1/3... 2/3... 3/3"` then `"Voice STT — Standby"`.
- **Status:** Known, not fixed
- **Risk:** MEDIUM — EdgeTTS backend online but STT capture failing
- **Proposed Fix:** Check microphone permission / WASAPI device enumeration
- **ETA:** 1 day

### 🟢 P2 — PresenceShell Blinking (Partially Fixed)
- **Description:** UI flickers during mode changes (clarification/thinking toggle).
- **Status:** Timer slowed (80ms → 120ms), invalidate restricted to strip rect, layout cache added
- **Risk:** LOW — may still flicker on rapid mode toggles
- **Proposed Fix:** If unstable, implement double-buffering or DWM composition flags
- **ETA:** If needed, 1 day

### 🟢 P2 — "HI" → Clarification (Fixed)
- **Description:** Simple greetings incorrectly triggered contested-intent clarification.
- **Status:** FIXED — Phatic fast-path added to `predictive_turn_engine.cpp`
- **Risk:** LOW — bypass only triggers if TextEncoder intent label is exactly `"greeting"`
- **Proposed Fix:** Verify TextEncoder heuristic → intent label mapping includes all phatic variants
- **ETA:** 0.5 day verification

---

## Feature Roadmap

> Ordered by recommended implementation sequence. ETA estimates are rough.

### Phase A — Stability & Crashes
| # | Feature | Description | ETA | Depends On |
|---|---------|-------------|-----|------------|
| A1 | **Screen Viewing Crash Fix** | Stack trace + audit screen capture pipeline + exception guards | 1–2 days | — |
| A2 | **STT Fix** | WASAPI device enumeration + permission checks + retry backoff | 1 day | A1 |
| A3 | **Sleep Epoch Tuning** | Increase idle threshold, batch data before DMC consolidation | 1 day | A2 |

### Phase B — Memory & Archive Hardening
| # | Feature | Description | ETA | Depends On |
|---|---------|-------------|-----|------------|
| B1 | **T4 ArchiveWriter** | Sleep consolidates → archive immutable history via Merkle-DAG + Parquet | 2–3 days | A3 |
| B2 | **Auto-Promotion T1→T2→T3** | Policy layer deciding when to promote episodic → semantic → procedural | 3–4 days | B1 |
| B3 | **SDM Scale Stress Test** | Push SDM to 100K–1M+ vectors, measure retrieval latency | 3–4 days | B2 |
| B4 | **CSV Replacement** | Audit all `*.csv` writers, migrate to CMF native storage | 1 week | B3 |

### Phase C — Learning & Model Quality
| # | Feature | Description | ETA | Depends On |
|---|---------|-------------|-----|------------|
| C1 | **Real GenerativeModel** | Replace EMA with EM/gradient descent on (observation, intent) pairs | 2–3 weeks | B4 |
| C2 | **Multi-Modal Fusion Learning** | Train fusion weights end-to-end instead of heuristic | 2–3 weeks | C1 |
| C3 | **Logging System Cleanup** | Replace `printf` with `YukiUtils::log` across codebase | 3–4 days | B4 |

### Phase D — Infrastructure & Polish
| # | Feature | Description | ETA | Depends On |
|---|---------|-------------|-----|------------|
| D1 | **Vendor Stub Queue** | moodycamel lock-free queue integration | 2–3 days | C3 |
| D2 | **SecuritySandbox** | Sandboxed execution for ToolExecutor | 1 week | D1 |
| D3 | **AvatarRenderer v2** | Full-body procedural anime character with spring physics, lip sync, mood expressions, eye tracking | 1 week | D1 |

---

## Architecture Overview

YUKI v1.0 is a closed-loop Active Inference cognitive OS in C++ with three planes:

### Perception Layer
- **AudioEncoder** — FFT + MFCC + YIN pitch
- **TextEncoder** — Word2Vec + JL projection, 9 heuristic scores
- **VisualEncoder** — HOG + JL projection
- **SignalConditioningLayer** — 50ms window, SNR/dropout/artifact detection
- **MultiModalFusionGate** — Cross-modal agreement scoring

### Memory Layer (CMF 5-Tier)
| Tier | Name | Storage | Latency | Capacity |
|------|------|---------|---------|----------|
| T0 | Working | RAM / VSE posterior | <1 µs | Session |
| T1 | Episodic | SDM + LSH + Hypervector | <1 ms | ~1 GB |
| T2 | Semantic | SQLite + HDC Knowledge Graph + HNSW | <5 ms | ~100 GB |
| T3 | Procedural | Binary blobs + DMC TinyMLP (48→128→24, REINFORCE) | <10 ms | ~10 GB |
| T4 | Archive | Merkle-DAG + Parquet | >100 ms | Infinite |

### Inference Layer
- **VariationalStateEstimator** — 24 states (Intent × Engagement × Urgency)
- **FreeEnergyCalculator** — Expected free energy G(π) for policy selection
- **PolicySelector** — 7 safety constraints (C1–C7), fallback hierarchy
- **PrecisionEngine** — Per-sensor per-dimension precision from prediction error
- **GenerativeModel** — EMA online learning (lr=0.05), anti-overfitting decay

### Predictive Layer
- **TurnCoordinator** — Template routing, zero hardcoded strings
- **ActiveInferenceRetrieval (AIR)** — KL-divergence retrieval, NOT cosine similarity
- **ClarificationEngine** — Contested intent detection (threshold 0.65)

### Learning Layer
- **BackgroundLearningEngine** — 24/7 thread, 0.5 samples/sec
- **KnowledgeDaemon** — Python + Scrapling web fetch
- **MassCurriculumLoader** — 9-topic bootstrap, `.mass_complete` flag, self-destructing
- **AutoCurriculum** — DELETED (constitutional P1 violation resolved)

### Sleep Layer
- **SleepThread** — 7 sub-tasks including DMC consolidation, counterfactual replay
- **MemoryDistiller** — Vector index persistence
- **ArchiveWriter** — Merkle-DAG epoch finalization

### Constitutional Layer (5 Laws)
| Law | Status |
|-----|--------|
| P1 Never Commit Early | Resolved — no hardcoded strings in templates |
| P2 Never Generate Without Grounding | Resolved — all outputs VSE-driven |
| P3 Every Turn Teaches | Resolved — EMA learning + training log |
| P4 Know Thy Ignorance | Resolved — contested intent → clarification |
| P5 Thou Shalt Not Deceive Thyself | Resolved — thresholds constexpr, documented |

---

## Build Status

```
MSVC:     Clean, zero warnings
Tests:    17/17 passing (4 AIR + 13 TurnCoordinator)
Runtime:  Yuki is running — perceives, remembers, infers, retrieves, dreams, learns
```

---

## Session History

### 2026-06-02 — PresenceShell Glass-Acrylic Rewrite
**Completed:**
1. Bottom-up layout anchoring (fixes clarification panel crush)
2. 5-layer cognitive thinking strip (Sense → Recall → Think → Choose → Speak)
3. Pulsing animation @ 8fps with restricted invalidate (strip rect only)
4. Static cache for `layoutChildren` — no full redraw on every keystroke
5. Professional single-row progress bar design (6px bar, 8px labels)
6. Hover-aware detail tooltip showing active layer description
7. GDI+ `AddRectangle` fix (`Gdiplus::Rect` wrapper)
8. Timer 2 slowed from 80ms → 120ms for smoother CPU usage

**Files Modified:**
- `src/PresenceShell.h`
- `src/PresenceShell.cpp`
- `src/brain/predictive/predictive_turn_engine.h`
- `src/brain/predictive/predictive_turn_engine.cpp`
- `src/BabyMode.h`
- `src/BabyMode.cpp`
- `CMakeLists.txt`

### 2026-05-28 — CMF Phase 1.5 & 2 Complete
**Completed:**
- HNSW vector similarity search wired into EpisodicStore
- TurnCoordinator retrieves context from CMF
- Noun phrase extraction (multi-word), relation inference (is_a, requires, causes, part_of)
- Hebbian reinforcement + decay
- MassCurriculumLoader with 9-topic bootstrap and `.mass_complete` flag
- Self-destructing `unique_ptr`, `.gitignore` for CMF data

### 2026-05-27 — Yuki v1.0 Foundation
**Completed:**
- Phase 0 Foundation (CoreBus, StatePlane, ControlPlane, GlobalWorkspace, ActiveInferenceCore)
- Variational State Estimator (24 factorized states, continuous policy space)
- Signal Conditioning + Observation Encoder layers
- Precision Engine with real computations
- PolicySelector with 7 safety constraints

---

## Active vs. Stub

| Component | Status |
|-----------|--------|
| Perception (Audio/Text/Visual Encoders) | **ACTIVE** |
| Signal Conditioning + Fusion Gate | **ACTIVE** |
| CMF T1–T3 (Episodic/Semantic/Procedural) | **ACTIVE** |
| AIR Retrieval | **ACTIVE** |
| DMC Consolidation | **ACTIVE** |
| 9-Topic Curriculum | **ACTIVE** |
| PresenceShell + Thinking Strip | **ACTIVE** |
| T4 ArchiveWriter | **STUB** |
| Auto-Promotion T1→T2→T3 | **STUB** |
| SDM 100K+ Scale Test | **STUB** |
| CSV → CMF Migration | **STUB** |
| Real GenerativeModel (EM/gradient descent) | **STUB** |
| Multi-Modal Fusion Learning | **STUB** |
| Logging Cleanup (`printf` → `YukiUtils::log`) | **STUB** |
| moodycamel Vendor Queue | **STUB** |
| SecuritySandbox for ToolExecutor | **STUB** |
| AvatarRenderer v2 (Full Body) | **IN PROGRESS** |

---

## Wiring: Cognitive Thinking Strip

`TurnCoordinator::shape_response()` drives the strip:

```
Step 1 (Sense):     [████░░░░░░] Sense  "Perceiving multi-modal input..."
Step 2 (Recall):    [██░░░░░░░░] Recall "Retrieving episodic context..."
Step 3 (Think):     [░░░░░░░░░░] Think  "Intent inferred: greeting"
Step 4 (Choose):    [░░░░░░░░░░] Choose "Policy selected"
Step 5 (Speak):     [░░░░░░░░░░] Speak  "Formulating response..."
Done:               (strip auto-clears)
```

- **Colors:** Teal → Mint → Amber → Plum → Sky
- **Pulse:** Sine wave @ 120ms timer (~8fps) for active layers
- **Hover:** Shows detail tooltip + all layer labels

---

## User Instructions for Next Session

1. Read `D:\Yuki_1.0\status.md` at session start (single source of truth)
2. Do NOT save per-chat snippets to memory — only this consolidated state
3. After every task: build + test + run `D:\Yuki_1.0\log_status.ps1`
4. Before every new command: memorize previous chat context for continuity
5. Format Gemini prompts as: complete code for new files, ADD/REPLACE/REMOVE for existing files, always include wire logic

---

## 2026-07-19 — Digital Organism Phase 1: Survival + Motivation Layers

**Added (`src/brain/organism/`):**
- `MetabolismEngine` — power/compute/storage/network budgets, starvation detection ("electricity as food")
- `EconomyEngine` — credit ledger: income (tasks, proactive help, discovery, optimization × reputation), expenses (upkeep, inference, storage, network, sleep), penalties (failure, rejection, exhaustion, overflow, atrophy), upgrades (larger model, faster inference, more memory, better sensors, new tools, compute redundancy)
- `DriveSystem` — homeostasis / curiosity / social / competence drives → Global Affect State (urgency, contentment, restlessness) → goal proposals
- `OrganismController` — organism loop: tick metabolism + upkeep + drives, life-event hooks, sleep consolidation gated by earned credits, proactive action selection
- `tests/test_organism_core.cpp` — 9 unit tests

**Next:** wire OrganismController into BabyMode/TurnCoordinator, make SleepThread pay the consolidation cost, bias PolicySelector with AffectState.

---

*End of consolidated project state.*
