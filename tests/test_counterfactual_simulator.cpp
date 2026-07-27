#include "brain/causal/CounterfactualSimulator.h"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    using namespace yuki::causal;

    std::cout << "[TEST] CounterfactualSimulator starting..." << std::endl;

    auto scm = std::make_shared<StructuralCausalModel>();
    VariableId idX = scm->addLinearVariable("X", {}, {}, 0.0, 1.0);
    VariableId idY = scm->addLinearVariable("Y", {idX}, {2.0}, 0.0, 1.0);

    CounterfactualSimulator sim;
    sim.setModel(scm);

    Evidence ev;
    ev.observations[idX] = 2.0;
    ev.observations[idY] = 5.0; // U_Y = 5.0 - 2.0*2.0 = 1.0

    CounterfactualQuery q;
    q.targetY = idY;
    q.interventionX = idX;
    q.interventionValue = 4.0; // What if X had been 4.0?
    q.evidence = ev;

    auto res = sim.simulate(q);
    assert(res.valid);
    assert(std::abs(res.factualY - 5.0) < 1e-6);
    assert(std::abs(res.predictedY - (2.0 * 4.0 + 1.0)) < 1e-6); // 9.0
    assert(std::abs(res.effect - 4.0) < 1e-6); // 9.0 - 5.0

    // Test ATE
    double ate = sim.computeATE(idX, idY, 1.0, 0.0, 100);
    assert(std::abs(ate - 2.0) < 1e-1);

    // Test serialization
    auto bytes = sim.serialize();
    assert(!bytes.empty());

    CounterfactualSimulator sim2;
    bool ok = sim2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] CounterfactualSimulator PASSED!" << std::endl;
    return 0;
}
