#ifndef YUKI_TEST_RESULT_PACK_H
#define YUKI_TEST_RESULT_PACK_H

#include <cstdint>
#include <vector>

namespace yuki {
namespace testing {

struct SingleTestOutcome {
    uint64_t testId = 0;
    bool     passed = false;
    float    executionTimeMs = 0.0f;
    float    score = 0.0f;
};

class TestResultPack {
public:
    uint64_t                     suiteId = 0;
    std::vector<SingleTestOutcome> outcomes;
    float                        overallPassRate = 0.0f;
    float                        avgExecutionTimeMs = 0.0f;
    uint32_t                     totalTests = 0;
    uint32_t                     passedTests = 0;
    uint32_t                     failedTests = 0;

    bool isAllPassing() const { return failedTests == 0 && totalTests > 0; }
};

} // namespace testing
} // namespace yuki

#endif
