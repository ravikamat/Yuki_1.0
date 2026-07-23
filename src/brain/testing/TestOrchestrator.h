#ifndef YUKI_TEST_ORCHESTRATOR_H
#define YUKI_TEST_ORCHESTRATOR_H

#include "brain/testing/TestSuiteDAG.h"
#include "brain/testing/TestResultPack.h"
#include "brain/testing/SmartTestSelector.h"
#include <memory>

namespace yuki {
namespace testing {

class TestOrchestrator {
public:
    TestSuiteDAG buildSuite(const std::vector<uint64_t>& testIds);
    TestResultPack runSuite(const TestSuiteDAG& dag);
};

} // namespace testing
} // namespace yuki

#endif
