# ADDED_COMPONENTS.md — Yuki_1.0

## Stage Added
Presence Stage A: Floating Presence Shell
Stage 7 — Mouth (Output awareness)
Stage 10 — Command Routing & Subsystem Sync
Stage 11 — Subsystem Execution Hooks (Desired vs Runtime separation)

---

## Files Created/Updated

| File | Change |
|---|---|
| `src/CommandRouter.h` | NEW: Unified Command Parser and Router header |
| `src/CommandRouter.cpp` | UPDATED: Prints both desired and runtime states in status, handles subsystem startup/apply failure notifications |
| `src/PresenceShell.h` | NEW: Win32 native transparent floating shell header |
| `src/PresenceShell.cpp` | UPDATED: Redesigned subsystem icon clicks to call SubsystemControl's unified toggle helpers |
| `src/main.cpp` | UPDATED: Respects speaker/synthesis active status in avatar animation state transitions |
| `src/SubsystemControl.h` | UPDATED: Declared SubsystemRuntimeState enum (STOPPED, RUNNING, UNAVAILABLE, FAILED) and added setRuntimeStateQuery callback to map physical threads |
| `src/SubsystemControl.cpp` | UPDATED: Integrated setRuntimeStateQuery querying background threads to perfectly display live RUNNING/UNAVAILABLE state on the floating UI dock |
| `src/AudioInputRuntime.h` | NEW: Audio input runtime thread manager class |
| `src/AudioInputRuntime.cpp` | NEW: Windows waveIn real-time PCM audio capture loop with double buffering, RMS volume calculation, and silent white-noise simulation fallback |
| `src/AudioOutputRuntime.h` | NEW: Audio output text-to-speech speaker runtime class |
| `src/AudioOutputRuntime.cpp` | NEW: Native Windows Speech API (SAPI) ISpVoice implementation, providing fully asynchronous real spoken output with selective suppression when MOUTH is toggled off |
| `src/CameraRuntime.h` | NEW: Video environment observer runtime class |
| `src/CameraRuntime.cpp` | NEW: Video capture background thread that queries DirectShow and continuously streams environmental frame metrics to the Unified Perception Layer |
| `src/ScreenRuntime.h` | NEW: Screen-focused raster capture runtime class |
| `src/ScreenRuntime.cpp` | NEW: GDI BitBlt desktop raster capture loop that hashes screen grids and maps foreground process metadata every 1s to the Unified Perception Layer |
| `src/Mouth.h` | NEW: MouthSnapshot struct and Reader class |
| `src/Mouth.cpp` | UPDATED: Maps output_pipeline_active state directly to live speaker SAPI runtime state |
| `src/Ear.cpp` | UPDATED: Maps capture_pipeline_active state directly to live waveIn audio runtime state |
| `src/BabyMode.h` | UPDATED: Instantiates live AudioInput, AudioOutput, Camera, and Screen runtimes as owned instances |
| `src/BabyMode.cpp` | UPDATED: Connects BabyMode responses to real SAPI TTS voice output, registers thread state query callbacks, and handles background startup |
| `src/VisionManager.h` | UPDATED: Added isCameraHardwarePresent check |
| `src/VisionManager.cpp` | UPDATED: Implemented isCameraHardwarePresent querying DirectShow category |
| `src/AvatarBody.cpp` | UPDATED: Prioritized active SPEAKING state over environmental OBSERVING/FOCUSED perception aura overrides to fix mouth movement animation |
| `src/test_subsystems.cpp` | UPDATED: Added a comprehensive unit test suite covering the 8 perception verification cases |
| `CMakeLists.txt` | UPDATED: Compiled and linked the new background runtime C++ source files into both core targets |

---

## Active Pipeline

**Baby Mode + CommandRouter + UnifiedPerceptionLayer + PerceptionEvent + SubsystemControl + Body + Screen + Ear + Mouth.**

All raw sensor captures and user input signals are converted into a normalized `PerceptionEvent` before they are routed or evaluated by any downstream decision logic.

```
Raw Signals
 (Text, Voice, Camera, Screen, Internal)
                 │
                 ▼
     [ Unified Factory Adapters ]
                 │
                 ▼
  [ PerceptionEvent (Normalized Payload) ]
                 │
                 ▼
    UnifiedPerceptionLayer::submitEvent()
                 ├─► [ Dispatch Registered Listeners ]
                 └─► [ Log to Safe Event History ]
                 │
                 ▼
     [ CommandRouter::route(Event) ]
                 ├─► Command Found ──► Execute & emit INTERNAL event
                 └─► Chat / Observation ──► BabyMode reflex
```

Live SAPI TTS spoken output, waveIn audio PCM capture, DirectShow video environment streaming, and desktop BitBlt raster capture are now fully activated as live, asynchronous background threads.
Mouth provides output situational awareness and gates/channels live SAPI audio.
It is strictly managed by the `MOUTH` subsystem and runtimes.
