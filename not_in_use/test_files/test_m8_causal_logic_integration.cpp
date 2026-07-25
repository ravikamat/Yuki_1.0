#include <cassert>
#include <cstdio>
#include "brain/logic/PropositionalEngine.h"
#include "brain/causality/CausalGraph.h"

using namespace yuki::logic;
using namespace yuki::causality;

int main() {
    // Integration test: Propositional logic validates causal graph invariants
    CausalGraph cg;
    cg.addNode("Rain");       // 0
    cg.addNode("Sprinkler");  // 1
    cg.addNode("WetGrass");   // 2

    cg.addEdge(0, 2); // Rain -> WetGrass
    cg.addEdge(1, 2); // Sprinkler -> WetGrass

    // Logic KB: WetGrass <-> Rain OR Sprinkler
    // In CNF: (!WetGrass OR Rain OR Sprinkler) AND (!Rain OR WetGrass) AND (!Sprinkler OR WetGrass)
    PropositionalEngine engine;
    CNF kb;
    kb.addClause(Clause{{Literal{2, true}, Literal{0, false}, Literal{1, false}}});
    kb.addClause(Clause{{Literal{0, true}, Literal{2, false}}});
    kb.addClause(Clause{{Literal{1, true}, Literal{2, false}}});

    // Check consistency when Rain is true
    kb.addClause(Clause{{Literal{0, false}}});
    auto sol = engine.solve(kb);
    assert(sol.result == PropositionalEngine::Result::SAT);
    assert(sol.assignment[2] == true); // WetGrass must be true

    // Check d-separation on CausalGraph: Rain and Sprinkler independent given empty Z (if independent causes)
    assert(cg.dSeparated(0, 1, {}));

    std::puts("=== test_m8_causal_logic_integration: ALL PASS ===");
    return 0;
}
