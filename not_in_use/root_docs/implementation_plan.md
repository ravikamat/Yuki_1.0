# YUKI — COMPLETE BUILD PLAN
## Master Reference Document
### Everything We Planned — Stage by Stage

---

> This document is the single source of truth.
> Every stage, every component, every purpose.
> Written so this plan is never lost or forgotten.

---

## CURRENT STATE — What Yuki Already Has

Before building anything new, here is what exists and works:

### Brain Layer (d:/Yuki_1.0/src/brain/)
| Component | File | What it does | Status |
|---|---|---|---|
| PatternEngine | PatternEngine.cpp | Classifies input by keyword patterns | ✅ Working |
| SynthesisEngine | SynthesisEngine.cpp | Generates responses from evidence | ✅ Working |
| EvidenceGraphBuilder | EvidenceGraphBuilder.cpp | Builds evidence from agent results | ✅ Working |
| AgentRuntime | AgentRuntime.cpp | Runs research agents in parallel | ✅ Working |
| KnowledgeDaemon | KnowledgeDaemon.cpp | Background online learning | ✅ Working |
| ConceptVault | ConceptVault.cpp | Stores learned concepts permanently | ✅ Working |
| TaskDecomposer | TaskDecomposer.cpp | Breaks tasks into subtasks | ✅ Working |
| SkillRegistry | SkillRegistry.cpp | Stores and retrieves learned skills | ✅ Working |
| AutonomousSkillBuilder | AutonomousSkillBuilder.cpp | Builds new script skills automatically | ✅ Working |
| TraceStore | TraceStore.cpp | Records every turn's full trace | ✅ Working |
| SelfAuditEngine | SelfAuditEngine.cpp | Finds failures, queues learning | ✅ Working |
| UnknownTopicFlow | UnknownTopicFlow.cpp | 5-stage rescue for unknown input | ✅ Working |
| MobileServer | MobileServer.cpp | HTTP server for phone access | ✅ Working |
| MotherCore | MotherCore.cpp | Central 18-step cognitive pipeline | ✅ Working |
| WebReconAgent | WebReconAgent.cpp | Searches web for information | ✅ Working |
| ClarificationEngine | ClarificationEngine.cpp | Asks clarifying questions | ✅ Working (basic) |
| EmpathyLayer | (in MotherCore) | Handles emotional statements | ✅ Working |
| UserMemory | (in MotherCore) | Remembers user info | ✅ Working |

### System Layer (d:/Yuki_1.0/src/)
| Component | File | What it does | Status |
|---|---|---|---|
| BabyMode | BabyMode.cpp | Central hub — wires everything | ✅ Working |
| CommandRouter | CommandRouter.cpp | Handles system commands | ✅ Working |
| SubsystemControl | SubsystemControl.cpp | Manages all sensors on/off | ✅ Working |
| AutoSensor | AutoSensor.cpp | Auto-starts all sensors on launch | ✅ Working |
| PresenceShell | PresenceShell.cpp | Floating dark chat window (Win32+GDI+) | ✅ Working |
| AvatarBody | AvatarBody.cpp | Avatar display window | ✅ Working |
| CameraRuntime | CameraRuntime.cpp | Live camera capture (DirectShow) | ✅ Working |
| ScreenRuntime | ScreenRuntime.cpp | Screen capture (BitBlt) | ✅ Working |
| SpeechToTextRuntime | SpeechToTextRuntime.cpp | Voice → text (Whisper) | ✅ Working |
| Mouth (TTS) | Mouth.cpp | Text → voice output | ✅ Working |
| NeuralSpine | NeuralSpine.cpp | Fallback reasoning engine | ✅ Working |
| MobileServer | brain/MobileServer.cpp | Phone WiFi access on port 8765 | ✅ Working |

---

## THE 7-PHASE BUILD PLAN

---

# PHASE 1 — TRUE LANGUAGE UNDERSTANDING
## "She understands MEANING, not just words"

### What this phase unlocks:
Instead of matching keywords, Yuki understands what any sentence means — in any language, any phrasing, said any way.

---

### Component 1.1 — SemanticParser

**Purpose:** Break any sentence into structured meaning — who, what, why, how, when, where.

**What it does:**
```
Input:  "operate my phone and message my friend Rahul on WhatsApp"

Output:
  actions:  [operate_device, send_message]
  device:   phone (type=mobile, ownership=user)
  action2:  message
  person:   Rahul (relationship=friend)
  platform: WhatsApp
  content:  unknown → needs clarification
```

**How it works:**
- Tokenises every word
- Tags each word: verb, noun, pronoun, adjective, etc.
- Extracts semantic slots: who, what, which, where, when, why
- Works for ANY language after translation layer

**Files to create:**
- `src/brain/SemanticParser.h`
- `src/brain/SemanticParser.cpp`

**Connects to:** GoalModel (feeds parsed slots into it)

**Success criteria:** 
- "build me a health app" → correctly extracts: action=build, type=app, purpose=health
- "aaj koi recipe batao" → after translation, same quality extraction
- "I'm not feeling well" → correctly identified as emotional, NOT a task

---

### Component 1.2 — GoalModel

**Purpose:** A structured container that holds everything Yuki knows and doesn't know about what you want.

**What it looks like:**
```cpp
struct GoalModel {
    string goal;           // what the user wants
    string domain;         // area: tech, food, health, creative etc.
    map<string,string> known_slots;    // confirmed info
    vector<string> unknown_slots;      // still need to find out
    vector<string> gaps;              // things she doesn't know how to do
    string language;                   // input language
    string tone;                       // formal/casual/urgent
    bool needs_clarification;
    bool needs_research;
    bool needs_execution;
};
```

**Files to create:**
- `src/brain/GoalModel.h`
- `src/brain/GoalModel.cpp`

**Connects to:** SemanticParser fills it, ClarificationEngine reads unknown_slots, AutonomousPlanner reads known_slots

---

### Component 1.3 — Language Detection & Translation Layer

**Purpose:** Detect what language was used, extract meaning, and ensure Yuki responds in the SAME language.

**What it does:**
```
"aaj koi acha khane ka recipie batao"
  → Detect: Hindi (Romanised / Hinglish)
  → Translate meaning: "Tell me a good food recipe for today"
  → Process as English internally
  → Generate response
  → Translate response back to Hindi
  → Reply in Hindi / Hinglish matching user's style
```

**Languages supported:**
- Hindi / Hinglish (primary — your language)
- English (already works)
- Any language Whisper can transcribe (30+ languages)

**Also fix:** Whisper language config: `"en"` → `"auto"` (1-line fix, can do NOW)

**Files to create:**
- `src/brain/LanguageLayer.h`
- `src/brain/LanguageLayer.cpp`

**Success criteria:**
- Speak Hindi → Yuki responds in Hindi
- Mix Hindi/English → she matches your mix
- ANY language → she detects and matches

---

# PHASE 2 — SELF-LEARNING FROM SCRATCH
## "She knows what she doesn't know — and learns it"

### What this phase unlocks:
When Yuki encounters something she doesn't know how to do, she doesn't give up. She goes and learns it from real documentation, extracts the steps, and comes back ready.

---

### Component 2.1 — CapabilityMap

**Purpose:** A live database of what Yuki CAN and CANNOT do. Updated every time she learns something new.

**What it looks like:**
```
CAPABILITY: "send_whatsapp_message"
  status: UNKNOWN → LEARNING → KNOWN
  method: ADB + Android automation
  steps:  [connect device, open app, find contact, type, send]
  tools_required: [ADB]
  tools_installed: false → true (after install)
  last_used: timestamp
  success_rate: 94%

CAPABILITY: "colour_grade_premiere"
  status: KNOWN
  method: ExtendScript JSX
  steps:  [open PP, import, apply LUT, export]
  tools_required: [Premiere Pro, .cube LUT file]
  tools_installed: true
```

**Files to create:**
- `src/brain/CapabilityMap.h`
- `src/brain/CapabilityMap.cpp`
- `data/capability_map.json` (persistent storage)

**Connects to:** GoalModel reads gaps, SelfAuditEngine updates success rates

---

### Component 2.2 — DocReader

**Purpose:** Read full documentation pages — not just search snippets. Extract actual usable knowledge.

**What it does:**
```
Gap detected: "How to automate Premiere Pro"

DocReader:
  → Goes to: adobe.com/developer/ExtendScript-docs
  → Reads full documentation (not just first paragraph)
  → Extracts:
      - API names and what they do
      - Code examples
      - Error handling patterns
      - Known limitations
  → Stores structured knowledge in CapabilityMap
```

**Different from WebReconAgent:**
- WebReconAgent = quick snippets for answering questions
- DocReader = deep reading for learning how to DO things

**Files to create:**
- `src/brain/DocReader.h`
- `src/brain/DocReader.cpp`

---

### Component 2.3 — KnowledgeExtractor

**Purpose:** Turn raw documentation text into actionable steps Yuki can actually execute.

**What it does:**
```
Raw doc text:
  "To apply a LUT in Premiere Pro via script, use:
   app.project.activeSequence.videoTracks[0]
   .clips[0].colorSettings.lumaInput = 0.5"

KnowledgeExtractor output:
  Step: apply_lut
  Code: [exact ExtendScript code]
  Pre-condition: sequence must be active
  Post-condition: LUT applied to clip
  Error handling: if clips[0] undefined → no clip imported yet
```

**Files to create:**
- `src/brain/KnowledgeExtractor.h`
- `src/brain/KnowledgeExtractor.cpp`

---

# PHASE 3 — SMART PLANNING & CLARIFICATION
## "She builds her own plan and asks only what she must"

### What this phase unlocks:
Before doing anything, Yuki builds a complete step-by-step plan. She asks only the questions she truly cannot answer herself. She shows you the plan before touching anything.

---

### Component 3.1 — Enhanced ClarificationEngine

**Purpose:** Ask the minimum number of questions needed to fill GoalModel's unknown slots.

**Rules it follows:**
```
Rule 1: If she can figure it out herself → NEVER ask
Rule 2: Ask ONE question at a time → wait for answer
Rule 3: Questions are ordered by priority (most blocking first)
Rule 4: Once answered → stored in UserMemory → never asked again
Rule 5: If truly ambiguous → pick most likely → state assumption
```

**Example:**
```
Goal: send message to friend
Unknown slots: [friend_name, platform, message_content]

Question order:
  1. "Which friend?" (blocks everything else)
  2. "WhatsApp or SMS?" (blocks platform choice)
  3. "What should I say?" (blocks content)

NOT asked (she figures out herself):
  - "Do you want to send a message?" (obviously yes)
  - "Should I use your phone?" (obviously yes)
```

**Upgrade from current:** Current ClarificationEngine is generic.
New version is GoalModel-driven — questions come directly from unknown_slots.

**Files to modify:**
- `src/brain/ClarificationEngine.h` (extend)
- `src/brain/ClarificationEngine.cpp` (full upgrade)

---

### Component 3.2 — AutonomousPlanner

**Purpose:** Build a complete execution tree for any goal. Know what tools are needed. Know what order to do things. Know what could go wrong.

**What it produces:**
```
PLAN for: "send WhatsApp message to Rahul saying I'll be late"

Step 1: Check ADB installed
  → Tool: ADB
  → Check: run "adb version"
  → If fail: install ADB (needs permission)

Step 2: Check phone connected
  → Run: "adb devices"
  → If empty: ask user to connect phone + enable USB debug

Step 3: Open WhatsApp
  → Run: adb shell am start -n com.whatsapp/.Main

Step 4: Find Rahul's chat
  → Use UIAutomation to search for "Rahul"
  → Tap on his chat

Step 5: Type message
  → adb shell input text "I'll be late"

Step 6: Send
  → adb shell input keyevent 66 (Enter)

Step 7: Verify
  → Screenshot → OCR → confirm message appears as "Sent"

Step 8: Report
  → "Message sent to Rahul on WhatsApp."

PERMISSION REQUIRED AT: Step 1 (if ADB not installed)
CONFIRMATION REQUIRED AT: Step 6 (before sending)
```

**Files to create:**
- `src/brain/AutonomousPlanner.h`
- `src/brain/AutonomousPlanner.cpp`

---

# PHASE 4 — EXECUTION ENGINE
## "She actually DOES things — not just talks about them"

### What this phase unlocks:
Yuki gets hands. She can control your PC, open software, type, click, read the screen, manage files, install tools — anything a human can do with a keyboard and mouse.

---

### Component 4.1 — SystemExecutor

**Purpose:** The hands of Yuki. Executes every type of action on Windows.

**Sub-systems inside SystemExecutor:**

```
4.1.1 AppLauncher
  → ShellExecute() — open any application
  → FindWindow() — find already-open app
  → PostMessage(WM_CLOSE) — close an app

4.1.2 KeyboardController
  → SendInput() — type any text
  → Keyboard shortcuts: Ctrl+S, Alt+F4, Win+D etc.
  → Special keys: Enter, Tab, Escape, arrow keys

4.1.3 MouseController
  → SetCursorPos() — move mouse
  → mouse_event() — click, double-click, right-click
  → Scroll up/down

4.1.4 UIAutomationController
  → Find any button/textbox by name
  → Click it, type into it, read its value
  → Works on ANY Win32/WPF/UWP application

4.1.5 ScreenReader (OCR)
  → ScreenRuntime captures screen (already exists)
  → Windows OCR API reads text from capture
  → "What does that button say?" → reads it

4.1.6 SystemSettingsController
  → Volume: Core Audio API
  → Dark mode: Registry
  → WiFi: WlanAPI
  → Bluetooth: Bluetooth API
  → Display brightness: WMI

4.1.7 ProcessManager
  → EnumProcesses() — list all running apps
  → TerminateProcess() — kill a process
  → GetProcessMemoryInfo() — RAM per app
  → CPU per app via PerformanceCounters

4.1.8 FileOperator
  → Create, copy, move, delete, rename files
  → Full undo log for every operation
  → Never deletes — moves to Review folder first

4.1.9 ScriptRunner
  → Run .py Python scripts
  → Run .bat batch scripts
  → Run .jsx ExtendScript (for Adobe apps)
  → Run .ps1 PowerShell scripts
  → Capture output, detect errors
```

**Files to create:**
- `src/brain/SystemExecutor.h`
- `src/brain/SystemExecutor.cpp`
- `src/brain/UIAutomationController.h/.cpp`
- `src/brain/OCREngine.h/.cpp`
- `src/brain/FileOperator.h/.cpp`
- `src/brain/ScriptRunner.h/.cpp`

---

### Component 4.2 — VerificationEngine

**Purpose:** After every action, verify it actually worked. Don't just assume.

**What it does:**
```
After: "send WhatsApp message"
  → Screenshot the screen
  → OCR reads: "Sent" checkmark visible?
  → If yes: "Message confirmed sent."
  → If no: "Message may not have sent. Check manually."

After: "install Android Studio"
  → Run: android studio --version
  → If version returns: "Installed successfully. Version 2024.1"
  → If error: "Installation may have failed. Error: [exact error]"

After: "organise files"
  → Count files in new folders
  → Verify duplicates moved to Review
  → Report: "24,847 files organised. 1,203 duplicates in Review folder."
```

**Files to create:**
- `src/brain/VerificationEngine.h`
- `src/brain/VerificationEngine.cpp`

---

### Component 4.3 — DependencyInstaller

**Purpose:** When a tool is missing, find it, ask permission, install it silently.

**What it does:**
```
AutonomousPlanner says: "Need ADB"
DependencyInstaller:
  → Checks if ADB is installed
  → If not: "I need ADB (Android Debug Bridge) to connect your phone.
              It's from Google, free, safe.
              Should I install it? It takes about 30 seconds."
  → You: yes
  → winget install -e --id Google.AndroidPlatformTools
  → Waits → verifies → "ADB installed. Continuing."

Package managers it uses:
  → winget (Windows built-in)
  → pip (Python packages)
  → npm (Node.js packages)
  → Direct download for things not in package managers
```

**Files to create:**
- `src/brain/DependencyInstaller.h`
- `src/brain/DependencyInstaller.cpp`

---

# PHASE 5 — AUTOMATIC SELF-IMPROVEMENT
## "She finds her own weak spots and fixes them"

### What this phase unlocks:
Without being asked, Yuki monitors herself, finds where she's slow or wrong, and improves — always with your permission for code changes.

---

### Component 5.1 — PerformanceProfiler

**Purpose:** Find exactly which parts of Yuki are slow, heavy, or failing.

**What it monitors:**
```
Pipeline timing:
  → Step 1 (PatternEngine): avg 12ms ✅
  → Step 9 (EvidenceGraph): avg 847ms ⚠️ TOO SLOW
  → Step 14 (Synthesis): avg 34ms ✅

Memory usage:
  → ConceptVault: 124MB ✅
  → TraceStore: 2.1GB ⚠️ TOO LARGE (needs pruning)

Accuracy:
  → Questions answered correctly: 87%
  → Failed turns: 13% → top failure topics listed
  → Repeated failures on same topic: flagged for P0 learning
```

**Files to create:**
- `src/brain/PerformanceProfiler.h`
- `src/brain/PerformanceProfiler.cpp`

---

### Component 5.2 — SelfRewriter

**Purpose:** Read her own source code, understand it, and write improved versions of slow/broken functions.

**What it does:**
```
PerformanceProfiler finds: EvidenceGraphBuilder.cpp line 247 is slow

SelfRewriter:
  → Opens: d:/Yuki_1.0/src/brain/EvidenceGraphBuilder.cpp
  → Reads the slow function
  → Understands what it does
  → Identifies: "it rebuilds the full graph on every call
                  but only 3% of it changes each time"
  → Writes: optimised version with incremental update
  → Shows you: [before code] vs [after code]
  → "This should be ~60% faster. May I apply this?"
  → You: yes → writes file → queues rebuild
```

**Files to create:**
- `src/brain/SelfRewriter.h`
- `src/brain/SelfRewriter.cpp`

---

### Component 5.3 — RebuildManager

**Purpose:** When code changes are approved, safely rebuild Yuki and restart her.

**Process:**
```
1. Backup current yuki.exe
2. Run: cmake --build build --config Release
3. If build succeeds: swap exe → restart
4. If build fails: restore backup → report error → SelfRewriter tries again
5. After restart: run benchmark → compare speeds
6. Report: "Rebuild complete. EvidenceGraph is now 58% faster."
```

**Files to create:**
- `src/brain/RebuildManager.h`
- `src/brain/RebuildManager.cpp`

---

### Component 5.4 — Watchdog Process

**Purpose:** A SEPARATE executable that monitors yuki.exe and restarts her if she crashes.

**This is critical — a crashed process cannot fix itself.**

```
yuki_watchdog.exe (separate process, always running)
  │
  ├── Every 2 seconds: is yuki.exe alive?
  │
  ├── If ALIVE: continue monitoring
  │
  └── If DEAD (crashed):
        1. Read crash dump / error log
        2. ErrorClassifier: what caused this?
           → Missing file?    → restore it → restart
           → Missing DLL?     → install → restart
           → Out of memory?   → clear caches → restart
           → Code bug?        → log it → restart old version
                                → queue SelfRewriter to fix
        3. Restart yuki.exe
        4. Notify you: "I crashed due to [reason].
                        I've restarted. Fix is queued."
```

**Files to create:**
- `src/yuki_watchdog.cpp` (separate executable)
- Added to CMakeLists.txt as separate target

---

# PHASE 6 — SELF CODE OPTIMISATION ON DEMAND
## "You say 'optimise yourself' — she does it"

### What this phase unlocks:
When you explicitly ask Yuki to optimise herself, or when she notices she's slowing down, she reads her own code, finds the bottleneck, writes the fix, shows you, gets permission, rebuilds.

---

### Component 6.1 — CodeReader

**Purpose:** Read and UNDERSTAND her own source files — not just as text, but as code with logic.

**What it does:**
```
Reads: MotherCore.cpp
Builds internal map:
  → handleInput() → calls 18 steps
  → step5_classifyIntent() → calls PatternEngine
  → step9_buildEvidence() → calls EvidenceGraphBuilder ← SLOW
  → step14_synthesise() → calls SynthesisEngine
  
Knows:
  → Which function calls which
  → What each function's job is
  → Where data flows through
  → What the bottleneck is connected to
```

**Files to create:**
- `src/brain/CodeReader.h`
- `src/brain/CodeReader.cpp`

---

### Component 6.2 — BottleneckAnalyser

**Purpose:** Combine runtime profiling + code reading to pinpoint exactly what to fix.

**Output:**
```
ANALYSIS REPORT:
  Slowest component:    EvidenceGraphBuilder (avg 847ms)
  Location:             src/brain/EvidenceGraphBuilder.cpp:247
  Cause:                Full rebuild on every call
  Fix type:             Incremental update cache
  Estimated improvement: 55-65% faster
  Risk:                 Low — isolated function
  Recommendation:       Rewrite buildEvidence() with dirty flag pattern
```

**Files to create:**
- `src/brain/BottleneckAnalyser.h`
- `src/brain/BottleneckAnalyser.cpp`

---

### Component 6.3 — CodeOptimiser

**Purpose:** Write the actual improved code. Show the diff. Apply with permission.

**Interaction with you:**
```
Yuki: "I found a bottleneck. Here is what I want to change:

BEFORE (line 247, EvidenceGraphBuilder.cpp):
  void buildEvidence() {
    graph_.clear();        // clears everything
    for (auto& a : agents_) graph_.add(a.result());  // rebuilds all
  }

AFTER:
  void buildEvidence() {
    for (auto& a : agents_) {
      if (a.isDirty()) graph_.update(a.result());  // only updates changed
    }
  }

Expected improvement: ~60% faster evidence building.
This is safe — no logic change, just efficiency.

May I apply this change and rebuild myself?"

You: yes → applied → rebuilt → tested → reported
```

---

# PHASE 7 — SELF FEATURE ADDITION
## "You say 'add this feature' — she builds it into herself"

### What this phase unlocks:
You can ask Yuki to add entirely new capabilities to herself. Script skills are instant. Core C++ features need a recompile — always with your permission.

---

### Component 7.1 — FeatureWriter

**Purpose:** Write entirely new C++ modules based on what you describe.

**Example:**
```
You: "Add a feature where you detect if I'm frustrated
      from how I'm typing — fast typing, short words"

FeatureWriter:
  → Understands: need FrustrationDetector module
  → Determines: where it connects (InputPerception layer)
  → Writes: FrustrationDetector.h + FrustrationDetector.cpp
      Signals:
        → Words per minute > threshold → stress flag
        → Short words (< 4 chars avg) → direct/urgent flag
        → Caps words → emphasis flag
        → "???" or "!" → frustration flag
  → Updates: BabyMode.cpp to wire it in
  → Shows you the new files
  → "May I add this feature and rebuild?"
```

**Files to create:**
- `src/brain/FeatureWriter.h`
- `src/brain/FeatureWriter.cpp`

---

### Component 7.2 — IntegrationPlanner

**Purpose:** Figure out exactly WHERE in Yuki's architecture a new feature should connect.

**Yuki's pipeline insertion points:**
```
Pre-pipeline:   Before MotherCore processes input
                → Language detection, emotion detection, typing analysis

Pipeline:       Inside MotherCore's 18 steps
                → Any step can be extended or a new step inserted

Post-pipeline:  After response is generated
                → Response filtering, format adjustment, translation

Side-channel:   Background thread
                → Monitoring, profiling, self-audit, watchdog
```

---

### Component 7.3 — LiveSkillBuilder

**Purpose:** For script-based skills (Python, batch) — no recompile needed. Build and register instantly.

**Example:**
```
You: "Learn to give me a morning report every day —
      weather + top news + my PC status"

LiveSkillBuilder:
  → Writes: morning_report.py
      fetch_weather(location="auto")
      fetch_news(category="top", count=5)
      get_pc_status()
      format_and_display()
  → Registers in SkillRegistry:
      trigger: ["morning report", "daily briefing", "subah ki report"]
      script: morning_report.py
  → "Morning report skill is active. Say 'morning report' anytime."

INSTANT. No recompile. No downtime.
```

---

# PSYCHOLOGICAL LEARNING LAYER
## "She understands humans, not just words"

### This is a cross-phase capability that builds across phases 1-4

**What she learns:**
```
Human Psychology:
  → Maslow's hierarchy → what every human fundamentally needs
  → Big Five personality model (OCEAN)
  → Attachment theory → why people connect or avoid
  → Cognitive biases → why humans think the way they do

Communication Psychology:
  → Active listening
  → Mirroring communication style
  → Reading between the lines
  → The 7-38-55 rule (7% words, 38% tone, 55% body language)
  → When silence is the right response

Difficult Personalities:
  → Narcissistic → how to handle
  → Highly anxious → what they need
  → Aggressive → how to de-escalate
  → Emotionally closed → how to open up
  → Introverts vs extroverts → different needs

What she builds with this knowledge:
  → YOUR personal psychological profile (built over time)
  → Communication style that matches YOU specifically
  → Reading between the lines of what you say
  → Detecting when you're stressed, frustrated, excited
  → Adapting response length, tone, depth to your current state
```

---

# SAFETY RULES — PERMANENT, NEVER REMOVED

```
RULE 1: PLAN BEFORE ACT
  She never does anything to your system without showing
  you the plan first and getting your approval.

RULE 2: NEVER AUTO-DELETE
  Files are NEVER deleted automatically.
  Always moved to a Review folder.
  You decide what gets deleted.

RULE 3: FULL UNDO LOG
  Every file operation, every system change is logged.
  "Yuki undo" reverses everything possible.

RULE 4: ASK BEFORE INSTALL
  No software, library, or tool is installed without
  explicitly asking you first and you saying yes.

RULE 5: ASK BEFORE RECOMPILE
  She never modifies her own code or rebuilds herself
  without showing you the change and getting your yes.

RULE 6: NEVER ILLEGAL
  Hard wall. Permanently blocked.
  No hacking, no accessing others' accounts,
  no malware, no privacy violations.

RULE 7: HONEST ABOUT UNCERTAINTY
  If she doesn't know → she says so.
  If she might be wrong → she tells you.
  Never pretends to be certain when she isn't.

RULE 8: YOUR DATA IS YOURS
  She never sends your personal data anywhere
  without your explicit instruction.
```

---

# BUILD ORDER — WHAT TO BUILD NEXT

| Priority | Phase | Component | Why first |
|---|---|---|---|
| 1 | Phase 1 | SemanticParser | Foundation of everything |
| 2 | Phase 1 | GoalModel | Required by all phases |
| 3 | Phase 1 | LanguageLayer | Hindi + multilingual now |
| 4 | Phase 3 | Enhanced ClarificationEngine | Immediate improvement |
| 5 | Phase 2 | CapabilityMap | Needed by Planner |
| 6 | Phase 3 | AutonomousPlanner | Needed by Executor |
| 7 | Phase 4 | SystemExecutor | PC control unlocked |
| 8 | Phase 4 | DependencyInstaller | Tool installation |
| 9 | Phase 4 | VerificationEngine | Confirm things worked |
| 10 | Phase 5 | Watchdog | Crash recovery |
| 11 | Phase 5 | PerformanceProfiler | Find bottlenecks |
| 12 | Phase 5 | SelfRewriter | Fix bottlenecks |
| 13 | Phase 5 | RebuildManager | Apply fixes safely |
| 14 | Phase 6 | CodeReader | Understand own code |
| 15 | Phase 6 | BottleneckAnalyser | Deep analysis |
| 16 | Phase 6 | CodeOptimiser | Write the fix |
| 17 | Phase 7 | LiveSkillBuilder | Instant new skills |
| 18 | Phase 7 | FeatureWriter | New C++ features |
| 19 | Phase 7 | IntegrationPlanner | Wire new features |
| 20 | Phase 7 | DependencyInstaller | Install for new features |

---

# WHAT YUKI WILL BE ABLE TO DO — AFTER ALL 7 PHASES

```
You say ANYTHING in ANY LANGUAGE
  → She understands the meaning from the core
  → She knows what she can and cannot do
  → She learns what she doesn't know — from the internet, from docs
  → She asks you only the questions she genuinely needs answered
  → She builds her own step-by-step plan
  → She shows you the plan and asks permission
  → She does the work — controls your PC, software, files, devices
  → She verifies the result
  → She reports back and asks for feedback
  → She continuously improves herself in the background
  → She fixes her own bugs and crashes
  → She adds new skills when you ask
  → She optimises her own code when you ask
  → She redesigns her own UI when you give her freedom

All of this — on your PC — loyal only to you.
No cloud. No subscription. No third party seeing your data.
```

---

> **This document is the master plan.
> When we build, we follow this.
> When we forget, we come back here.**
