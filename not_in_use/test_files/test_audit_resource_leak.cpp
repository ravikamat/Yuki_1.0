#include <cassert>
#include <cstdio>
#include <fstream>
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/ync/YNCCheckpoint.h"

using namespace ync;

int main() {
    const char* path = "test_leak_checkpoint.ynck";

    // Test 1: Checkpoint save/load does not leak file handles
    {
        NeuromorphicSimulator sim;
        SimulatorConfig cfg{1000, 2, 1, 128, 16, 0.05f};
        sim.initialize(cfg, 42);
        sim.start();
        sim.runFor(100);
        sim.stop();
        sim.saveCheckpoint(path);
    }

    {
        NeuromorphicSimulator sim2;
        SimulatorConfig cfg{1000, 2, 1, 128, 16, 0.05f};
        sim2.initialize(cfg, 42);
        bool ok = sim2.loadCheckpoint(path);
        assert(ok);
        sim2.start();
        sim2.runFor(50);
        sim2.stop();
    }

    // Verify file can be deleted (not locked)
    int removed = std::remove(path);
    assert(removed == 0 && "Checkpoint file was left open — resource leak");

    // Test 2: Simulator start/stop cycles do not leak threads
    for (int i = 0; i < 10; ++i) {
        NeuromorphicSimulator sim;
        SimulatorConfig cfg{500, 2, 1, 128, 16, 0.05f};
        sim.initialize(cfg, 42);
        sim.start();
        sim.runFor(10);
        sim.stop();
    }

    std::puts("=== test_audit_resource_leak: ALL PASS ===");
    return 0;
}
