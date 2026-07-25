#include <cassert>
#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
#include "brain/ync/NeuromorphicSimulator.h"

using namespace ync;

int main() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg{500, 4, 1, 64, 16, 0.05f};
    sim.initialize(cfg, 777);
    sim.start();

    std::atomic<bool> active{true};

    // Parallel thread continuously bombarding neuromodulators while sim runs steps
    std::thread fuzz_thread([&]() {
        while (active.load()) {
            sim.deliverReward(0.1f);
            sim.deliverPunishment(0.05f);
            sim.deliverSurprise(0.2f);
            std::this_thread::yield();
        }
    });

    sim.runFor(500);

    active.store(false);
    fuzz_thread.join();
    sim.stop();

    std::puts("=== test_ync_concurrency_fuzz: ALL PASS ===");
    return 0;
}
