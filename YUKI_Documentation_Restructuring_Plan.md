# YUKI v1.0 — Documentation Triad Restructuring Plan
> **Date:** 2026-07-22
> **Authority:** `yuki_flow.md` is the single source of truth for all operational logic
> **Scope:** Restructure `YUKI_ROADMAP.md` + `project_files_documentation.md` + `yuki_flow.md` into a clean, non-overlapping documentation architecture

---

## ⚠️ INSTRUCTION FOR GEMINI (Code Writer)
> Execute this plan atomically. For every file you modify:  
> 1. **Read the entire file first** (Rule 9)  
> 2. **Cross-reference against `yuki_flow.md`** — if any statement contradicts the authoritative flow, wrap it in `~~strikethrough~~` and append `[DISCARDED — see yuki_flow.md]`  
> 3. **Move** content that belongs in another file (don't copy — move)  
> 4. **Delete** content marked [DELETE] after striking it  
> 5. Report an audit table of every change

---

# SECTION 1: LEFTOVER / ORPHANED CONTENT
> The following content does NOT belong in any of the three existing files.  
> It must be either moved to a new file, moved to an existing but wrong file, or permanently discarded.

## 1.1 Content That Needs a NEW File

| Content | Current Location | Target New File | Reason |
|:---|:---|:---|:---|
| Session History (2026-07-20, 2026-07-19, 2026-06-02, 2026-05-28) | `YUKI_ROADMAP.md` §7 | `CHANGELOG.md` | Historical session logs are not roadmap content |
| Gate Verification Results (10-turn run, Gates A-J) | `YUKI_ROADMAP.md` §2 | `test/reports/verification_2026-07-19.md` | Test results belong in test reports, not roadmap |
| Digital Organism Manifesto (living organism analogy, resource economy philosophy, phase plan table, honest limitations, roadmap options 1-5) | `project_files_documentation.md` — "Legacy Codebase File Catalog" preamble | `DESIGN_PHILOSOPHY.md` | Philosophical/motivational content is not technical documentation |
| Heuristics & Hardcoded Mappings — RESOLVED issues (stream_workers, TextEncoder, EntityProcessor, MultiModalFusionGate, DocReader, FileOperator, BackgroundAgents) | `project_files_documentation.md` — "Heuristics, Hardcoded, Stubs" | `CHANGELOG.md` under "Resolved Issues" | Resolved issues belong in changelog, not living documentation |

## 1.2 Content That Must Be PERMANENTLY DISCARDED

| Content | Current Location | Why Discard |
|:---|:---|:---|
| Old Build Plan: Phase A (Emergency Fixes), Phase B (Code Skills), Phase C (Self-Correction), Phase D (Organism Polish) | `YUKI_ROADMAP.md` §5 | **Predates M3/M3.5 architecture.** The milestone-based plan (M0→M8) in §1 replaces this entirely. The old phases are based on a pre-research-planner mental model. |
| Component Diagnosis: "Active vs Stub" table with SystemExecutor, FileOperator, ScriptRunner, ArchiveWriter, SleepThread, APIAdapter, CodeWriter, SelfCorrectionLoop | `YUKI_ROADMAP.md` §3.2 | **Stale diagnosis.** These stubs are being replaced by M3 (ResearchAgent) and M3.5 (TestOrchestrator). The file catalog in `project_files_documentation.md` already tracks stub status per-file. Duplicated and outdated. |
| Active Issues: P0 Screen Crash, P1 STT Retry, P2 PresenceShell Blinking, P2 test_predictive_turn_engine | `YUKI_ROADMAP.md` §4 | **Issue tracker content.** Roadmap should reference issue IDs, not host full issue descriptions. Move to issue tracker or `KNOWN_ISSUES.md`. |
| "Baseline Consolidated Status & Diagnosis" entire section | `YUKI_ROADMAP.md` — everything below milestone table up to §8 | **Snapshot-in-time diagnosis.** This is a session handoff artifact, not a living document. The milestone table + file catalog already communicate status. |

## 1.3 Content That Must Be MOVED (Not Copied) Between Existing Files

| Content | Move From | Move To |
|:---|:---|:---|
| 19 Cognitive Stages table | `YUKI_ROADMAP.md` §8 + `project_files_documentation.md` end | `yuki_flow.md` — append as new §16 |
| Phatic Fast-Path Rule | `YUKI_ROADMAP.md` §8 + `project_files_documentation.md` end | `yuki_flow.md` — append to §5.2 or new §16 |
| Master System Overview ASCII diagram | `project_files_documentation.md` §1 | `yuki_flow.md` §1 (replace mermaid if needed, but mermaid is canonical) |
| Phase 8 (M3) summary + architecture diagram | `project_files_documentation.md` §8 | `yuki_flow.md` §8 (expand to full detail) |
| Phase 9 (M3.5) summary + architecture diagram | `project_files_documentation.md` §9 | `yuki_flow.md` §9 (expand to full detail) |
| Condition Matrix COND-01..COND-20 | `project_files_documentation.md` §14 | `yuki_flow.md` §14 (already there — delete duplicate from proj_docs) |
| File & Subsystem Tracing Index (22 canonical items) | `yuki_flow.md` §15 | Keep in yuki_flow.md. The 52-item version in proj_docs is the SUPerset and stays there. |

---

# SECTION 2: FILE-BY-FILE RESTRUCTURING PLAN

---

## FILE A: `yuki_flow.md` — AUTHORITATIVE OPERATIONAL SPECIFICATION
> **Rule:** This file contains ONLY formal logic, flows, formulas, decision matrices, data structures, and cognitive stage definitions.  
> **Rule:** NO project status. NO file catalogs beyond the 22-core tracing index. NO history. NO philosophy.  
> **Rule:** This is the authority. All other files reference it.

### 2A.1 KEEP AS-IS (Already Canonical)

| Section | Status |
|:---|:---|
| §1 Master System Overview (mermaid) | ✅ KEEP |
| §2 Phase 1: Bootstrapping | ✅ KEEP |
| §3 Phase 2: Sensory Acquisition | ✅ KEEP |
| §4 Phase 3: Active Inference (formulas, text_obs, PrecisionPredictor) | ✅ KEEP |
| §5 Phase 4: Memory Retrieval + §5 Policy Selection | ✅ KEEP |
| §6 Phase 6: Response Synthesis | ✅ KEEP |
| §7 Phase 7: Metacognition | ✅ KEEP |
| §8 Phase 8: M3 ResearchPlanner (7-stage loop) | ✅ KEEP |
| §9 Phase 9: M3.5 UTO (5-wave DAG) | ✅ KEEP |
| §10 Phase 10: M2 Code Synthesis | ✅ KEEP |
| §11 Phase 11: Organism Homeostasis | ✅ KEEP |
| §12 Phase 12: Sleep Thread | ✅ KEEP |
| §14 Exhaustive Condition Matrix (COND-01..COND-20) | ✅ KEEP |
| §15 File & Subsystem Tracing Index (22 items) | ✅ KEEP |

### 2A.2 ADD (Missing Content from Other Files)

| New Section | Source | Action |
|:---|:---|:---|
| §13 M3 + M3.5 File Inventory | `YUKI_ROADMAP.md` §4.3 + §5.3 | **MOVE** the new/modified file lists and test inventories into yuki_flow.md as a new §13. This is structural specification, not project status. |
| §16 Cognitive Architecture: 19 Stages | `YUKI_ROADMAP.md` §8 + `project_files_documentation.md` end | **MOVE** the 19-stage table here. It is the formal cognitive pipeline definition. |
| §17 Phatic Fast-Path Rule | `YUKI_ROADMAP.md` §8 + `project_files_documentation.md` end | **MOVE** the phatic rule here. It is formal logic. |
| §18 Build & Test Constraints (14 rules) | `YUKI_ROADMAP.md` §11 | **COPY** the critical constraints cheat sheet. These are build-system rules, not milestone status. |

### 2A.3 MODIFY

| Section | Change |
|:---|:---|
| §8 Phase 8 | Expand from summary to full detail using `project_files_documentation.md` §8.1-8.5 content. Add SubGoal/PlanNode/KnowledgePack structs. |
| §9 Phase 9 | Expand from summary to full detail using `project_files_documentation.md` §9.1-9.5 content. Add HistoricalDataReplay, ABTestFramework, SmartTestSelector structs. |

### 2A.4 RESULTING STRUCTURE

```
yuki_flow.md
├── §1  Master System Overview
├── §2  Phase 1: Bootstrapping
├── §3  Phase 2: Sensory Acquisition
├── §4  Phase 3: Active Inference
├── §5  Phase 4: Memory Retrieval & Policy Selection
├── §6  Phase 6: Response Synthesis
├── §7  Phase 7: Metacognition
├── §8  Phase 8: M3 ResearchPlanner (FULL DETAIL)
├── §9  Phase 9: M3.5 UTO (FULL DETAIL)
├── §10 Phase 10: M2 Code Synthesis
├── §11 Phase 11: Organism Homeostasis
├── §12 Phase 12: Sleep Thread
├── §13 M3 + M3.5 File & Test Inventory
├── §14 Exhaustive Condition Matrix
├── §15 File & Subsystem Tracing Index (22 core)
├── §16 The 19 Cognitive Stages
├── §17 Phatic Fast-Path Rule
└── §18 Build & Test Constraints (14 rules)
```

---

## FILE B: `YUKI_ROADMAP.md` — MILESTONE TRACKER & PROJECT STATUS
> **Rule:** This file contains ONLY milestone status, build targets, test coverage, forward compatibility, open questions, and high-level constraints.  
> **Rule:** NO architecture explanation beyond summaries (reference yuki_flow.md). NO file-by-file catalog. NO session history. NO diagnosis snapshots.

### 2B.1 KEEP (After Cleanup)

| Section | Status | Notes |
|:---|:---|:---|
| §1 Milestone Status Overview Table | ✅ KEEP | Fix test count to 16/16 consistently |
| §2 M3 ResearchPlanner — Architecture Summary | ✅ KEEP | Keep as **summary only**. Remove all data structure definitions (moved to yuki_flow.md). Keep status, file counts, test counts. |
| §3 M3.5 UTO — Architecture Summary | ✅ KEEP | Keep as **summary only**. Same as above. |
| §4 Forward Compatibility Matrix | ✅ KEEP | M4-M8 enablement |
| §5 Critical Constraints | ✅ KEEP | 14 non-negotiable rules |
| §6 Open Questions | ✅ KEEP | 5 unresolved decisions |

### 2B.2 STRIKE THROUGH + [DISCARDED]

| Section | Action | Replacement |
|:---|:---|:---|
| ~~"Baseline Consolidated Status & Diagnosis"~~ | **STRIKE entire section** | Nothing. Replaced by milestone table. |
| ~~§2 Gate Verification Results~~ | **STRIKE** | Move to test reports or CHANGELOG.md |
| ~~§3 Component Diagnosis (Active vs Stub)~~ | **STRIKE** | File catalog in proj_docs is authoritative for per-file status |
| ~~§4 Active Issues & Resolutions~~ | **STRIKE** | Move to issue tracker or KNOWN_ISSUES.md |
| ~~§5 Feature Roadmap & Complete Build Plan (Phase A-D)~~ | **STRIKE** | Replaced by M-milestone architecture. Add note: `~~Old phase-based plan~~ [DISCARDED — see yuki_flow.md Milestone Table]` |
| ~~§6 Architecture & Constitutional Layer~~ | **STRIKE** | Already in yuki_flow.md §5, §7, §18 |
| ~~§7 Session History~~ | **STRIKE** | Move to CHANGELOG.md |
| ~~§8 Cognitive Architecture Logic Flow (19 stages)~~ | **STRIKE** | Moved to yuki_flow.md §16 |
| ~~§8 Phatic Fast-Path Rule~~ | **STRIKE** | Moved to yuki_flow.md §17 |

### 2B.3 ADD

| New Section | Source | Action |
|:---|:---|:---|
| "How to Read This Document" | New | Add a paragraph: "For architecture details, see `yuki_flow.md`. For file-level implementation status, see `project_files_documentation.md`." |
| "Last Session Summary" | Handoff doc | Add a 3-bullet summary of the 2026-07-22 session (M3+M3.5 architecture finalized) |

### 2B.4 RESULTING STRUCTURE

```
YUKI_ROADMAP.md
├── Header (Gemini instruction)
├── "How to Read This Document" (NEW)
├── §1 Milestone Status Overview
│   └── Table: M0 ✅ → M8 🔴
│   └── Test coverage: 16/16 → 31/31 target
├── §2 M3 ResearchPlanner — Summary
│   └── Core principle, 7-stage loop (summary)
│   └── File count: 19 new + 9 modified
│   └── Test count: 5 new
├── §3 M3.5 UTO — Summary
│   └── Core principle, 6 techniques (summary)
│   └── File count: 13 new + 4 modified
│   └── Test count: 5 new
├── §4 Forward Compatibility Matrix (M4-M8)
├── §5 Critical Constraints (14 rules)
├── §6 Open Questions
└── §7 Last Session Summary (NEW)
```

---

## FILE C: `project_files_documentation.md` — FILE CATALOG & SUBSYSTEM INDEX
> **Rule:** This file contains ONLY the complete file-by-file catalog, wiring diagrams, data flow mappings, and per-component status.  
> **Rule:** NO decision matrices (in flow). NO milestone planning (in roadmap). NO philosophical manifestos. NO cognitive stage definitions.

### 2C.1 KEEP (After Cleanup)

| Section | Status |
|:---|:---|
| Header + Quick Navigation Index | ✅ KEEP |
| "Legacy Codebase File Catalog & Data Flow Mapping" — the massive tables | ✅ KEEP | This is the core value of this file |
| Per-file tables: src/, src/brain/, src/brain/core/, src/brain/database/, etc. | ✅ KEEP |
| Heuristics, Hardcoded, Stubs section — BUT ONLY the **unresolved** items | ✅ KEEP | Remove resolved items (moved to CHANGELOG) |

### 2C.2 STRIKE THROUGH + [DISCARDED]

| Section | Action |
|:---|:---|
| ~~"YUKI as a Living Digital Organism" manifesto~~ | **STRIKE** — Move to `DESIGN_PHILOSOPHY.md` |
| ~~"The Architecture: Yuki as Digital Organism" diagram~~ | **STRIKE** — Move to `DESIGN_PHILOSOPHY.md` |
| ~~"The Resource Economy" full section~~ | **STRIKE** — Move to `DESIGN_PHILOSOPHY.md` |
| ~~"Phase Plan: From Now to Organism" table~~ | **STRIKE** — Outdated, replaced by M-milestones |
| ~~"Honest Limitations" table~~ | **STRIKE** — Move to `DESIGN_PHILOSOPHY.md` |
| ~~"Roadmap: Variational State Estimator & GenerativeModel Upgrades" (Options 1-5)~~ | **STRIKE** — Move to `DESIGN_PHILOSOPHY.md` or `FUTURE_RESEARCH.md` |
| ~~"Core Recommendation: Combined Dynamic Inference"~~ | **STRIKE** — Move to `FUTURE_RESEARCH.md` |
| ~~Master System Overview ASCII diagram~~ | **STRIKE** — Already in yuki_flow.md §1 |
| ~~Phase 1-12 summaries~~ | **STRIKE** — Already in yuki_flow.md |
| ~~Phase 8 (M3) summary~~ | **STRIKE** — Already in yuki_flow.md §8 |
| ~~Phase 9 (M3.5) summary~~ | **STRIKE** — Already in yuki_flow.md §9 |
| ~~Exhaustive Condition Matrix~~ | **STRIKE** — Already in yuki_flow.md §14 |
| ~~19 Cognitive Stages table~~ | **STRIKE** — Moved to yuki_flow.md §16 |
| ~~Phatic Fast-Path Rule~~ | **STRIKE** — Moved to yuki_flow.md §17 |
| ~~"Heuristics & Hardcoded Mappings — RESOLVED" subsection~~ | **STRIKE** — Move resolved items to CHANGELOG.md |
| ~~"Stubs & Architectural Gaps — RESOLVED" subsection~~ | **STRIKE** — Move resolved items to CHANGELOG.md |

### 2C.3 MODIFY

| Section | Change |
|:---|:---|
| File catalog tables | Add M3/M3.5 files to the catalog. Currently missing all `src/brain/research/` and `src/brain/testing/` entries. |
| PolicySelector path | Fix from `src/brain/inference/` to `src/brain/policy/` (contradiction #3) |
| Test count references | Fix 15/15 → 16/16 where mentioned |

### 2C.4 RESULTING STRUCTURE

```
project_files_documentation.md
├── Header (Gemini instruction)
├── Quick Navigation Index
├── §1 Project Motive (ONE paragraph only, linking to DESIGN_PHILOSOPHY.md)
├── §2 File Catalog: src/ (root)
├── §3 File Catalog: src/brain/
├── §4 File Catalog: src/brain/core/
├── §5 File Catalog: src/brain/database/
├── §6 File Catalog: src/brain/emotion/
├── §7 File Catalog: src/brain/inference/
├── §8 File Catalog: src/brain/language/
├── §9 File Catalog: src/brain/learning/
├── §10 File Catalog: src/brain/memory/
├── §11 File Catalog: src/brain/organism/
├── §12 File Catalog: src/brain/predictive/
├── §13 File Catalog: src/brain/reasoning/
├── §14 File Catalog: src/brain/research/ (NEW — M3 files)
├── §15 File Catalog: src/brain/retrieval/
├── §16 File Catalog: src/brain/security/
├── §17 File Catalog: src/brain/self/
├── §18 File Catalog: src/brain/selftest/
├── §19 File Catalog: src/brain/skills/
├── §20 File Catalog: src/brain/sleep/
├── §21 File Catalog: src/brain/synthesis/
├── §22 File Catalog: src/brain/testing/ (NEW — M3.5 files)
├── §23 File Catalog: src/infrastructure/
├── §24 File Catalog: src/input/
├── §25 File Catalog: src/scrapling/
├── §26 File Catalog: src/vendor/
└── §27 Heuristics, Hardcoded, Stubs & Gaps — UNRESOLVED ONLY
```

---

# SECTION 3: NEW FILES TO CREATE

| File | Content Source | Purpose |
|:---|:---|:---|
| `CHANGELOG.md` | `YUKI_ROADMAP.md` §7 (session history) + `project_files_documentation.md` "Heuristics RESOLVED" | Chronological log of resolved issues, session milestones, and design decisions |
| `DESIGN_PHILOSOPHY.md` | `project_files_documentation.md` manifesto + organism analogy + resource economy + limitations + VSE upgrade options | Non-technical design rationale, philosophical positioning, and long-term research directions |
| `KNOWN_ISSUES.md` | `YUKI_ROADMAP.md` §4 (active issues) | Living issue tracker for P0/P1/P2 items |

---

# SECTION 4: CONSOLIDATION AUDIT CHECKLIST FOR GEMINI

After restructuring, verify:

| # | Check | Pass Criteria |
|:---|:---|:---|
| 1 | Zero duplication across the 3 files | Search for identical paragraphs. Any match = fail. |
| 2 | yuki_flow.md has NO project status | No milestone tables, no test counts, no "planned/completed" labels |
| 3 | YUKI_ROADMAP.md has NO architecture detail | No formulas, no data structures, no condition matrix, no stage definitions |
| 4 | project_files_documentation.md has NO logic specs | No decision trees, no formulas, no condition matrix, no cognitive stages |
| 5 | All 3 files reference `yuki_flow.md` as authority | Each header contains: "Authoritative Flow Reference: yuki_flow.md" |
| 6 | Discarded content is struck, not deleted | Every removed section visible with `~~strikethrough~~ [DISCARDED]` |
| 7 | Test count is consistent | 16/16 everywhere (not 15/15) |
| 8 | PolicySelector path is consistent | `src/brain/policy/` everywhere (not `src/brain/inference/`) |
| 9 | M3/M3.5 files appear in catalog | `src/brain/research/` and `src/brain/testing/` sections exist in proj_docs |
| 10 | 19 stages + phatic rule only in yuki_flow.md | Not present in roadmap or proj_docs |

---

# SECTION 5: QUICK REFERENCE — WHO OWNS WHAT

| If you need... | Go to... | Do NOT go to... |
|:---|:---|:---|
| How does the PolicySelector decide DEFER vs LEARN? | `yuki_flow.md` §5 | Roadmap or proj_docs |
| What is the formula for PrecisionPredictor? | `yuki_flow.md` §3.2 | Roadmap or proj_docs |
| What files do I need to implement for M3? | `yuki_flow.md` §13 | Roadmap (only has counts) |
| What is the ResearchPlanner SubGoal struct? | `yuki_flow.md` §8.5 | Roadmap or proj_docs |
| What milestone are we on? | `YUKI_ROADMAP.md` §1 | yuki_flow.md |
| How many tests must pass? | `YUKI_ROADMAP.md` §1 | yuki_flow.md |
| What is the M5 forward-compat plan? | `YUKI_ROADMAP.md` §4 | yuki_flow.md |
| What is the status of `FileOperator.cpp`? | `project_files_documentation.md` — src/brain table | Roadmap |
| What includes `predictive_turn_engine.h`? | `project_files_documentation.md` — wiring column | yuki_flow.md |
| What is Yuki's philosophy as digital organism? | `DESIGN_PHILOSOPHY.md` | proj_docs |
| What was fixed in the 2026-07-20 session? | `CHANGELOG.md` | Roadmap |
| What issues are currently open? | `KNOWN_ISSUES.md` | Roadmap |

---

*End of Restructuring Plan. Execute atomically. Report back with audit table.*
