#include "brain/testing/TestOrchestrator.h"
#include <cassert>

int main() {
    yuki::testing::TestOrchestrator orch;
    std::vector<uint64_t> ids = {1, 2, 3};

    auto dag = orch.buildSuite(ids);
    assert(dag.nodes.size() == 3);

    auto result = orch.runSuite(dag);
    assert(result.isAllPassing());

    return 0;
}
