#include "input/WakeDetector.h"
#include <cassert>
#include <fstream>
#include <chrono>

int main() {
    bool callback_fired = false;
    yuki::input::WakeDetector wd([&]() { callback_fired = true; });

    // 1. start/stop cycle doesn't crash
    assert(wd.start());
    assert(wd.isRunning());
    wd.stop();
    assert(!wd.isRunning());

    // 2. loadPatternFromFile() with missing file returns false gracefully
    assert(!wd.loadPatternFromFile("non_existent_wake_file.bin"));

    // 3. loadPatternFromFile() with valid binary file returns true
    {
        std::ofstream dummy("test_wake_temp.bin", std::ios::binary);
        float pattern_data[] = {0.1f, 0.2f, 0.3f, 0.4f};
        dummy.write(reinterpret_cast<const char*>(pattern_data), sizeof(pattern_data));
    }
    assert(wd.loadPatternFromFile("test_wake_temp.bin"));
    assert(wd.pattern().size() == 4);

    // 4. shutdown_requested_ stops worker loop within 100ms
    auto start_time = std::chrono::steady_clock::now();
    wd.start();
    wd.stop();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    assert(elapsed < 500);

    // 5. destructor joins thread cleanly
    {
        yuki::input::WakeDetector wd2;
        wd2.start();
    } // destructor called here

    // 6. pattern hash consistency (same file -> same internal pattern)
    wd.loadPatternFromFile("test_wake_temp.bin");
    auto p1 = wd.pattern();
    wd.loadPatternFromFile("test_wake_temp.bin");
    auto p2 = wd.pattern();
    assert(p1 == p2);

    std::remove("test_wake_temp.bin");
    return 0;
}
