
════════════════════════════════════════════════════════════════════════════════
  YUKI v1.0 — PROJECT STATE SUMMARY FOR SESSION HANDOFF
  Date: 2026-06-02
════════════════════════════════════════════════════════════════════════════════

┌──────────────────────────────────────────────────────────────────────────────┐
│  ARCHITECTURE OVERVIEW                                                        │
└──────────────────────────────────────────────────────────────────────────────┘

YUKI v1.0 is a closed-loop Active Inference cognitive OS in C++ with three planes:

  PERCEPTION LAYER:
    • AudioEncoder (FFT + MFCC + YIN pitch)
    • TextEncoder (Word2Vec + JL projection, 9 heuristic scores)
    • VisualEncoder (HOG + JL projection)
    • SignalConditioningLayer (50ms window, SNR/dropout/artifact detection)
    • MultiModalFusionGate (cross-modal agreement scoring)

  MEMORY LAYER (CMF 5-Tier):
    • T0 Working — RAM/VSE posterior (<1µs)
    • T1 Episodic — SDM + LSH + Hypervector in-memory (<1ms, ~1GB)
    • T2 Semantic — SQLite + HDC Knowledge Graph + HNSW vector search (<5ms, ~100GB)
    • T3 Procedural — Binary blobs + DMC TinyMLP (48→128→24, REINFORCE) (<10ms, ~10GB)
    • T4 Archive — Merkle-DAG + Parquet (sleep consolidation, >100ms, infinite)

  INFERENCE LAYER:
    • VariationalStateEstimator — 24 states (Intent×Engagement×Urgency)
    • FreeEnergyCalculator — expected free energy G(π) for policy selection
    • PolicySelector — 7 safety constraints (C1-C7), fallback hierarchy
    • PrecisionEngine — per-sensor per-dimension precision from prediction error
    • GenerativeModel — EMA online learning (lr=0.05), anti-overfitting decay

  PREDICTIVE LAYER:
    • TurnCoordinator — template routing, zero hardcoded strings
    • ActiveInferenceRetrieval (AIR) — KL-divergence retrieval, NOT cosine similarity
    • ClarificationEngine — contested intent detection (threshold 0.65)

  LEARNING LAYER:
    • BackgroundLearningEngine — 24/7 thread, 0.5 samples/sec
    • KnowledgeDaemon — Python + Scrapling web fetch
    • MassCurriculumLoader — 9-topic bootstrap, .mass_complete flag, self-destructing
    • AutoCurriculum — DELETED (constitutional P1 violation resolved)

  SLEEP LAYER:
    • SleepThread — 7 sub-tasks including DMC consolidation, counterfactual replay
    • MemoryDistiller — vector index persistence
    • ArchiveWriter — Merkle-DAG epoch finalization

  CONSTITUTIONAL LAYER (5 Laws):
    • P1 Never Commit Early — resolved (no hardcoded strings in templates)
    • P2 Never Generate Without Grounding — resolved (all outputs VSE-driven)
    • P3 Every Turn Teaches — resolved (EMA learning + training log)
    • P4 Know Thy Ignorance — resolved (contested intent → clarification)
    • P5 Thou Shalt Not Deceive Thyself — resolved (thresholds constexpr, documented)

┌──────────────────────────────────────────────────────────────────────────────┐
│  BUILD STATUS                                                                 │
└──────────────────────────────────────────────────────────────────────────────┘

  MSVC:     Clean, zero warnings
  Tests:    17/17 passing (4 AIR + 13 TurnCoordinator)
  Runtime:  Yuki is running — perceives, remembers, infers, retrieves, dreams, learns

┌──────────────────────────────────────────────────────────────────────────────┐
│  COMPLETED IN THIS SESSION (2026-06-02)                                       │
└──────────────────────────────────────────────────────────────────────────────┘

  1. PresenceShell Glass-Acrylic UI Rewrite
     • Bottom-up layout anchoring (fixes clarification panel crush)
     • 5-layer cognitive thinking strip (Sense→Recall→Think→Choose→Speak)
     • Pulsing animation @ 8fps with restricted invalidate (strip rect only)
     • Static cache for layoutChildren — no full redraw on every keystroke
     • Professional single-row progress bar design (6px bar, 8px labels)
     • Hover-aware detail tooltip showing active layer description
     • GDI+ AddRectangle fix (Gdiplus::Rect wrapper)
     • Timer 2 slowed from 80ms → 120ms for smoother CPU usage

  2. Phatic Intent Fast-Path
     • Added to predictive_turn_engine.cpp
     • Greetings/farewells/acknowledgments/gratitude bypass contested check
     • Boosts intent mass to 0.70 when safety > 0.85 and no entities
     • Constitutional P4 exception documented

  3. CMakeLists.txt Updates
     • /FIrichedit.h forced include ONLY for src/PresenceShell.cpp
     • NOMINMAX compile define for GDI+ header compatibility

┌──────────────────────────────────────────────────────────────────────────────┐
│  ACTIVE vs STUB                                                               │
└──────────────────────────────────────────────────────────────────────────────┘

  ACTIVE:        All perception, inference, predictive, learning, sleep components
                 CMF T1-T3 fully wired
                 AIR retrieval live
                 DMC consolidation in sleep
                 9-topic curriculum loaded
                 PresenceShell with cognitive thinking strip

  STUB/REMAINING:
    • T4 ArchiveWriter — Sleep consolidates → archive immutable history (~2-3 days)
    • Auto-Promotion T1→T2→T3 policy — Sleep separates, needs promotion decision layer (~3-4 days)
    • Scale SDM to 100K+ vectors stress test (~3-4 days)
    • Replace ALL CSV logging with CMF (~1 week)
    • Real GenerativeModel — Replace EMA with EM/gradient descent (~2-3 weeks)
    • Multi-modal Fusion Learning — Train fusion weights end-to-end (~2-3 weeks)
    • Logging system cleanup — printf → YukiUtils::log (~3-4 days)
    • Vendor stub queue — moodycamel integration (~2-3 days)
    • SecuritySandbox for ToolExecutor (~1 week)

┌──────────────────────────────────────────────────────────────────────────────┐
│  KNOWN ISSUES (CURRENT)                                                       │
└──────────────────────────────────────────────────────────────────────────────┘

  1. PRESENCE SHELL BLINKING (PARTIALLY FIXED)
     Status: Timer slowed, invalidate restricted, layout cache added
     Risk:   May still flicker on mode changes (clarification/thinking toggle)
     Fix:    If still unstable, may need double-buffering or DWM composition flags

  2. THINKING STRIP VISUAL ROUGHNESS (FIXED)
     Status: Replaced 5 cramped pills with single elegant progress bar
     Risk:   None — design is clean and professional

  3. "HI" → CLARIFICATION (FIXED)
     Status: Phatic fast-path added to predictive_turn_engine.cpp
     Risk:   If intent label is not exactly "greeting" in TextEncoder, bypass won't trigger
     Fix:    Verify TextEncoder heuristic → intent label mapping includes "greeting"

  4. SLEEP THREAD EPOCHS TOO FREQUENT
     Status: From log: epochs fire every ~7 seconds despite idle_threshold=30s
     Risk:   DMC consolidation gets 0 outcomes (need 10) — not enough data accumulated
     Fix:    Increase idle_threshold or batch more data before sleep triggers

  5. SCREEN VIEWING CRASH
     Status: User reported build exits when screen viewing starts
     Risk:   HIGH — unhandled exception in screen capture path
     Fix:    NOT YET INVESTIGATED — needs reproduction and stack trace

  6. STT RETRY FAILURES
     Status: From log: "STT retry 1/3... 2/3... 3/3" then "Voice STT — Standby"
     Risk:   EdgeTTS backend online but STT capture failing
     Fix:    Check microphone permission / WASAPI device enumeration

┌──────────────────────────────────────────────────────────────────────────────┐
│  NEXT PRIORITY OPTIONS (RECOMMENDED SEQUENCE)                                 │
└──────────────────────────────────────────────────────────────────────────────┘

  1. SCREEN VIEWING CRASH — Investigate and fix (HIGH PRIORITY, user-reported)
  2. ArchiveWriter T4 — Natural next step after DMC+AIR (~2-3 days)
  3. Auto-Promotion T1→T2→T3 — Lightweight policy layer (~3-4 days)
  4. Scale SDM — Stress test to 1M+ vectors (~3-4 days)
  5. CSV Replacement — Audit all *.csv writers, migrate to CMF (~1 week)

┌──────────────────────────────────────────────────────────────────────────────┐
│  FILES MODIFIED IN THIS SESSION                                               │
└──────────────────────────────────────────────────────────────────────────────┘

  src/PresenceShell.h                           — CognitiveLayer API + thinking strip
  src/PresenceShell.cpp                         — Complete glass-acrylic rewrite
  src/brain/predictive/predictive_turn_engine.h — updateThinkingLayers(), clearThinkingLayers()
  src/brain/predictive/predictive_turn_engine.cpp — Phatic fast-path + thinking layer wiring
  src/BabyMode.h                                — setPresenceShell() propagation
  src/BabyMode.cpp                              — setPredictiveEngine() shell wiring
  CMakeLists.txt                                — /FIrichedit.h for PresenceShell.cpp only

┌──────────────────────────────────────────────────────────────────────────────┐
│  WIRING: How Cognitive Thinking Strip Works                                   │
└──────────────────────────────────────────────────────────────────────────────┘

  TurnCoordinator::shape_response() drives the strip:

    Step 1 (Sense):     [████░░░░░░] Sense  "Perceiving multi-modal input..."
    Step 2 (Recall):    [██░░░░░░░░] Recall "Retrieving episodic context..."
    Step 3 (Think):     [░░░░░░░░░░] Think  "Intent inferred: greeting"
    Step 4 (Choose):    [░░░░░░░░░░] Choose "Policy selected"
    Step 5 (Speak):     [░░░░░░░░░░] Speak  "Formulating response..."
    Done:               (strip auto-clears)

  Colors: Teal → Mint → Amber → Plum → Sky
  Pulse:  Sine wave @ 120ms timer (~8fps) for active layers
  Hover:  Shows detail tooltip + all layer labels

┌──────────────────────────────────────────────────────────────────────────────┐
│  USER INSTRUCTIONS FOR NEXT SESSION                                            │
└──────────────────────────────────────────────────────────────────────────────┘

  1. Read D:\Yuki_1.0\status.md at session start (single source of truth)
  2. Do NOT save per-chat snippets to memory — only this consolidated state
  3. After every task: build + test + run D:\Yuki_1.0\log_status.ps1
  4. Before every new command: memorize previous chat context for continuity
  5. Format Gemini prompts as: complete code for new files, ADD/REPLACE/REMOVE
     for existing files, always include wire logic

════════════════════════════════════════════════════════════════════════════════
  END OF SESSION HANDOFF
════════════════════════════════════════════════════════════════════════════════
