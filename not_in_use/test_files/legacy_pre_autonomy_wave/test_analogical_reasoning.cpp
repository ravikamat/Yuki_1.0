#include "brain/reasoning/AnalogicalReasoning.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::reasoning;

    std::cout << "[TEST] AnalogicalReasoning starting..." << std::endl;

    AnalogicalReasoning ar;

    // Source domain: Water pipe system (pipe -> flow -> pressure)
    Domain source;
    source.name = "WaterSystem";
    source.entities = {{0, "Pipe", {}}, {1, "Water", {}}};
    source.relations = {{0, "FLOWS", {1, 0}, 1.0, false}};

    // Target domain: Electrical circuit (wire -> current -> voltage)
    Domain target;
    target.name = "ElectricalCircuit";
    target.entities = {{0, "Wire", {}}, {1, "Current", {}}};
    target.relations = {{0, "FLOWS", {1, 0}, 1.0, false}};

    Mapping map = ar.findAnalogy(source, target);
    assert(map.score > 0.0);
    assert(map.entityMap.size() > 0);

    double sim = ar.computeSimilarity(source, target);
    assert(sim > 0.0);

    Domain target2 = target;
    TransferResult transferRes = ar.transfer(source, map, target2);
    assert(transferRes.confidence >= 0.0);

    // Test serialization
    auto bytes = ar.serialize();
    assert(!bytes.empty());

    AnalogicalReasoning ar2;
    bool ok = ar2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] AnalogicalReasoning PASSED!" << std::endl;
    return 0;
}
