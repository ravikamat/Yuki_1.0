#include <iostream>
#include <cassert>
#include "src/brain/learning/neural/RewardShaper.h"

int main() {
    using yuki::learning::neural::RewardShaper;
    using yuki::learning::neural::RewardContext;

    RewardShaper shaper;
    RewardContext c;
    c.ownerAccepted = true;
    c.taskCompleted = true;
    c.factVerified = true;
    c.selfEvalApproved = true;
    c.critiqueApproved = true;
    c.safe = true;
    c.usedEfficientTools = true;
    c.localBackendSucceeded = true;

    float r = shaper.computeReward(c);
    if (r <= 3.0f) {
        std::cerr << "[FAIL] testrewardshaper_extended: reward should be high for optimal context\n";
        return 1;
    }

    std::cout << "[PASS] testrewardshaper_extended\n";
    return 0;
}
