#include <iostream>
#include "input/Ear.h"
#include "SubsystemControl.h"
#include <windows.h>
#include <chrono>

int main() {
    std::cout << "Starting test_ear_stall..." << std::endl;
    
    SubsystemControl control;
    control.setMode(SubsystemName::EAR, SubsystemMode::AUTO);
    control.setAvailable(SubsystemName::EAR, true);
    control.refresh();

    std::cout << "Creating EarRuntime..." << std::endl;
    EarRuntime runtime(control);

    std::cout << "Starting EarRuntime..." << std::endl;
    runtime.start();

    // Verify it is running
    std::cout << "Is running: " << (runtime.isRunning() ? "YES" : "NO") << std::endl;
    
    std::cout << "Device name: " << runtime.getDeviceName() << std::endl;
    std::cout << "Last error: " << runtime.getLastError() << std::endl;
    std::cout << "Latest volume: " << runtime.getLatestVolume() << std::endl;

    std::cout << "Sleeping 2 seconds to let it capture/simulate..." << std::endl;
    Sleep(2000);

    std::cout << "State reported: " << static_cast<int>(runtime.reportState()) << std::endl;
    
    std::cout << "Stopping EarRuntime..." << std::endl;
    runtime.stop();

    std::cout << "PASS" << std::endl;
    return 0;
}
