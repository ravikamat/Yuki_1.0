#include <cassert>
#include <cstdio>
#include <vector>
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/ync/YNCCheckpoint.h"

using namespace ync;

int main() {
    const char* ckpt_path = "test_scale_recovery.ynck";

    NeuromorphicSimulator sim1;
    SimulatorConfig cfg{1000, 4, 1, 128, 16, 0.05f};
    sim1.initialize(cfg, 42);
    sim1.start();
    sim1.runFor(100);
    sim1.stop();

    // Save checkpoint
    sim1.saveCheckpoint(ckpt_path);

    // Save original state details for comparison
    uint64_t orig_time = sim1.global_time.load();
    float orig_dopamine = sim1.nms.dopamine.load();

    // Load into clean simulator
    NeuromorphicSimulator sim2;
    sim2.initialize(cfg, 999);
    bool loaded = sim2.loadCheckpoint(ckpt_path);
    assert(loaded);
    assert(sim2.global_time.load() == orig_time);
    assert(sim2.nms.dopamine.load() == orig_dopamine);

    // Clean up file
    std::remove(ckpt_path);

    std::puts("=== test_ync_checkpoint_recovery: ALL PASS ===");
    return 0;
}
