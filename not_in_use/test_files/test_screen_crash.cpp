#include <iostream>
#include "input/ScreenRuntime.h"
#include "SubsystemControl.h"

int main() {
    std::cout << "Starting test..." << std::endl;
    SubsystemControl control;
    control.setMode(SubsystemName::SCREEN_EYE, SubsystemMode::FORCED_ON);
    
    ScreenRuntime runtime(control);
    std::cout << "Calling start()..." << std::endl;
    runtime.start();
    
    std::cout << "Waiting 2 seconds..." << std::endl;
    Sleep(2000);
    
    std::cout << "Calling stop()..." << std::endl;
    runtime.stop();
    
    std::cout << "Test completed." << std::endl;
    return 0;
}
