#ifndef YUKI_SMART_TEST_SELECTOR_H
#define YUKI_SMART_TEST_SELECTOR_H

#include "brain/testing/TestSuiteDAG.h"
#include <vector>

namespace yuki {
namespace testing {

enum class SelectionTier : uint8_t {
    QUICK_SCREEN = 0,
    MEDIUM_REGRESSION,
    FULL_SUITE
};

class SmartTestSelector {
public:
    std::vector<uint64_t> selectTests(const TestSuiteDAG& dag, SelectionTier tier = SelectionTier::QUICK_SCREEN);
};

} // namespace testing
} // namespace yuki

#endif
