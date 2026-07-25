#include "brain/core/SystemWarmUp.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/security/SecuritySandbox.h"
#include "input/encoding/TextEncoder.h"
#include <thread>
#include <future>
#include <vector>

using namespace yuki::core;

WarmUpMetrics SystemWarmUp::execute(
    TurnCoordinator* coordinator,
    memory::MemoryFabric* memory,
    research::ToolRegistry* tools,
    security::SecuritySandbox* sandbox
) {
    if (warmed_) {
        return {};
    }

    WarmUpMetrics metrics;
    auto t0 = std::chrono::steady_clock::now();

    // 1. Thread pools: spawn all workers eagerly to warm cache lines & OS thread allocators
    {
        auto t1 = std::chrono::steady_clock::now();
        warmThreadPools();
        metrics.thread_pool_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);
    }

    // 2. SQLite / MemoryFabric DB pre-warming
    {
        auto t1 = std::chrono::steady_clock::now();
        if (memory) warmSQLite(memory);
        metrics.sqlite_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);
    }

    // 3. Perception models: trigger dummy encoding to load regex caches & tables
    {
        auto t1 = std::chrono::steady_clock::now();
        if (coordinator) warmPerceptionModels(coordinator);
        metrics.model_cache_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1);
    }

    // 4. Security cache pre-normalization
    {
        if (sandbox) warmSecurityCache(sandbox);
    }

    metrics.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    metrics.success = true;
    warmed_ = true;
    return metrics;
}

void SystemWarmUp::warmThreadPools() {
    unsigned int n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;

    std::vector<std::future<void>> futures;
    futures.reserve(n);
    for (unsigned int i = 0; i < n; ++i) {
        futures.push_back(std::async(std::launch::async, []() {
            volatile int x = 0;
            (void)x;
        }));
    }
    for (auto& f : futures) f.wait();
}

void SystemWarmUp::warmSQLite(memory::MemoryFabric* memory) {
    if (memory) {
        memory->warmConnection();
    }
}

void SystemWarmUp::warmPerceptionModels(TurnCoordinator* coordinator) {
    if (coordinator) {
        perception::TextEncoder encoder;
        auto features = encoder.encode("system warmup phrase for regex precompilation");
        (void)features;
    }
}

void SystemWarmUp::warmSecurityCache(security::SecuritySandbox* sandbox) {
    if (sandbox) {
        sandbox->buildCache();
    }
}
