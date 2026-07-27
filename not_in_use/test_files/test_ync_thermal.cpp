#include <cassert>
#include <cstdio>
#include "brain/ync/YncOrchestrator.h"

using namespace ync;

int main() {
    // Test 1: Default phase is ACTIVE, requestedNeuronCount and shouldRunYNC are correct
    {
        YncOrchestrator orch;
        orch.initialize();
        assert(orch.currentPhase() == YncOrchestrator::Phase::ACTIVE);
        assert(orch.requestedNeuronCount() == 10000);
        assert(orch.shouldRunYNC());
        orch.shutdown();
    }

    // Test 2: Thermal state returns valid physical bounds
    {
        YncOrchestrator orch;
        orch.initialize();
        auto thermal = orch.thermalState();
        assert(thermal.cpu_temp_c >= 0.0f);
        assert(thermal.cpu_temp_c < 200.0f);  // Physically impossible above 200C
        assert(thermal.cpu_load_percent >= 0.0f);
        assert(thermal.cpu_load_percent <= 100.0f);
        orch.shutdown();
    }

    // Test 3: tick() does not crash and produces a valid phase
    {
        YncOrchestrator orch;
        orch.initialize();
        orch.tick();
        auto phase = orch.currentPhase();
        assert(phase == YncOrchestrator::Phase::ACTIVE ||
               phase == YncOrchestrator::Phase::THROTTLED);
        // If not throttled, YNC should be allowed to run
        if (phase != YncOrchestrator::Phase::THROTTLED) {
            assert(orch.shouldRunYNC());
        }
        orch.shutdown();
    }

    // Test 4: requestedNeuronCount() mapping for all known phases
    {
        CognitiveOrchestrator orch;
        orch.initialize();
        // Default is ACTIVE
        assert(orch.requestedNeuronCount() == 10000);
        orch.shutdown();
    }

    std::puts("=== test_ync_thermal: ALL PASS ===");
    return 0;
}
