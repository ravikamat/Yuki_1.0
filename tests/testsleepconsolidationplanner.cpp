#include <iostream>
#include <cassert>
#include "src/brain/sleep/SleepConsolidationPlanner.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"

int main() {
    using yuki::brain::sleep::SleepConsolidationPlanner;
    using yuki::brain::sleep::SleepPlanMode;
    using yuki::platform::DeviceProfile;
    using yuki::platform::DeviceTier;
    using yuki::platform::RuntimeBudget;

    SleepConsolidationPlanner planner;
    DeviceProfile profile;
    RuntimeBudget budget;

    profile.tier = DeviceTier::VERY_LOW;
    auto plan = planner.planSleepCycle(profile, budget);
    if (plan.mode != SleepPlanMode::EXTRACTION_ONLY) {
        std::cerr << "[FAIL] testsleepconsolidationplanner: expected EXTRACTION_ONLY for VERY_LOW device\n";
        return 1;
    }

    std::cout << "[PASS] testsleepconsolidationplanner\n";
    return 0;
}
