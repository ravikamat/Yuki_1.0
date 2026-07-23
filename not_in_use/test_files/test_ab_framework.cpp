#include "brain/testing/ABTestFramework.h"
#include <cassert>

int main() {
    yuki::testing::ABTestFramework ab;

    std::vector<float> vA = {1.0f, 1.1f, 1.2f};
    std::vector<float> vB = {2.0f, 2.1f, 2.2f};

    auto result = ab.compare(vA, vB);
    assert(result.isSignificant);
    assert(result.variantBWins);

    return 0;
}
