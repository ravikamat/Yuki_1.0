#include "brain/causal/StructuralCausalModel.h"
#include "brain/causal/CounterfactualSimulator.h"
#include "brain/reasoning/AnalogicalReasoning.h"
#include "brain/language/MetaphorEngine.h"

#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::causal;
    using namespace yuki::reasoning;
    using namespace yuki::language;

    std::cout << "[TEST] M11 Full Integration starting..." << std::endl;

    // 1. SCM
    auto scm = std::make_shared<StructuralCausalModel>();
    VariableId idX = scm->addLinearVariable("X", {}, {}, 0.0, 1.0);
    VariableId idY = scm->addLinearVariable("Y", {idX}, {2.0}, 0.0, 1.0);

    // 2. CounterfactualSimulator
    CounterfactualSimulator sim;
    sim.setModel(scm);
    Evidence ev;
    ev.observations[idX] = 1.0;
    ev.observations[idY] = 2.0;

    CounterfactualQuery q;
    q.targetY = idY;
    q.interventionX = idX;
    q.interventionValue = 3.0;
    q.evidence = ev;

    auto res = sim.simulate(q);
    assert(res.valid);

    // 3. AnalogicalReasoning
    AnalogicalReasoning ar;
    Domain d1{"Source", {{0, "A", {}}}, {{0, "REL", {0}, 1.0, false}}};
    Domain d2{"Target", {{0, "B", {}}}, {{0, "REL", {0}, 1.0, false}}};
    Mapping map = ar.findAnalogy(d1, d2);
    assert(map.score > 0.0);

    // 4. MetaphorEngine
    MetaphorEngine met;
    met.setAnalogicalReasoning(&ar);
    auto metaphor = met.generateMetaphor("Logic", "Architecture");
    assert(!metaphor.expression.empty());

    std::cout << "[TEST] M11 Full Integration PASSED!" << std::endl;
    return 0;
}
