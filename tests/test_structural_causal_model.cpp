#include "brain/causal/StructuralCausalModel.h"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    using namespace yuki::causal;

    std::cout << "[TEST] StructuralCausalModel starting..." << std::endl;

    StructuralCausalModel scm;

    // Build model X -> Y -> Z
    // X = U_X
    // Y = 2.0 * X + U_Y
    // Z = 3.0 * Y + U_Z

    VariableId idX = scm.addLinearVariable("X", {}, {}, 0.0, 1.0);
    VariableId idY = scm.addLinearVariable("Y", {idX}, {2.0}, 0.0, 1.0);
    VariableId idZ = scm.addLinearVariable("Z", {idY}, {3.0}, 0.0, 1.0);

    assert(scm.getVariableCount() == 3);
    assert(scm.isAcyclic());

    auto order = scm.getTopologicalOrder();
    assert(order.size() == 3);
    assert(order[0] == idX);

    // Solve with noise U = [1.0, 0.5, 0.2]
    std::vector<Value> noise = {1.0, 0.5, 0.2};
    auto sol = scm.solve(noise);

    assert(std::abs(sol[idX] - 1.0) < 1e-6);
    assert(std::abs(sol[idY] - (2.0 * 1.0 + 0.5)) < 1e-6); // 2.5
    assert(std::abs(sol[idZ] - (3.0 * 2.5 + 0.2)) < 1e-6); // 7.7

    // Intervene do(Y = 10.0)
    Intervention in{idY, 10.0};
    auto solIn = scm.interveneAndSolve(noise, in);
    assert(std::abs(solIn[idX] - 1.0) < 1e-6);
    assert(std::abs(solIn[idY] - 10.0) < 1e-6);
    assert(std::abs(solIn[idZ] - (3.0 * 10.0 + 0.2)) < 1e-6); // 30.2

    // Test serialization
    auto bytes = scm.serialize();
    assert(!bytes.empty());

    StructuralCausalModel scm2;
    bool ok = scm2.deserialize(bytes);
    assert(ok);
    assert(scm2.getVariableCount() == 3);

    std::cout << "[TEST] StructuralCausalModel PASSED!" << std::endl;
    return 0;
}
