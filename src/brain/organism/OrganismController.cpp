#include "OrganismController.h"

namespace yuki::organism {

OrganismController::OrganismController() : m_economy(m_metabolism) {}

void OrganismController::tick(double dtSeconds) {
    if (dtSeconds <= 0.0) return;

    m_metabolism.tick(dtSeconds);
    m_economy.payUpkeep(dtSeconds);

    m_sinceInteractionSec += dtSeconds;
    m_sinceDiscoverySec   += dtSeconds;

    // Atrophy: extended inactivity is billed hourly (model drift analog).
    if (m_sinceInteractionSec > kAtrophyAfterSec) {
        m_atrophyAccumSec += dtSeconds;
        while (m_atrophyAccumSec >= 3600.0) {
            m_economy.penalize(PenaltyKind::Atrophy);
            m_atrophyAccumSec -= 3600.0;
        }
    }

    DriveInputs in;
    in.metabolic = m_metabolism.snapshot();
    in.secondsSinceUserInteraction = m_sinceInteractionSec;
    in.secondsSinceDiscovery       = m_sinceDiscoverySec;
    const int total = m_tasksCompleted + m_tasksFailed;
    in.recentTaskSuccessRate = (total > 0)
        ? static_cast<double>(m_tasksCompleted) / static_cast<double>(total)
        : 1.0;
    in.surplusCredits = m_economy.credits();
    m_drives.update(in);

    // Hunger has a price — billed once per starvation episode.
    if (in.metabolic.starving) {
        if (!m_starvationBilled) {
            m_economy.penalize(PenaltyKind::ResourceExhaustion);
            m_starvationBilled = true;
        }
    } else {
        m_starvationBilled = false;
    }
}

void OrganismController::onUserInteraction() {
    m_sinceInteractionSec = 0.0;
    m_atrophyAccumSec     = 0.0;
}

void OrganismController::onTaskCompleted(double qualityScore) {
    ++m_tasksCompleted;
    m_economy.earn(IncomeSource::TaskCompleted, qualityScore);
}

void OrganismController::onTaskFailed() {
    ++m_tasksFailed;
    m_economy.penalize(PenaltyKind::FailedTask);
}

void OrganismController::onProactiveHelpAccepted() {
    m_economy.earn(IncomeSource::ProactiveHelpAccepted, 1.0);
}

void OrganismController::onProactiveHelpRejected() {
    m_economy.penalize(PenaltyKind::UserRejection);
}

void OrganismController::onKnowledgeDiscovered() {
    m_sinceDiscoverySec = 0.0;
    m_economy.earn(IncomeSource::KnowledgeDiscovered, 1.0);
}

void OrganismController::onEfficiencyImprovement() {
    m_economy.earn(IncomeSource::EfficiencyImprovement, 1.0);
}

void OrganismController::onMemoryOverflow(double gbFreed) {
    m_metabolism.releaseStorage(gbFreed);
    m_economy.penalize(PenaltyKind::MemoryOverflow);
}

bool OrganismController::tryBeginSleepConsolidation() {
    return m_economy.paySleepConsolidation();
}

std::optional<GoalProposal> OrganismController::nextProactiveAction() const {
    const auto goals = m_drives.proposeGoals();
    const GoalProposal* best = nullptr;
    for (const auto& g : goals) {
        if (!best || g.urgency > best->urgency) best = &g;
    }
    if (best && best->urgency >= kActionUrgencyGate) return *best;
    return std::nullopt;
}

} // namespace yuki::organism
