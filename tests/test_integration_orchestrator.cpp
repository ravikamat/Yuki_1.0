#include "brain/core/IntegrationOrchestrator.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::core;

    std::cout << "[TEST] IntegrationOrchestrator starting..." << std::endl;

    IntegrationOrchestrator orchestrator;
    assert(orchestrator.getModuleCount() == 0);

    orchestrator.registerModule("A", {}, []() {
        ModuleStatus s; s.name = "A"; s.health = ModuleHealth::OK; return s;
    });

    orchestrator.registerModule("B", {"A"}, []() {
        ModuleStatus s; s.name = "B"; s.health = ModuleHealth::OK; return s;
    });

    assert(orchestrator.getModuleCount() == 2);
    assert(orchestrator.validateCoherence());
    assert(!orchestrator.hasCycles());

    auto report = orchestrator.getSystemHealth();
    assert(report.okCount == 2);
    assert(report.overallScore == 1.0);

    // Test cycle detection
    orchestrator.registerModule("C", {"B"}, nullptr);
    orchestrator.registerModule("A", {"C"}, nullptr); // Create A -> B -> C -> A cycle

    assert(orchestrator.hasCycles());
    auto cycles = orchestrator.detectCycles();
    assert(!cycles.empty());

    // Test serialization
    auto bytes = orchestrator.serialize();
    assert(!bytes.empty());

    IntegrationOrchestrator orch2;
    bool ok = orch2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] IntegrationOrchestrator PASSED!" << std::endl;
    return 0;
}
