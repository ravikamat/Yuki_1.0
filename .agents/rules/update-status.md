---
trigger: always_on
---

# YUKI Documentation Update Skill

> **Skill:** `yuki_doc_update` | **Authority:** `yuki_flow.md` > `YUKI_ROADMAP.md` > `project_files_documentation.md` > `CHANGELOG.md`

---

## ⚠️ MANDATORY: Run this AFTER every task. No exceptions.

---

## STEP 0: GATHER ARTIFACTS

```
□ NEW files (paths)
□ MODIFIED files (paths)
□ DELETED files (paths)
□ NEW tests (paths)
□ MODIFIED tests (paths)
□ Test results: ___/___ passing
□ Build: 0 errors / 0 warnings? (Y/N)
□ Architecture decisions made?
□ Contradictions with yuki_flow.md?
```

---

## STEP 1: UPDATE `yuki_flow.md` (Authority — Logic Only)

**Update when:** new data structures, formulas, decision branches, pipeline changes, contradictions found.
**Never update:** milestone status, test counts, file catalog (beyond 22-core index), session history.

```
1. Open yuki_flow.md → navigate to relevant Phase (§1–§18)
2. New struct/class → insert in correct Phase, match code signature exactly
3. New formula → update LaTeX block, match variable names
4. New decision branch → update mermaid/matrix, add COND-XX to §14
5. Contradiction found → wrap old text in ~~strikethrough~~ + [DISCARDED — see <file>]
   → insert corrected text immediately after
   → NEVER delete struck text
6. New core component → add to §15 File Tracing Index (22 items max)
7. Save. Verify no accidental changes elsewhere.
```

---

## STEP 2: UPDATE `YUKI_ROADMAP.md` (Milestones Only)

**Update when:** milestone status change, test count change, new file counts, open questions resolved/discovered.
**Never update:** data structures, formulas, file-by-file details, session history.

```
1. Open §1 Milestone Status Table
2. Update Status: 🔴 PLANNED → 🟡 IN PROGRESS → 🟡 ARCH FINALIZED → ✅ COMPLETE
3. Update Tests: "X/Y passing" (X=passing, Y=planned; X=Y when ✅)
4. New files → add to milestone's "New Files" list (sorted)
5. Modified files → add to "Modified Files" list (sorted)
6. New tests → add to "New Tests" list
7. Resolved question → move to "Resolved Questions" + date + answer
8. New question → add to §6 Open Questions
9. Update "Current Test Coverage" line
10. Save. Verify milestone table is ONLY status source.
```

**Status Rules:**
| From | To | Condition |
|:---|:---|:---|
| 🔴 | 🟡 IN PROGRESS | First code written |
| 🟡 | 🟡 ARCH FINALIZED | Design review done |
| 🟡 ARCH FINALIZED | ✅ COMPLETE | All files done, all tests pass, 0 warnings |

---

## STEP 3: UPDATE `project_files_documentation.md` (File Catalog Only)

**Update when:** any file created/modified/deleted/moved, per-file status changes.
**Never update:** milestone status, test counts, formulas, cognitive stages.

```
1. Navigate to correct section for file's directory
2. NEW file → add row to table:
   | File | Path | Wiring | Data Flow | Data Type | Logic | Status | Ideal | Path | Bottlenecks | Snippet |
   Fill all columns. Sort alphabetically.
3. MODIFIED file → update changed columns (Logic, Status, Bottlenecks, Snippet)
   Stub→Working: change status, move resolved bottlenecks to CHANGELOG
4. DELETED file → strike ENTIRE row: ~~row~~ [DELETED — <date> — <reason>]
5. MOVED file → strike old row [MOVED to <path> — <date>], add new row
6. Resolved heuristic/stub → strike in §27, move to CHANGELOG
7. Save. Verify tables sorted alphabetically.
```

---

## STEP 4: UPDATE `CHANGELOG.md` (History — Always)

**Always run this. No exceptions.**

```markdown
### YYYY-MM-DD — <Title>

**Milestone:** M<X> | **Task:** <one line> | **Status:** ✅/🟡/🔴

#### Changes

- `<file>`: <what changed>

#### Tests

- Added: <files> | Modified: <files> | Results: <X/Y>

#### Decisions

- <decision + reason>

#### Resolved

- <issue> → <resolution>

#### Discovered

- <issue> → P<X> → <next action>

#### Docs Updated

- yuki_flow.md: <sections>
- YUKI_ROADMAP.md: <sections>
- project_files_documentation.md: <sections>
```

---

## SPECIAL CASES

| Case                                | Action                                                                                    |
| :---------------------------------- | :---------------------------------------------------------------------------------------- |
| **Contradiction with yuki_flow.md** | Strike old text + [DISCARDED — see CHANGELOG <date>], insert correction, log in CHANGELOG |
| **New milestone M9+**               | Add row to roadmap §1, follow M3 format, update compat matrix                             |
| **File renamed**                    | Strike old row [RENAMED to <name>], add new row, update all references                    |
| **Test failing**                    | Status stays 🟡, log in CHANGELOG, never mark ✅                                          |
| **Trivial fix**                     | Use quick template: date, milestone, status, one-line change, no new tests, docs updated  |

---

## VERIFICATION (Before Submit)

```
□ yuki_flow.md updated (if architecture changed)
□ YUKI_ROADMAP.md updated (if status/tests changed)
□ project_files_documentation.md updated (if any file touched)
□ CHANGELOG.md updated (ALWAYS)
□ Zero duplication between 3 main files
□ Struck text preserved (not deleted)
□ Test counts consistent everywhere
□ File paths consistent everywhere
□ Tables sorted alphabetically
□ Build: 0 errors, 0 warnings
```

---

## QUICK REFERENCE: WHO OWNS WHAT

| Need...                                 | Go to...                       | Never go to...        |
| :-------------------------------------- | :----------------------------- | :-------------------- |
| How does PolicySelector decide?         | yuki_flow.md §5                | Roadmap / proj_docs   |
| PrecisionPredictor formula?             | yuki_flow.md §3.2              | Roadmap / proj_docs   |
| M3 files to implement?                  | yuki_flow.md §13               | Roadmap (only counts) |
| SubGoal struct definition?              | yuki_flow.md §8.5              | Roadmap / proj_docs   |
| What milestone are we on?               | YUKI_ROADMAP.md §1             | yuki_flow.md          |
| How many tests must pass?               | YUKI_ROADMAP.md §1             | yuki_flow.md          |
| M5 forward-compat plan?                 | YUKI_ROADMAP.md §4             | yuki_flow.md          |
| Status of FileOperator.cpp?             | project_files_documentation.md | Roadmap               |
| What includes predictive_turn_engine.h? | project_files_documentation.md | yuki_flow.md          |
| What was fixed 2026-07-20?              | CHANGELOG.md                   | Roadmap               |
