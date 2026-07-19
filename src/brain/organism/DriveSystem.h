#pragma once
// ============================================================================
//  DriveSystem — Motivation Layer
//  Four drives generate behavior from deficits:
//    Homeostasis (maintain viability), Curiosity (information-gain reward),
//    Social (loneliness penalty), Competence (mastery reward).
//  Their state is summarized as the Global Affect State ("mood") which biases
//  policy selection, and as concrete GoalProposals the OrganismController and
//  PolicySelector can act on without a user prompt.
//  Constitutional P5: thresholds constexpr and documented.
// ============================================================================

#include <string>
#include <vector>

#include "MetabolismEngine.h"

namespace yuki::organism {

enum class GoalKind {
    MaintainViability,    // homeostasis: earn credits / optimize / conserve
    ResearchTopic,        // curiosity: surprise-driven information foraging
    InitiateConversation, // social: proactively talk to the user
    RequestHarderTask,    // competence: surplus + mastery -> seek challenge
    Rest                  // starving: enter low-power rest
};

struct GoalProposal {
    GoalKind kind = GoalKind::MaintainViability;
    double urgency = 0.0; // 0..1
    std::string rationale;
};

struct AffectState {
    double urgency      = 0.0; // survival pressure
    double contentment  = 0.5; // satiation + social health
    double restlessness = 0.0; // surplus energy seeking an outlet
};

struct DriveInputs {
    MetabolicSnapshot metabolic;
    double secondsSinceUserInteraction = 0.0;
    double secondsSinceDiscovery       = 0.0;
    double recentTaskSuccessRate       = 1.0; // 0..1
    double surplusCredits              = 0.0;
};

class DriveSystem {
public:
    // Loneliness ramps from onset to full deficit.
    static constexpr double kLonelinessOnsetSec = 900.0;  // 15 min
    static constexpr double kLonelinessFullSec  = 3600.0; // 1 h
    // Curiosity ramps when no discovery has been made recently.
    static constexpr double kCuriosityOnsetSec  = 600.0;  // 10 min
    static constexpr double kCuriosityFullSec   = 2400.0; // 40 min
    // Deficits below this stay silent (no goal proposed).
    static constexpr double kGoalThreshold      = 0.35;
    // Credit surplus at which the organism starts feeling "bored".
    static constexpr double kBoredomSurplus     = 20.0;

    void update(const DriveInputs& in);

    double homeostasisDeficit() const { return m_homeostasis; }
    double curiosityDeficit()   const { return m_curiosity; }
    double socialDeficit()      const { return m_social; }
    double competenceDeficit()  const { return m_competence; }

    AffectState affect() const { return m_affect; }
    std::vector<GoalProposal> proposeGoals() const;

private:
    static double ramp(double x, double onset, double full);

    double m_homeostasis = 0.0;
    double m_curiosity   = 0.0;
    double m_social      = 0.0;
    double m_competence  = 0.0;
    AffectState m_affect;
};

} // namespace yuki::organism
