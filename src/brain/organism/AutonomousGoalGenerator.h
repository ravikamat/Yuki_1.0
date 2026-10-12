#pragma once
#include "DriveSystem.h"
#include <string>
#include <vector>
#include <memory>

namespace yuki::organism {

struct AutonomousGoal {
    std::string title;
    std::string topic;
    std::string drive_source; // "curiosity", "competence", "social", "homeostasis"
    float priority = 0.0f;
    uint32_t target_domain = 0;
};

class AutonomousGoalGenerator {
public:
    explicit AutonomousGoalGenerator(DriveSystem* drives = nullptr);
    ~AutonomousGoalGenerator() = default;

    void setDriveSystem(DriveSystem* drives) noexcept { drives_ = drives; }

    /// Generate prioritized autonomous goal from current 4D drive deficits
    AutonomousGoal generate();

private:
    DriveSystem* drives_{nullptr};
};

} // namespace yuki::organism
