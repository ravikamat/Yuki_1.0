#include "AutonomousGoalGenerator.h"
#include <algorithm>

namespace yuki::organism {

AutonomousGoalGenerator::AutonomousGoalGenerator(DriveSystem* drives)
    : drives_(drives)
{
}

AutonomousGoal AutonomousGoalGenerator::generate() {
    AutonomousGoal goal;
    if (!drives_) {
        goal.title = "Explore Environment";
        goal.topic = "general_cognition";
        goal.drive_source = "curiosity";
        goal.priority = 0.50f;
        goal.target_domain = 0;
        return goal;
    }

    double curiosity = drives_->curiosityDeficit();
    double competence = drives_->competenceDeficit();
    double social = drives_->socialDeficit();
    double homeostasis = drives_->homeostasisDeficit();

    double max_deficit = curiosity;
    goal.drive_source = "curiosity";
    goal.title = "Research Unknown Topic";
    goal.topic = "quantum_computing_foundations";
    goal.target_domain = 1;

    if (competence > max_deficit) {
        max_deficit = competence;
        goal.drive_source = "competence";
        goal.title = "Practice Code Synthesis";
        goal.topic = "cxx20_concepts_optimization";
        goal.target_domain = 2;
    }

    if (social > max_deficit) {
        max_deficit = social;
        goal.drive_source = "social";
        goal.title = "Initiate User Interaction";
        goal.topic = "recent_task_assistance";
        goal.target_domain = 3;
    }

    if (homeostasis > max_deficit) {
        max_deficit = homeostasis;
        goal.drive_source = "homeostasis";
        goal.title = "Conserve Resources & Consolidate";
        goal.topic = "database_vacuum_and_wal_checkpoint";
        goal.target_domain = 4;
    }

    goal.priority = std::clamp(static_cast<float>(max_deficit), 0.1f, 1.0f);
    return goal;
}

} // namespace yuki::organism
