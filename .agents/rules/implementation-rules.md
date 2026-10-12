---
trigger: always_on
description: Mandatory implementation rules for all tasks - zero stubs/heuristics, prefer merging over new files, pre-analysis, temp file cleanup, full integration wiring, and top-in-class research. Unconditional zero-bypass contract.
---

# Implementation & Execution Standards (Always On — Zero Bypass)

> **⚠️ ABSOLUTE AUTHORITY & ZERO-BYPASS ENFORCEMENT CONTRACT:**  
> This rule is **MANDATORY for EVERY single task and tool execution in Antigravity**.  
> **ZERO EXCEPTIONS, ZERO BYPASS, ZERO SKIPPING** under any condition — including rapid prototyping, emergency fixes, subagent tasks, refactors, or debugging sessions.  
> Any attempt to take shortcuts, skip pre-analysis, introduce stubs/heuristics, leave dangling wires, create redundant files, or abandon scratch files is a direct contract violation.

---

## 1. Zero Unnecessary Files — Merge Before Creating New Files
- **Never create new files unless strictly mandatory**:
  - Do NOT create a new file if the functionality logically belongs to, extends, or complements existing modules, classes, or headers.
  - Always inspect existing files first and prefer merging, refactoring, or co-locating new logic into existing files.
  - New files are permitted ONLY when strictly mandated by architectural separation (e.g., brand-new standalone subsystem specified by architecture, distinct translation unit required for build targets/libraries).
- **Consolidate & prevent file sprawl**: Keep code cohesion high and file counts minimal.

---

## 2. Mandatory Pre-Implementation Code & Architecture Analysis
- **Always analyze before implementing**:
  - Never start writing code blindly or jumping straight to edits.
  - Read and inspect existing source files, header definitions, callers, data flow, memory management paradigms, class hierarchies, and dependencies.
  - Verify type signatures, namespace structures, lifetime semantics, and project conventions before modifying code.
- **Trace execution paths**: Map upstream callers and downstream callees prior to drafting changes to avoid breaking dependencies.

---

## 3. Temporary & Scratch File Lifecycle Management
- **Immediate cleanup of temporary artifacts**:
  - If temporary scripts, scratch files, ad-hoc test runners, or intermediate artifacts are created during debugging, analysis, or verification:
    - **Remove them immediately** once testing and verification are complete and the task is done.
    - If an artifact must be preserved for future reference but is not part of the active production build, move it to a dedicated `not_in_use` / archive directory.
  - Never leave stray scripts, dump files, or temporary test files in root or source directories.

---

## 4. Complete End-to-End Implementation & Full Integration Wiring
- **Full wiring & call-site integration**:
  - When implementing a feature or modification, ensure it is fully wired into the entire system:
    - Register new classes/methods in the main engine loop, orchestrator, dispatcher, or factory where appropriate.
    - Update all caller sites, constructors, dependency injections, and headers.
    - Ensure build files (`CMakeLists.txt`, build scripts) include all necessary sources and link targets.
  - **Zero dangling code**: Never leave orphan functions, unused interfaces, or incomplete wiring.
- **Complete implementation without omission**:
  - Implement all code paths, edge cases, state transitions, and error handling fully.
  - Do not leave partial logic, placeholder returns, or unhandled branches.

---

## 5. Strict Prohibition on Heuristics, Stubs, and Docstring-Only Placeholders
- **Real logic only**:
  - NEVER create heuristic shortcuts, stubs, mock implementations, or fake return values (e.g., `return true;`, `// TODO: implement later`).
  - NEVER create docstring-only, comment-only, or dummy signature files.
  - Every function, method, class, and algorithm MUST contain real, robust, production-grade, mathematically and logically complete implementation.

---

## 6. Research Top-in-Class Implementations & Clarify Ambiguity
- **Research first on confusion / complexity**:
  - If a problem, algorithm, or domain is complex, confusing, or unfamiliar, research industry-standard, top-in-class, and state-of-the-art solutions and algorithms.
  - Look for proven, battle-tested patterns and algorithmic designs to model.
- **Ask clarifying questions when unclear**:
  - If after thorough research the architectural requirement, design intent, or requirement is still ambiguous or underspecified, **stop and ask the user clarifying questions**.
  - Never guess, assume, or write placeholder code when requirements are unclear.

---

## 7. Mandatory Post-Task Verification Gate (Must Pass 100% Before Completion)
Before concluding any task, verify:
```
[ ] Code analyzed before implementation
[ ] No unnecessary files created (merged into existing files where possible)
[ ] Real, production-grade logic implemented (zero stubs, zero heuristics, zero dummy comments)
[ ] All integration points, callers, and build files wired end-to-end (zero dangling code)
[ ] All temporary files / scratch test files deleted or moved to not_in_use
[ ] Clean build with 0 errors and all tests passing
[ ] Documentation and changelog updated per update-status.md
```
