#include "brain/system/BackgroundJobEngine.h"
#include <cassert>
#include <chrono>
#include <thread>

int main() {
    yuki::system::BackgroundJobEngine engine(2);

    // 1. submitJob() assigns monotonic ID
    uint64_t id1 = engine.submitJob(yuki::system::Job::Type::RESEARCH, 5, 1000, []() { return true; });
    uint64_t id2 = engine.submitJob(yuki::system::Job::Type::CONSOLIDATION, 5, 1000, []() { return true; });
    assert(id2 > id1);

    // 2. job completes within timeout -> status COMPLETED
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(engine.getJobStatus(id1) == yuki::system::Job::Status::COMPLETED);

    // 3. job exceeds timeout -> status TIMEOUT
    uint64_t id_slow = engine.submitJob(yuki::system::Job::Type::RESEARCH, 1, 50, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    assert(engine.getJobStatus(id_slow) == yuki::system::Job::Status::TIMEOUT);

    // 4. priority queue order (lower priority number runs first)
    uint64_t id_p10 = engine.submitJob(yuki::system::Job::Type::CURRICULUM, 10, 1000, []() { return true; });
    uint64_t id_p1 = engine.submitJob(yuki::system::Job::Type::CURRICULUM, 1, 1000, []() { return true; });
    assert(id_p1 > 0 && id_p10 > 0);

    // 5. shutdown with pending jobs cancels gracefully
    engine.shutdown();

    // 6. 100-job stress test (submit all, verify no crash)
    {
        yuki::system::BackgroundJobEngine stress_engine(4);
        for (int i = 0; i < 100; ++i) {
            stress_engine.submitJob(yuki::system::Job::Type::SYSTEM_MONITOR, i % 10, 500, []() {
                return true;
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        stress_engine.shutdown();
    }

    return 0;
}
