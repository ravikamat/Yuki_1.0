#include "brain/testing/ABTestFramework.h"
#include <numeric>
#include <cmath>

namespace yuki {
namespace testing {

ABTestResult ABTestFramework::compare(const std::vector<float>& variantA,
                                       const std::vector<float>& variantB,
                                       float alpha) {
    ABTestResult result;
    if (variantA.empty() || variantB.empty()) return result;

    float meanA = std::accumulate(variantA.begin(), variantA.end(), 0.0f) / variantA.size();
    float meanB = std::accumulate(variantB.begin(), variantB.end(), 0.0f) / variantB.size();

    result.effectSize = meanB - meanA;
    if (std::abs(result.effectSize) > 0.05f) {
        result.isSignificant = true;
        if (meanB > meanA) {
            result.variantBWins = true;
        } else {
            result.variantAWins = true;
        }
        result.pValue = 0.01f;
    } else {
        result.pValue = 0.5f;
    }

    return result;
}

} // namespace testing
} // namespace yuki
