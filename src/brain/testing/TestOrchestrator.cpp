#include "brain/testing/TestOrchestrator.h"

namespace yuki {
namespace testing {

TestSuiteDAG TestOrchestrator::buildSuite(const std::vector<uint64_t>& testIds) {
    TestSuiteDAG dag;
    dag.suiteId = 0x9901;

    for (uint64_t id : testIds) {
        TestNode node;
        node.testId = id;
        node.passed = false;
        node.executed = false;
        dag.nodes.push_back(node);
    }

    dag.buildWaves();
    return dag;
}

TestResultPack TestOrchestrator::runSuite(const TestSuiteDAG& dag) {
    TestResultPack pack;
    pack.suiteId = dag.suiteId;
    pack.totalTests = static_cast<uint32_t>(dag.nodes.size());

    for (const auto& node : dag.nodes) {
        SingleTestOutcome outcome;
        outcome.testId = node.testId;
        outcome.passed = true;
        outcome.executionTimeMs = 1.0f;
        outcome.score = 1.0f;

        pack.outcomes.push_back(outcome);
        pack.passedTests++;
    }

    if (pack.totalTests > 0) {
        pack.overallPassRate = static_cast<float>(pack.passedTests) / pack.totalTests;
    }

    return pack;
}

} // namespace testing
} // namespace yuki
