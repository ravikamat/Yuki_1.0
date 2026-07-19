#pragma once
// ============================================================================
//  OrganismController — the organism loop (evolution of BabyMode)
//  Orchestrates metabolism, economy, and drives. Life-event hooks are wired
//  from TurnCoordinator / ToolExecutor / KnowledgeDaemon. The controller
//  proposes proactive actions when drive deficits exceed the action gate,
//  and gates sleep consolidation behind earned credits.
// ============================================================================

#include <optional>

#include "DriveSystem.h"
#include "EconomyEngine.h"
#include "MetabolismEngine.h"

namespace yuki::organism {

class OrganismController {
public:
    // Minimum goal urgency before the organism acts without a prompt.
    static constexpr double kActionUrgencyGate = 0.45;
    // Extended inactivity after which atrophy is billed hourly.
    static constexpr double kAtrophyAfterSec   = 6.0 * 3600.0;

    OrganismController();

    // Advance the organism by dtSeconds: regen, upkeep, drives, atrophy.
    void tick(double dtSeconds);

    // -- Life-event hooks ----------------------------------------------------
    void onUserInteraction();
    void onTaskCompleted(double qualityScore); // 0..1
    void onTaskFailed();
    void onProactiveHelpAccepted();
    void onProactiveHelpRejected();
    void onKnowledgeDiscovered();
    void onEfficiencyImprovement();
    void onMemoryOverflow(double gbFreed);

    // Sleep gating: consolidation is an investment that must be affordable.
    bool tryBeginSleepConsolidation();

    // Highest-urgency drive goal above the action gate, if any.
    std::optional<GoalProposal> nextProactiveAction() const;

    AffectState affect() const { return m_drives.affect(); }

    MetabolismEngine& metabolism() { return m_metabolism; }
    EconomyEngine&    economy()    { return m_economy; }
    DriveSystem&      drives()     { return m_drives; }

private:
    MetabolismEngine m_metabolism;
    EconomyEngine    m_economy;
    DriveSystem      m_drives;

    double m_sinceInteractionSec = 0.0;
    double m_sinceDiscoverySec   = 0.0;
    double m_atrophyAccumSec     = 0.0;
    bool   m_starvationBilled    = false;
    int    m_tasksCompleted      = 0;
    int    m_tasksFailed         = 0;
};

} // namespace yuki::organism
