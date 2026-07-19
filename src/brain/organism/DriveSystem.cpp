#include "DriveSystem.h"

#include <algorithm>

namespace yuki::organism {

double DriveSystem::ramp(double x, double onset, double full) {
    if (full <= onset) return (x >= full) ? 1.0 : 0.0;
    return std::clamp((x - onset) / (full - onset), 0.0, 1.0);
}

void DriveSystem::update(const DriveInputs& in) {
    // Homeostasis: deficit grows as the weakest metabolic budget drains.
    m_homeostasis = std::clamp(1.0 - in.metabolic.viability, 0.0, 1.0);

    // Curiosity: nothing new discovered lately -> information foraging urge.
    m_curiosity = ramp(in.secondsSinceDiscovery, kCuriosityOnsetSec, kCuriosityFullSec);

    // Social: loneliness ramps with time since last user interaction.
    m_social = ramp(in.secondsSinceUserInteraction, kLonelinessOnsetSec, kLonelinessFullSec);

    // Competence: mastery (high success rate) plus credit surplus = boredom,
    // which motivates asking for harder work.
    m_competence = std::clamp(in.recentTaskSuccessRate, 0.0, 1.0)
                 * ramp(in.surplusCredits, kBoredomSurplus, 5.0 * kBoredomSurplus);

    // Global affect ("mood").
    m_affect.urgency      = in.metabolic.starving ? 1.0 : m_homeostasis;
    m_affect.contentment  = std::clamp((in.metabolic.viability + (1.0 - m_social)) * 0.5, 0.0, 1.0);
    m_affect.restlessness = std::max(m_curiosity, m_competence);
}

std::vector<GoalProposal> DriveSystem::proposeGoals() const {
    std::vector<GoalProposal> goals;
    if (m_homeostasis > kGoalThreshold)
        goals.push_back({GoalKind::MaintainViability, m_homeostasis,
                         "resource reserves low - earn credits or optimize"});
    if (m_curiosity > kGoalThreshold)
        goals.push_back({GoalKind::ResearchTopic, m_curiosity,
                         "no recent discovery - forage for information"});
    if (m_social > kGoalThreshold)
        goals.push_back({GoalKind::InitiateConversation, m_social,
                         "loneliness - initiate interaction with the user"});
    if (m_competence > kGoalThreshold)
        goals.push_back({GoalKind::RequestHarderTask, m_competence,
                         "surplus energy + mastery - request harder work"});
    if (m_affect.urgency >= 1.0)
        goals.push_back({GoalKind::Rest, 1.0,
                         "starving - enter low-power rest"});
    return goals;
}

} // namespace yuki::organism
