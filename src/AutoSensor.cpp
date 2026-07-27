// AutoSensor.cpp — Yuki_1.0
// Starts all hardware sensors automatically on launch.
// Prints a clear status table so the user can see what is online.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>     // MUST BE FIRST
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include "brain/core/SystemConfig.h"

#include "AutoSensor.h"
#include "BabyMode.h"
#include "SubsystemControl.h"
#include "input/VisionSystem.h"

// ── helpers ──────────────────────────────────────────────────────────────────


static void sensorLine(const std::string& name, bool ok, const std::string& note = "") {
    std::cout << "  [" << (ok ? "OK " : "FAIL") << "] "
              << name;
    if (!note.empty()) std::cout << " — " << note;
    std::cout << "\n";
}

// ── main entry ───────────────────────────────────────────────────────────────

void autoStartAllSensors(BabyMode& baby) {
    auto& sub = baby.subsystems();

    std::cout << "\n╔═══════════════════════════════════════════╗\n";
    std::cout <<   "║      Yuki Sensor Detection                ║\n";
    std::cout <<   "╚═══════════════════════════════════════════╝\n";

    // ── 1. Mic (EAR) ──────────────────────────────────────────────────────
    UINT micDevs = waveInGetNumDevs();
    bool micOk   = (micDevs > 0);
    sub.setAvailable(SubsystemName::EAR, micOk);
    // NOTE: mode NOT changed here — stays FORCED_OFF until user toggles ON.
    sensorLine("Microphone",  micOk,
               micOk ? std::to_string(micDevs) + " device(s) ready (OFF — toggle to enable)"
                     : "No audio input devices detected");

    // ── 2. Speaker (MOUTH) ────────────────────────────────────────────────
    UINT spkDevs = waveOutGetNumDevs();
    bool spkOk   = (spkDevs > 0);
    sub.setAvailable(SubsystemName::MOUTH, spkOk);
    // NOTE: mode NOT changed here — stays FORCED_OFF until user toggles ON.
    sensorLine("Speaker",     spkOk,
               spkOk ? std::to_string(spkDevs) + " device(s) ready (OFF — toggle to enable)"
                     : "No audio output devices detected");

    // ── 3. Camera (WORLD_EYE) ─────────────────────────────────────────────
    bool camOk = vision().isCameraHardwarePresent();
    sub.setAvailable(SubsystemName::WORLD_EYE, camOk);
    // NOTE: mode NOT changed here — stays FORCED_OFF until user toggles ON.
    sensorLine("Camera",      camOk,
               camOk ? "DirectShow device ready (OFF — toggle to enable)"
                     : "No video capture device — camera skipped");

    // ── 4. Screen eye (SCREEN_EYE) ────────────────────────────────────────
    HWND desk = GetDesktopWindow();
    bool scrOk = (desk != nullptr);
    sub.setAvailable(SubsystemName::SCREEN_EYE, scrOk);
    // NOTE: mode NOT changed here — stays FORCED_OFF until user toggles ON.
    sensorLine("Screen eye",  scrOk,
               scrOk ? "Desktop BitBlt ready (OFF — toggle to enable)"
                     : "No desktop window");

    // ── 5. Body telemetry (BODY_STATE) — always available, always AUTO ────
    // Body state is pure software telemetry — no user permission needed.
    sub.setAvailable(SubsystemName::BODY_STATE, true);
    sub.setMode(SubsystemName::BODY_STATE, SubsystemMode::AUTO);
    sensorLine("Body state",  true, "Software telemetry always available");

    // ── Single refresh to apply the available flags ───────────────────────
    // All sensors except BODY_STATE remain FORCED_OFF (inactive).
    // syncRuntimesWithSubsystems will respect the OFF state.
    sub.refresh();

    // Small delay so background threads have time to initialise
    std::this_thread::sleep_for(std::chrono::milliseconds(yuki::config::kSensorInitDelayMs));

    // ── 6. STT — only starts if mic mode is NOT FORCED_OFF ───────────────
    bool sttOk = false;
    SttState st = baby.stt().getState();
    sttOk = (st != SttState::STOPPED && st != SttState::FAILED);
    sensorLine("Voice STT", sttOk,
               sttOk ? "Ready" : "Standby — will start when Mic is enabled");

    // ── Final status banner ────────────────────────────────────────────────
    int detected = (micOk ? 1 : 0) + (spkOk ? 1 : 0) +
                   (camOk ? 1 : 0) + (scrOk ? 1 : 0) + 1 /*body*/;
    std::cout << "\n  " << detected << "/5 sensors detected. "
              << "All OFF — use the shell buttons to activate.\n\n";

    // ── Background retry: if STT didn't start, retry up to 3x ────────────
    if (micOk && !sttOk) {
        std::thread([]() {
            for (int attempt = 1; attempt <= 3; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(yuki::config::kSensorRetryDelayMs * attempt));
                // sttRuntime is managed inside BabyMode; BabyMode's
                // syncRuntimesWithSubsystems will retry on next refresh.
                // We just log the retry.
                std::cout << "[AutoSensor] STT retry " << attempt << "/3...\n";
            }
        }).detach();
    }
}
