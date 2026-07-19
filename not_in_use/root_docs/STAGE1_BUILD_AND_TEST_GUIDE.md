# Yuki_1.0 — Stage 1 Complete Build & Test Guide

---

## 5. Build Instructions

### Prerequisites
Open an **x64 Native Tools Command Prompt for VS 2019/2022** (not regular cmd or PowerShell).

Verify tools before building:
```bat
cl
cmake --version
git --version
```

### Build Steps (run from `d:\Yuki_1.0\`)
```bat
mkdir build
cd build
cmake ..
cmake --build .
```

The executable is placed at:
```
d:\Yuki_1.0\build\Debug\yuki.exe
```

### Run
```bat
build\Debug\yuki.exe
```

---

## 6. What Was Completed

| # | Deliverable | Status |
|---|---|---|
| 1 | `CMakeLists.txt` — CMake 3.20, C++17, MSVC flags | ✅ Done |
| 2 | `src/FeatureFlags.h` + `.cpp` — toggle struct + loader | ✅ Done |
| 3 | `src/CheckpointTracer.h` + `.cpp` — event recorder + full/smart render | ✅ Done |
| 4 | `src/BabyMode.h` + `.cpp` — sense + reflex pipeline | ✅ Done |
| 5 | `src/main.cpp` — terminal loop + banner + trace output | ✅ Done |
| 6 | `docs/ADDED_COMPONENTS.md` | ✅ Done |
| 7 | `docs/RESTRUCTURE_PLAN.md` | ✅ Done |
| 8 | `docs/ENVIRONMENT_REQUIREMENTS.md` | ✅ Done |
| 9 | `scripts/clean_project.bat` | ✅ Done |
| 10 | `scripts/collect_setup_info.bat` | ✅ Done |

---

## 7. Errors and Risks Noticed

| Risk | Mitigation |
|---|---|
| Box-drawing characters (`┌ │ └`) may display as `?` in older Windows consoles | Open the terminal, go to Properties → Font → select a Unicode font (e.g. Consolas or Cascadia Code). Or set `chcp 65001` before running. |
| `cl` not found in PATH | Must use **x64 Native Tools Command Prompt**, not plain cmd/PowerShell |
| CMake generator mismatch | CMake auto-detects VS; if you have multiple VS versions installed, explicitly pass `-G "Visual Studio 17 2022" -A x64` to `cmake ..` |
| `REFLEX_QUESTION_RESPONSE` fires before `REFLEX_COMMAND_RESPONSE` | By design — question takes priority over command. Input like "how do I open chrome?" is treated as a question, not a command. |

---

## 8. Step-by-Step Testing Instructions

After building, run `build\Debug\yuki.exe` and enter each input below exactly.

---

### Test 1 — Name Detection
```
You: yuki
```

**Expected reaction:**
```
Yuki: I heard my name.
```

**Expected checkpoint trace:**
```
┌─────────────────────────────────────────┐
│          CHECKPOINT TRACE (FULL)         │
├─────────────────────────────────────────┤
│ [ 1]  TURN_START                        │
│ [ 2]  SIGNAL_IN                  → yuki │
│ [ 3]  NORMALIZED                 → yuki │
│ [ 4]  NAME_DETECTED              → yuki │
│ [ 5]  SENSORY_BUFFER_READY              │
│ [ 6]  REFLEX_ENTER                      │
│ [ 7]  REFLEX_NAME_RESPONSE              │
│ [ 8]  TURN_END                          │
└─────────────────────────────────────────┘
```

---

### Test 2 — Question Detection
```
You: what is this?
```

**Expected reaction:**
```
Yuki: This feels like a question. I am not understanding it yet, only sensing it.
```

**Expected checkpoints include:**
```
PATTERN_QUESTION
REFLEX_QUESTION_RESPONSE
```

---

### Test 3 — Command Detection
```
You: open chrome
```

**Expected reaction:**
```
Yuki: This feels like an action request. I am not executing it yet.
```

**Expected checkpoints include:**
```
PATTERN_COMMAND  → open
REFLEX_COMMAND_RESPONSE
```

---

### Test 4 — Empty Input (press Enter with no text)
```
You: [press Enter]
```

**Expected reaction:**
```
Yuki: I sensed no input.
```

**Expected checkpoints include:**
```
NORMALIZED       → (empty string)
SENSORY_BUFFER_READY  is_empty=true
REFLEX_ENTER
REFLEX_EMPTY
TURN_END
```

---

### Test 5 — Generic Fallback
```
You: hello there
```

**Expected reaction:**
```
Yuki: I received input and produced a basic reflex response.
```

**Expected checkpoints include:**
```
SENSORY_BUFFER_READY
REFLEX_ENTER
REFLEX_GENERIC_RESPONSE
TURN_END
```

---

### Test 6 — Exit
```
You: quit
```

**Expected output:**
```
[Yuki] Shutting down. Goodbye.
```
Process exits with code 0.

---

## 9. Expected Terminal Output (Full Session Example)

```
╔══════════════════════════════════════════════╗
║     Yuki_1.0 — Blank Slate Baby Mode         ║
║     Stage 1  |  Sensory-Reflex Shell         ║
╚══════════════════════════════════════════════╝
  Type anything and press Enter.
  Type  quit  to exit.

You: yuki
Yuki: I heard my name.

┌─────────────────────────────────────────┐
│          CHECKPOINT TRACE (FULL)         │
├─────────────────────────────────────────┤
│ [ 1]  TURN_START                        │
│ [ 2]  SIGNAL_IN                  → yuki │
│ [ 3]  NORMALIZED                 → yuki │
│ [ 4]  NAME_DETECTED              → yuki │
│ [ 5]  SENSORY_BUFFER_READY              │
│ [ 6]  REFLEX_ENTER                      │
│ [ 7]  REFLEX_NAME_RESPONSE              │
│ [ 8]  TURN_END                          │
└─────────────────────────────────────────┘

You: quit

[Yuki] Shutting down. Goodbye.
```

---

## 10. Failure Diagnosis Checklist

### Build Fails

| Symptom | Cause | Fix |
|---|---|---|
| `'cl' is not recognized` | Wrong terminal | Use x64 Native Tools Command Prompt |
| `CMake Error: No CMAKE_CXX_COMPILER` | MSVC not found by CMake | Run `cmake .. -G "Visual Studio 17 2022" -A x64` |
| `C2001: newline in constant` | File saved with wrong encoding | Ensure all `.cpp`/`.h` files are UTF-8 |
| `LNK2019: unresolved external` | A `.cpp` not listed in CMakeLists | Check all 4 `.cpp` files appear under `add_executable` |
| CMake version error | CMake too old | Update CMake to 3.20+ |

### Runtime Issues

| Symptom | Cause | Fix |
|---|---|---|
| Trace not printing | `show_terminal_trace` flag is false | Verify `FeatureFlags.cpp` sets it to `true` |
| Box characters show as `?` | Console not Unicode | Run `chcp 65001` before launching, or switch to Windows Terminal |
| `PATTERN_QUESTION` missing for `what is this?` | Pattern detection bug | Confirm `questionByStem` loop in `BabyMode.cpp` checks for `"what"` at position 0 |
| `PATTERN_COMMAND` fires for a question containing "open" | Priority issue | The reflex priority is: empty → name → question → command. If `looks_like_question` is also true, question wins. |
| Empty input not detected | `trimWhitespace` bug | Check `trimWhitespace()` returns `""` for all-whitespace strings |
| `TURN_END` missing | Early return path skipped it | `TURN_END` is emitted in `process()` after `reflex()` returns — all code paths go through it |

### Trace is Missing Checkpoints

1. Open `BabyMode.cpp` and verify every `tracer_.hit(...)` call is present in order.
2. Confirm `tracer_.clear()` is called at the **start** of `process()` not at the end.
3. Confirm `main.cpp` calls `baby.tracer().renderFull()` **after** `baby.process(line)` returns.

---

## 11. Current Project State Summary

### What exists right now

```
Yuki_1.0/
├── CMakeLists.txt          ← CMake 3.20+, C++17, MSVC x64, builds 'yuki'
├── src/
│   ├── main.cpp            ← Banner, flag init, terminal loop, trace output
│   ├── FeatureFlags.h/.cpp ← Global feature toggles (all ON by default)
│   ├── CheckpointTracer.h/.cpp ← Event recorder, renderFull(), renderSmart()
│   ├── BabyMode.h/.cpp     ← sense() + reflex() pipeline, all checkpoints
├── docs/
│   ├── ADDED_COMPONENTS.md ← Component registry, pipeline description
│   ├── RESTRUCTURE_PLAN.md ← Minimal-file rules, future module list
│   └── ENVIRONMENT_REQUIREMENTS.md ← Build env spec + auto-update anchor
└── scripts/
    ├── clean_project.bat       ← Removes build/, .vs/, CMakeCache etc.
    └── collect_setup_info.bat  ← Queries cl/cmake/git, injects into docs
```

### What is active
- **BabyMode pipeline only** — sense + reflex with full checkpoint tracing
- **All feature flags ON** — trace and checkpoint hit display both enabled

### What is NOT present (by design)
- No memory, planner, intent engine, NLP, web, tools, Python, multithreading
- No legacy Yuki code of any kind
- No external library dependencies

### Readiness for Stage 2
The architecture is specifically shaped to grow:
- Add `InputEvidence.h/.cpp` → plug into `BabyMode::sense()` output
- Add `Intent.h/.cpp` → replace or extend `BabyMode::reflex()` logic
- Add `Logger.h/.cpp` → called from `main.cpp` alongside `CheckpointTracer`
- All future modules follow the one-concept = one `.h` + one `.cpp` rule
