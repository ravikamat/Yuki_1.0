#include <cassert>
#include <cstdio>
#include "brain/logic/PropositionalEngine.h"

using namespace yuki::logic;

int main() {
    PropositionalEngine engine;

    // Test 1: Simple SAT problem: (A OR B) AND (!A OR B)
    {
        CNF cnf;
        cnf.addClause(Clause{{Literal{0, false}, Literal{1, false}}});
        cnf.addClause(Clause{{Literal{0, true}, Literal{1, false}}});
        auto sol = engine.solve(cnf);
        assert(sol.result == PropositionalEngine::Result::SAT);
        assert(sol.assignment[1] == true); // B must be true
    }

    // Test 2: Simple UNSAT problem: (A) AND (!A)
    {
        CNF cnf;
        cnf.addClause(Clause{{Literal{0, false}}});
        cnf.addClause(Clause{{Literal{0, true}}});
        auto sol = engine.solve(cnf);
        assert(sol.result == PropositionalEngine::Result::UNSAT);
    }

    // Test 3: String fact consistency
    {
        std::vector<std::string> facts = {"factA", "!factB", "factC"};
        assert(engine.isConsistent(facts));
        std::vector<std::string> inconsistent = {"factA", "!factA"};
        assert(!engine.isConsistent(inconsistent));
    }

    // Test 4: Resolution proof: KB = {A -> B, A}, Prove B
    {
        CNF kb;
        kb.addClause(Clause{{Literal{0, true}, Literal{1, false}}}); // !A OR B
        kb.addClause(Clause{{Literal{0, false}}});                  // A
        Clause negated_goal{{Literal{1, true}}};                   // !B
        assert(engine.proveByResolution(kb, negated_goal));
    }

    std::puts("=== test_propositional_engine: ALL PASS ===");
    return 0;
}
