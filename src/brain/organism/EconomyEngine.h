#pragma once
// ============================================================================
//  EconomyEngine — Resource Economy (Earn → Spend → Grow)
//  Credits are the organism's currency. Work earns them, existing costs them,
//  and surplus buys capability upgrades. Sleep consolidation is an investment
//  that must be affordable — the organism has to earn its dreams.
//  Constitutional P5: every tariff is constexpr and documented.
// ============================================================================

#include <cstddef>
#include <string>
#include <vector>

#include "MetabolismEngine.h"

namespace yuki::organism {

enum class IncomeSource {
    TaskCompleted,          // base credits
    ProactiveHelpAccepted,  // bonus credits
    KnowledgeDiscovered,    // discovery bonus
    EfficiencyImprovement   // optimization bonus
};

enum class PenaltyKind {
    FailedTask,         // cost + reputation loss
    UserRejection,      // social penalty
    ResourceExhaustion, // "hunger" — degraded performance billed once per episode
    MemoryOverflow,     // forced forgetting
    Atrophy             // extended inactivity — model drift
};

enum class UpgradeKind {
    LargerModel,       // more capable reasoning
    FasterInference,   // lower latency
    MoreMemory,        // longer context, more episodes
    BetterSensors,     // camera / mic / screen bandwidth
    NewTool,           // API or software capability (registered by SkillSystem)
    ComputeRedundancy  // survival insurance
};

struct Transaction {
    double credits = 0.0; // signed delta
    std::string reason;
};

class EconomyEngine {
public:
    // -- Income tariffs ------------------------------------------------------
    static constexpr double kTaskBaseCredits   = 10.0;
    static constexpr double kProactiveBonus    = 15.0;
    static constexpr double kDiscoveryBonus    = 8.0;
    static constexpr double kOptimizationBonus = 12.0;

    // -- Penalty tariffs -----------------------------------------------------
    static constexpr double kFailedTaskPenalty     = 6.0;
    static constexpr double kRejectionPenalty      = 4.0;
    static constexpr double kExhaustionPenalty     = 10.0;
    static constexpr double kOverflowPenalty       = 5.0;
    static constexpr double kAtrophyPenaltyPerHour = 1.0;

    // -- Reputation multiplier (applied to all income) ----------------------
    static constexpr double kReputationMin  = 0.5;
    static constexpr double kReputationMax  = 2.0;
    static constexpr double kReputationStep = 0.05;

    // -- Running costs -------------------------------------------------------
    static constexpr double kUpkeepCreditsPerSecond = 0.002;  // electricity bill
    static constexpr double kIdleDrawKWhPerSecond   = 0.0002; // idle power draw
    static constexpr double kInferenceCostPerGflop  = 0.0005; // per-thought cost
    static constexpr double kStorageCostPerGb       = 0.05;   // per-GB cost
    static constexpr double kNetworkCostPerMb       = 0.001;  // per-MB cost
    static constexpr double kSleepConsolidationCost = 2.5;    // offline investment
    static constexpr double kSleepPowerKWh          = 0.01;   // power for one epoch

    // -- Upgrade price list --------------------------------------------------
    static constexpr double kCostLargerModel       = 120.0;
    static constexpr double kCostFasterInference   = 80.0;
    static constexpr double kCostMoreMemory        = 60.0;
    static constexpr double kCostBetterSensors     = 70.0;
    static constexpr double kCostNewTool           = 40.0;
    static constexpr double kCostComputeRedundancy = 100.0;

    static constexpr std::size_t kLedgerCap = 512; // bounded transaction log

    explicit EconomyEngine(MetabolismEngine& metabolism);

    double credits() const;
    double reputation() const;

    // Income. qualityScore in 0..1 scales the payout (0.5x .. 1.0x of base),
    // then the reputation multiplier is applied.
    void earn(IncomeSource source, double qualityScore);
    void penalize(PenaltyKind kind);

    // Continuous electricity cost + idle power draw. Call every tick.
    void payUpkeep(double dtSeconds);

    // Per-use costs, coupled to metabolic draw. False = cannot afford.
    bool payInference(double gflop);
    bool payStorage(double gb);
    bool payNetwork(double mb);

    // Sleep gating — consolidation must be earned.
    bool canAffordSleep() const;
    bool paySleepConsolidation();

    double upgradeCost(UpgradeKind kind) const;
    bool purchase(UpgradeKind kind); // applies capacity effect to metabolism

    const std::vector<Transaction>& ledger() const;

private:
    void record(double delta, std::string reason);
    void adjustReputation(double delta);

    MetabolismEngine& m_metabolism;
    double m_credits;
    double m_reputation;
    std::vector<Transaction> m_ledger;
};

} // namespace yuki::organism
