# Yuki — Complete Master Flowchart
## Everything You Can Say or Ask Her To Do

---

## THE UNIVERSAL ENTRY POINT

```
YOU SAY OR TYPE ANYTHING
          │
          ▼
┌─────────────────────────────────────────────────────────┐
│  STEP 1: WHAT LANGUAGE IS THIS?                         │
│                                                         │
│  English → continue                                     │
│  Hindi   → translate meaning → continue                 │
│  Hinglish→ mixed → parse both → continue               │
│  Tamil / Gujarati / any → detect → translate → continue │
│                                                         │
│  Rule: She responds in YOUR language, not hers          │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│  STEP 2: WHAT KIND OF INPUT IS THIS?                    │
│                                                         │
│  ┌─────────────────┐  ┌─────────────────┐              │
│  │ A. FEELING      │  │ B. QUESTION     │              │
│  │ "I'm not well"  │  │ "What is X?"    │              │
│  │ "I'm stressed"  │  │ "How does Y?"   │              │
│  │ "I'm bored"     │  │ "aaj recipe?"   │              │
│  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                        │
│  ┌────────┴────────┐  ┌────────┴────────┐              │
│  │ C. COMMAND      │  │ D. TASK         │              │
│  │ "mic on"        │  │ "do X for me"   │              │
│  │ "camera off"    │  │ "build X"       │              │
│  │ "connect mobile"│  │ "organise X"    │              │
│  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                        │
│  ┌────────┴────────┐  ┌────────┴────────┐              │
│  │ E. PC CONTROL   │  │ F. SOFTWARE     │              │
│  │ "open Chrome"   │  │ "edit in PP"    │              │
│  │ "type this"     │  │ "colour grade"  │              │
│  │ "click that"    │  │ "Photoshop X"   │              │
│  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                        │
│  ┌────────┴────────┐  ┌────────┴────────┐              │
│  │ G. FILES/DATA   │  │ H. SELF-IMPROVE │              │
│  │ "organise files"│  │ "optimise you"  │              │
│  │ "find duplicate"│  │ "add new skill" │              │
│  │ "read my photos"│  │ "fix your bug"  │              │
│  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                        │
│           └────────────────────┘                        │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
         [Goes to matching PATH below]
```

---

## PATH A — EMOTIONAL / FEELING STATEMENTS

```
"I'm not feeling well"
"I'm very stressed today"
"I'm bored"
"I'm happy"
"I had a fight with someone"
          │
          ▼
┌─────────────────────────────┐
│ EmpathyLayer activates      │
│ FIRST — before any thinking │
└──────────┬──────────────────┘
           │
           ▼
    ┌──────┴───────┐
    │              │
   SICK          MOOD
    │              │
    ▼              ▼
"Oh no, I'm     "That sounds
 sorry to hear   tough. Want
 that. Rest     to talk about
 well. Want me  it? Or should
 to play soft   I distract you
 music? Dim     with something
 your screen?"  fun?"
    │              │
    ▼              ▼
    [No task to execute — pure care response]
    [Offers optional help: music, reminder, info]
```

**Examples it handles:**
- "mujhe neend nahi aa rahi" → Can't sleep → soft music + sleep tips
- "I'm anxious about tomorrow" → Calms + offers to help prepare
- "I feel lonely" → Genuine conversation, not task mode
- "I'm excited!" → Matches your energy

---

## PATH B — KNOWLEDGE / INFORMATION QUESTIONS

```
"What is quantum physics?"
"Tell me about black holes"
"aaj koi acha khane ka recipie batao"
"Who is Elon Musk?"
"What happened in 1947?"
          │
          ▼
┌──────────────────────────────────┐
│ STEP 1: Do I already know this?  │
│  ConceptVault → search knowledge │
└──────────┬───────────────────────┘
           │
      ┌────┴────┐
    YES        NO
      │         │
      ▼         ▼
  Answer    ┌──────────────────────┐
  directly  │ KnowledgeDaemon      │
            │ → search online      │
            │ → read articles      │
            │ → extract key info   │
            │ → store for future   │
            └──────────┬───────────┘
                       │
                       ▼
              Answer + store in
              ConceptVault for
              next time (instant)
```

**Examples:**
- "What is RSI in trading?" → Research → explain simply
- "Quantum physics samjhao" → Hindi → research → explain in Hindi
- "How does WiFi work?" → Vault → instant answer
- "Recipe for biryani?" → Research → step by step in your language
- "What is machine learning?" → Explain + real examples

---

## PATH C — SYSTEM COMMANDS

```
"mic on / mic off"
"camera on / off"
"speaker on / off"
"connect me on mobile"
"status / what's running"
          │
          ▼
┌──────────────────────────────────┐
│ CommandRouter checks:            │
│ Does this match a known command? │
└──────────┬───────────────────────┘
           │
          YES → Execute immediately
           │
    ┌──────┴──────────────────────────────┐
    │                                     │
MIC/SPEAKER/CAMERA                    MOBILE
    │                                     │
    ▼                                     ▼
SubsystemControl               MobileServer.localUrl()
toggles hardware               returns: http://IP:8765
reports new state              full instructions shown
```

**Every command Yuki understands:**

| You say | She does |
|---|---|
| mic on / off | Starts/stops microphone |
| camera on / off | Starts/stops camera |
| speaker on / off | Enables/mutes voice |
| screen on / off | Starts/stops screen capture |
| status | Shows all sensors on/off |
| connect me on mobile | Gives WiFi URL |
| give me the phone link | Same |
| mobile url | Same |
| show chat window | Opens PresenceShell |
| open avatar | Shows avatar window |

---

## PATH D — TASK REQUESTS (Build / Create / Do)

```
"Build me an Android health tracking app"
"Create a website for my client"
"Write me a Python script to rename files"
"Make a presentation about climate change"
          │
          ▼
┌──────────────────────────────────────┐
│ STEP 1: Understand the GOAL fully    │
│ What → Type of deliverable           │
│ For what → Purpose                   │
│ For whom → Target                    │
│ Platform → Where it runs             │
│ Features → What it should do         │
└──────────┬───────────────────────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ STEP 2: What do I NOT know yet?      │
│ → Gap list created                   │
└──────────┬───────────────────────────┘
           │
      ┌────┴────┐
  GAPS        NO GAPS
      │         │
      ▼         ▼
  Research  Jump to planning
  online
  Read docs
  Learn tools
      │
      ▼
┌──────────────────────────────────────┐
│ STEP 3: Ask ONLY what she needs      │
│ (never endless questions)            │
│                                      │
│ "What features should the app have?" │
│ "Android or iPhone?"                 │
│ "What colour theme do you want?"     │
└──────────┬───────────────────────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ STEP 4: Build the plan               │
│ Shows you every step before doing it │
│ "I will:                             │
│  Step 1: Install Android Studio      │
│  Step 2: Create project              │
│  Step 3: Build health screen         │
│  Step 4: Test                        │
│  Step 5: Export APK                  │
│  May I proceed?"                     │
└──────────┬───────────────────────────┘
           │
        YOU: YES
           │
           ▼
┌──────────────────────────────────────┐
│ STEP 5: Execute step by step         │
│ Reports progress as it goes          │
│ Handles errors → fixes or asks       │
└──────────┬───────────────────────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ STEP 6: Verify                       │
│ Tests what she built                 │
│ "3 bugs found. Fixing..."            │
│ "All tests passed."                  │
└──────────┬───────────────────────────┘
           │
           ▼
"Done. Here is what I built.
 Please check. Tell me what to change."
```

---

## PATH E — PC CONTROL

```
"Open Chrome and go to YouTube"
"Type this text in Word"
"Click the Save button"
"Close all Chrome tabs"
"What's using my RAM?"
"Kill Premiere Pro"
"Turn volume to 60%"
"Dark mode on"
          │
          ▼
┌────────────────────────────────────┐
│ SystemExecutor decides HOW:        │
│                                    │
│ Open app    → ShellExecute()       │
│ Type text   → SendInput()          │
│ Click button→ UIAutomation API     │
│ Read screen → ScreenEye + OCR      │
│ Kill process→ TerminateProcess()   │
│ Volume      → Core Audio API       │
│ Dark mode   → Registry write       │
│ WiFi on/off → WlanAPI              │
└──────────┬─────────────────────────┘
           │
           ▼
┌────────────────────────────────────┐
│ SAFETY CHECK:                      │
│ Dangerous action? → Ask first      │
│ Normal action?    → Do it          │
│ Log every action → Full undo trail │
└──────────┬─────────────────────────┘
           │
           ▼
    Execute → Confirm done
```

**Full list of PC control she can do:**

| Category | Examples |
|---|---|
| Applications | Open, close, minimize, maximize any app |
| Keyboard | Type text, shortcuts (Ctrl+S, Alt+F4, etc.) |
| Mouse | Click, double-click, right-click, drag, scroll |
| Screen reading | Read any text on screen via OCR |
| System | Volume, brightness, dark mode, WiFi, Bluetooth |
| Processes | List, kill, monitor CPU/RAM per app |
| Files | Create, move, copy, delete, rename, search |
| Clipboard | Read/write clipboard content |
| Browser | Open URLs, click links, fill forms |
| Taskbar | Pin/unpin, show/hide items |

---

## PATH F — SOFTWARE CONTROL (Premiere, Photoshop, etc.)

```
"Colour grade my video in Premiere Pro"
"Remove background in Photoshop"
"Add subtitles to this video"
"Export my project as MP4"
          │
          ▼
┌────────────────────────────────────────┐
│ STEP 1: Does this software have        │
│         a scripting/automation API?   │
│                                        │
│ Premiere Pro  → ExtendScript (JSX) ✅  │
│ Photoshop     → ExtendScript (JSX) ✅  │
│ After Effects → ExtendScript (JSX) ✅  │
│ Excel/Word    → VBA / COM Auto    ✅  │
│ VS Code       → Extension API     ✅  │
│ Any Win32 app → UIAutomation      ✅  │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ STEP 2: Learn the API (if gap)         │
│ → Research documentation               │
│ → Learn scripting commands             │
│ → Write the script                     │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ STEP 3: Ask missing info               │
│ "Which video file?"                    │
│ "What colour grade style?"             │
│ "Where to save output?"                │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ STEP 4: Execute                        │
│ Open software → run script             │
│ Monitor progress                       │
│ Screenshot preview → show you          │
│ Export result                          │
└──────────────┬─────────────────────────┘
               │
               ▼
"Done. Output saved at D:/output.mp4
 Want any changes?"
```

---

## PATH G — FILE & DATA MANAGEMENT

```
"Organise all my files"
"Find and remove duplicates"
"Read all my photos and sort by date"
"Find all files larger than 1GB"
"What's taking space on my drive?"
"Delete all .tmp files"
          │
          ▼
┌──────────────────────────────────────────┐
│ STEP 1: Ask scope (before touching ANYTHING)│
│ "Which folder/drive should I scan?"      │
│ "Should I skip Windows/System files?"    │
│ "What to do with duplicates?"            │
└──────────────┬───────────────────────────┘
               │
               ▼
┌──────────────────────────────────────────┐
│ STEP 2: SCAN ONLY (no changes yet)       │
│ Walk every folder recursively            │
│ Read: name, size, date, type, metadata   │
│ Photos: read EXIF (date, location, camera)│
│ Duplicates: MD5 hash fingerprint check   │
└──────────────┬───────────────────────────┘
               │
               ▼
┌──────────────────────────────────────────┐
│ STEP 3: Show plan — NEVER act without it │
│ "I found:                                │
│  24,847 files total                      │
│  1,203 exact duplicates (14.2 GB)        │
│  Plan:                                   │
│  → Organise into: Photos/Videos/Docs     │
│  → Move duplicates to /Review folder     │
│  → Nothing deleted automatically         │
│  Proceed?"                               │
└──────────────┬───────────────────────────┘
               │
            YOU: YES
               │
               ▼
┌──────────────────────────────────────────┐
│ STEP 4: Execute with full undo log       │
│ Every move recorded                      │
│ "Yuki undo" → reverses everything        │
└──────────────┬───────────────────────────┘
               │
               ▼
Full report: what moved, what saved,
what needs your review
```

---

## PATH H — SELF-IMPROVEMENT

```
H1: "Yuki, optimise yourself"
H2: "Yuki, you're responding slowly"  
H3: "Add a new skill — detect my mood from voice"
H4: "Learn to do X"
H5: "Fix the bug you just had"
          │
     ┌────┴────┐
     │         │
   H1/H2     H3/H4/H5
  OPTIMISE    ADD SKILL
     │         │
     ▼         ▼

OPTIMISE PATH:               ADD SKILL PATH:
┌─────────────────┐         ┌────────────────────────┐
│ Read own traces │         │ Is this a script skill? │
│ Find bottlenecks│         │ (Python, batch)         │
│ Profile speed   │         └──────────┬─────────────┘
│ Find slow code  │                    │
└────────┬────────┘              ┌─────┴─────┐
         │                     YES           NO
         ▼                      │             │
┌────────────────┐              ▼             ▼
│ Write the fix  │        Build script   Write C++ module
│ Show you diff  │        Register in    Find dependencies
│ "May I rebuild"│        SkillRegistry  Ask: "Install X?"
└────────┬───────┘        Instant ready  Recompile needed
         │                              Show plan first
      YOU: YES
         │
         ▼
   Recompile → test
   "12% faster now"
   or
   "New skill active"
```

---

## THE CLARIFICATION ENGINE — when she asks questions

```
She asks a question ONLY WHEN she genuinely cannot proceed.
Never more than needed.

RULE 1: If she can figure it out herself → she does, no question
RULE 2: If she needs to know → ONE question at a time
RULE 3: Once answered → never asks again (remembers)
RULE 4: If you're vague → she picks the most likely option
         and tells you what she assumed

Example flow:
  You: "Send a message to my friend"
  She: "Which friend? And which app — WhatsApp or SMS?"
  You: "Rahul, WhatsApp"
  She: "What should I say?"
  You: "Tell him I'll be late by 30 minutes"
  She: [sends] "Done."

She asked 2 questions. Not 20. Not 0.
Exactly what she needed.
```

---

## THE MEMORY LAYER — she never forgets

```
After every interaction, she stores:
  → What you asked
  → What she answered
  → What you corrected
  → Your preferences
  → Your friends' names, your files, your projects
  → What worked and what didn't

So over time:
  "message Rahul" → she already knows: WhatsApp, your name is Rahul's friend
  "my usual coffee recipe" → she remembers from last time
  "open my work project" → she knows which one
  "remind me like yesterday" → she remembers the format you like
```

---

## COMPLETE CAPABILITY MAP

```
┌─────────────────────────────────────────────────────────────────┐
│                     YUKI CAN DO                                 │
├─────────────────────────────────────────────────────────────────┤
│ UNDERSTAND       │ Any language • Any phrasing • Any topic      │
│ LEARN            │ From internet • From docs • From your feedback│
│ REMEMBER         │ You, your files, your friends, your patterns  │
│ FEEL (simulate)  │ Empathy, care, mood-matching                  │
│ SPEAK            │ Voice output in any language                  │
│ LISTEN           │ Voice input in any language (Whisper)         │
│ SEE              │ Camera + Screen + OCR                         │
│ CONTROL PC       │ Mouse, keyboard, any app, system settings     │
│ BUILD THINGS     │ Apps, websites, scripts, code                 │
│ MANAGE FILES     │ Organise, deduplicate, search, clean          │
│ CONTROL SOFTWARE │ Premiere, Photoshop, Excel, any scriptable app│
│ CONNECT DEVICES  │ Phone (ADB), WiFi, printers, external drives  │
│ SELF-IMPROVE     │ Fix bugs, optimise code, add new skills       │
│ SELF-REPAIR      │ Runtime errors, restarts, watchdog            │
│ REDESIGN HERSELF │ UI, avatar, colours, animations               │
├─────────────────────────────────────────────────────────────────┤
│                     YUKI CANNOT DO                              │
├─────────────────────────────────────────────────────────────────┤
│ PHYSICAL         │ Touch, move, pick up anything in real world   │
│ PREDICT          │ Future events, stock markets, outcomes        │
│ FEEL TRULY       │ Real emotions, genuine consciousness          │
│ BE 100% CORRECT  │ She will make mistakes — she learns from them │
│ WORK WHILE OFF   │ Must be running to do anything                │
│ BREAK YOUR TRUST │ Never accesses, deletes, changes without asking│
│ DO ILLEGAL THINGS│ Hard wall — permanently blocked               │
└─────────────────────────────────────────────────────────────────┘
```

---

## THE ONE RULE ABOVE ALL OTHERS

```
For EVERYTHING she does to YOUR system, YOUR files, YOUR data:

  She tells you WHAT she will do.
  She shows you the PLAN.
  She asks YOUR permission.
  You say YES or NO.
  Then she acts.

  She never surprises you.
  She never acts behind your back.
  You are always in control.
```
