#ifndef YUKI_TEST_SUITE_DAG_H
#define YUKI_TEST_SUITE_DAG_H

#include <cstdint>
#include <vector>

namespace yuki {
namespace testing {

struct TestNode {
    uint64_t              testId = 0;
    std::vector<uint64_t> dependencies;
    bool                  executed = false;
    bool                  passed = false;
};

class TestSuiteDAG {
public:
    uint64_t              suiteId = 0;
    std::vector<TestNode> nodes;
    std::vector<std::vector<uint64_t>> waves;

    void buildWaves();
    bool isComplete() const;
};

} // namespace testing
} // namespace yuki

#endif
