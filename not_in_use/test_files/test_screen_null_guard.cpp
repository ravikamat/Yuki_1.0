#include <iostream>
#include "input/VisionSystem.h"
#include "SubsystemControl.h"
#include <windows.h>

int main() {
    std::cout << "Starting test_screen_null_guard..." << std::endl;
    
    // Create dummy control
    SubsystemControl control;
    // Set screen eye to active/available so capture proceeds
    control.setMode(SubsystemName::SCREEN_EYE, SubsystemMode::AUTO);
    control.setAvailable(SubsystemName::SCREEN_EYE, true);
    control.refresh();

    ScreenEyeReader reader;
    std::cout << "Calling reader.capture()..." << std::endl;
    ScreenSnapshot snap = reader.capture(control);

    std::cout << "Capture returned successfully!" << std::endl;
    std::cout << "Snapshot allowed: " << snap.allowed << std::endl;
    std::cout << "Snapshot active: " << snap.subsystem_active << std::endl;
    std::cout << "Foreground window present: " << snap.foreground_window_present << std::endl;
    std::cout << "Summary: " << snap.summary << std::endl;

    // The test passes if it executes without crashing or throwing
    std::cout << "PASS" << std::endl;
    return 0;
}
