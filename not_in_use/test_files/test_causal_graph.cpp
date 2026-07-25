#include <cassert>
#include <cstdio>
#include "brain/causality/CausalGraph.h"

using namespace yuki::causality;

int main() {
    // Build DAG: Confounder Z -> Treatment X, Confounder Z -> Outcome Y, Treatment X -> Outcome Y
    CausalGraph cg;
    cg.addNode("Z"); // 0
    cg.addNode("X"); // 1
    cg.addNode("Y"); // 2

    cg.addEdge(0, 1); // Z -> X
    cg.addEdge(0, 2); // Z -> Y
    cg.addEdge(1, 2); // X -> Y

    // Test 1: Without Z, X and Y are not d-separated
    assert(!cg.dSeparated(1, 2, {}));

    // Test 2: Given Z, the backdoor path X <- Z -> Y is blocked
    assert(cg.satisfiesBackdoor(1, 2, {0}));

    // Test 3: Empty set does NOT satisfy backdoor because of Z
    assert(!cg.satisfiesBackdoor(1, 2, {}));

    // Test 4: Adjustment set search finds Z
    auto adj = cg.findAdjustmentSet(1, 2);
    assert(adj.has_value());
    assert(adj->count(0));

    // Test 5: Intervention do(X=1) removes incoming edge Z -> X
    auto iv_graph = cg.intervene({1, true});
    assert(iv_graph.nodes[1].parents.empty());
    assert(iv_graph.satisfiesBackdoor(1, 2, {}));

    std::puts("=== test_causal_graph: ALL PASS ===");
    return 0;
}
