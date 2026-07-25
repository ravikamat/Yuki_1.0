#include "brain/core/SystemBenchmark.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::core;

    std::cout << "[TEST] SystemBenchmark starting..." << std::endl;

    SystemBenchmark bench;

    int counter = 0;
    bench.registerSubsystem("CoreLoop",
        [&]() { counter = 0; },
        [&]() { counter++; },
        [&]() { assert(counter > 0); }
    );

    bench.setBaseline("CoreLoop", 1.0, 1000.0, 2.0);

    auto result = bench.runSubsystemBenchmark("CoreLoop", 50);
    assert(result.subsystem == "CoreLoop");
    assert(result.latencyMs >= 0.0);
    assert(result.throughputOpsPerSec >= 0.0);

    auto report = bench.runFullBenchmark(20);
    assert(report.results.size() == 1);

    // Test saving / loading baselines
    bool saved = bench.saveBaselines("data/brain/test_baselines.txt");
    assert(saved);

    bool loaded = bench.loadBaselines("data/brain/test_baselines.txt");
    assert(loaded);

    // Test serialization
    auto bytes = bench.serialize();
    assert(!bytes.empty());

    SystemBenchmark bench2;
    bool ok = bench2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] SystemBenchmark PASSED!" << std::endl;
    return 0;
}
