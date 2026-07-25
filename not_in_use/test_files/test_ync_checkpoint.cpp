// test_ync_checkpoint.cpp -- spec-compliant checkpoint tests
#include "brain/ync/YNCCheckpoint.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

using namespace ync;

static std::string tmpPath() {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "test_checkpoint_%lld.ynck",
                  (long long)std::chrono::steady_clock::now().time_since_epoch().count());
    return std::string(buf);
}

static void test_save_creates_file() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg; cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    std::string path = tmpPath();
    bool ok = YNCCheckpoint::save(sim, path);
    assert(ok && "save must succeed");
    // Verify file exists
    FILE* f = std::fopen(path.c_str(), "rb");
    assert(f && "checkpoint file must be created");
    std::fclose(f);
    std::remove(path.c_str());
    std::puts("test_save_creates_file PASS");
}

static void test_load_restores_time() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg; cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    sim.start();
    sim.runFor(10);
    sim.stop();
    uint64_t t_saved = sim.global_time.load(std::memory_order_relaxed);
    std::string path = tmpPath();
    YNCCheckpoint::save(sim, path);

    // Reset and reload
    NeuromorphicSimulator sim2;
    sim2.initialize(cfg, 42);
    bool ok = YNCCheckpoint::load(sim2, path);
    assert(ok && "load must succeed");
    uint64_t t_loaded = sim2.global_time.load(std::memory_order_relaxed);
    assert(t_loaded == t_saved && "loaded time must match saved");
    std::remove(path.c_str());
    std::puts("test_load_restores_time PASS");
}

static void test_load_bad_path_returns_false() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg; cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    bool ok = YNCCheckpoint::load(sim, "/nonexistent/path/xyz.ynck");
    assert(!ok && "load from bad path must return false");
    std::puts("test_load_bad_path_returns_false PASS");
}

static void test_list_checkpoints_empty_dir() {
    auto results = YNCCheckpoint::listCheckpoints("d:/nonexistent_ync_dir");
    assert(results.empty() && "list must return empty for bad dir");
    std::puts("test_list_checkpoints_empty_dir PASS");
}

int main() {
    test_save_creates_file();
    test_load_restores_time();
    test_load_bad_path_returns_false();
    test_list_checkpoints_empty_dir();
    std::puts("=== test_ync_checkpoint: ALL PASS ===");
    return 0;
}
