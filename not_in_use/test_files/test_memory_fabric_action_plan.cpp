#include <iostream>
#include <cassert>
#include "brain/action/core/ActionPlan.h"
#include "brain/memory/MemoryFabric.h"

using namespace yuki;
using namespace yuki::action;
using namespace yuki::memory;

int main() {
    std::cout << "[TEST] Running test_memory_fabric_action_plan..." << std::endl;

    // 1. Create ActionPlan with nodes & dependencies
    ActionPlan original;
    original.planId = 101;
    original.parentRequestId = 5001;
    original.rollbackBudget = 3;
    original.aggregateRiskScore = 0.15f;
    original.checkpointIds = {901, 902};

    ActionNode node1;
    node1.nodeId = 1;
    node1.type = ActionType::FILE_CREATE;
    node1.status = ActionStatus::SUCCESS;
    node1.executed = true;
    node1.confidenceThreshold = 0.8f;
    node1.maxRetries = 2;
    original.nodes.push_back(node1);

    ActionNode node2;
    node2.nodeId = 2;
    node2.type = ActionType::COMPILE;
    node2.status = ActionStatus::PENDING;
    node2.executed = false;
    node2.inputDeps = {1};
    node2.confidenceThreshold = 0.7f;
    node2.maxRetries = 1;
    original.nodes.push_back(node2);

    original.buildWaves();

    // 2. Serialize and Deserialize directly
    auto bytes = original.serialize();
    assert(!bytes.empty());
    std::cout << "  Serialized ActionPlan size: " << bytes.size() << " bytes" << std::endl;

    auto restoredOpt = ActionPlan::deserialize(bytes);
    assert(restoredOpt.has_value());

    const auto& restored = restoredOpt.value();
    assert(restored.planId == original.planId);
    assert(restored.parentRequestId == original.parentRequestId);
    assert(restored.rollbackBudget == original.rollbackBudget);
    assert(restored.nodes.size() == original.nodes.size());
    assert(restored.nodes[0].nodeId == 1);
    assert(restored.nodes[1].inputDeps == std::vector<uint64_t>{1});

    // 3. Test corruption detection (flip a byte)
    auto corruptBytes = bytes;
    corruptBytes[corruptBytes.size() / 2] ^= 0xFF;
    auto corruptOpt = ActionPlan::deserialize(corruptBytes);
    assert(!corruptOpt.has_value());
    std::cout << "  Corruption detection (FNV-1a checksum) verified." << std::endl;

    // 4. MemoryFabric Store and Retrieve
    MemoryFabric fabric;
    fabric.storeActionPlan(original, MemoryTier::T1_EPISODIC);

    auto retrievedPlans = fabric.retrieveActionPlans("action_plan_101", RetrieveMode::FUZZY, 0.5f);
    assert(!retrievedPlans.empty());
    assert(retrievedPlans[0].planId == 101);
    std::cout << "  MemoryFabric T1 store & retrieve verified." << std::endl;

    std::cout << "[TEST] test_memory_fabric_action_plan PASSED." << std::endl;
    return 0;
}
