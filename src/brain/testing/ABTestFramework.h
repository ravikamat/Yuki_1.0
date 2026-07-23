#ifndef YUKI_AB_TEST_FRAMEWORK_H
#define YUKI_AB_TEST_FRAMEWORK_H

#include <vector>

namespace yuki {
namespace testing {

struct ABTestResult {
    float pValue = 1.0f;
    float effectSize = 0.0f;
    bool  variantAWins = false;
    bool  variantBWins = false;
    bool  isSignificant = false;
};

class ABTestFramework {
public:
    ABTestResult compare(const std::vector<float>& variantA,
                         const std::vector<float>& variantB,
                         float alpha = 0.05f);
};

} // namespace testing
} // namespace yuki

#endif
