#include "brain/testing/SmartTestSelector.h"
#include <cassert>

int main() {
    yuki::testing::SmartTestSelector selector;
    yuki::testing::TestSuiteDAG dag;

    for (uint64_t i = 1; i <= 8; ++i) {
        yuki::testing::TestNode n;
        n.testId = i;
        dag.nodes.push_back(n);
    }

    auto selected = selector.selectTests(dag, yuki::testing::SelectionTier::QUICK_SCREEN);
    assert(selected.size() == 2);

    return 0;
}
