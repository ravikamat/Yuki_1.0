#include <iostream>
#include <cassert>
#include "brain/core/SystemWarmUp.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/predictive/predictive_turn_engine.h"

using namespace yuki;
using namespace yuki::core;

int main() {
    std::cout << "[TEST] Running test_system_warmup..." << std::endl;

    auto user = std::make_shared<UserModel>();
    TurnCoordinator coordinator(user);
    memory::MemoryFabric memory;
    research::ToolRegistry registry;
    security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();

    SystemWarmUp warmUp;
    assert(!warmUp.isWarmed());

    // 1. First execution
    auto metrics1 = warmUp.execute(&coordinator, &memory, &registry, &sandbox);
    assert(metrics1.success);
    assert(warmUp.isWarmed());
    std::cout << "  First warm-up duration: " << metrics1.total_duration.count() << " ms" << std::endl;

    // 2. Idempotent second execution
    auto metrics2 = warmUp.execute(&coordinator, &memory, &registry, &sandbox);
    assert(metrics2.total_duration.count() == 0);
    assert(warmUp.isWarmed());
    std::cout << "  Second warm-up duration: " << metrics2.total_duration.count() << " ms (idempotent)" << std::endl;

    std::cout << "[TEST] test_system_warmup PASSED." << std::endl;
    return 0;
}
