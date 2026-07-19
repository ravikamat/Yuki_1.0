#include "EconomyEngine.h"

#include <algorithm>
#include <utility>

namespace yuki::organism {

EconomyEngine::EconomyEngine(MetabolismEngine& metabolism)
    : m_metabolism(metabolism), m_credits(0.0), m_reputation(1.0) {}

double EconomyEngine::credits() const { return m_credits; }
double EconomyEngine::reputation() const { return m_reputation; }

void EconomyEngine::record(double delta, std::string reason) {
    m_credits += delta;
    Transaction t;
    t.credits = delta;
    t.reason  = std::move(reason);
    m_ledger.push_back(std::move(t));
    if (m_ledger.size() > kLedgerCap) {
        m_ledger.erase(m_ledger.begin(),
                       m_ledger.begin() + static_cast<std::ptrdiff_t>(m_ledger.size() - kLedgerCap));
    }
}

void EconomyEngine::adjustReputation(double delta) {
    m_reputation = std::clamp(m_reputation + delta, kReputationMin, kReputationMax);
}

void EconomyEngine::earn(IncomeSource source, double qualityScore) {
    const double q = std::clamp(qualityScore, 0.0, 1.0);
    double base = 0.0;
    const char* reason = "";
    switch (source) {
        case IncomeSource::TaskCompleted:
            base = kTaskBaseCredits; reason = "task completed";
            adjustReputation(+kReputationStep);
            break;
        case IncomeSource::ProactiveHelpAccepted:
            base = kProactiveBonus; reason = "proactive help accepted";
            adjustReputation(+kReputationStep);
            break;
        case IncomeSource::KnowledgeDiscovered:
            base = kDiscoveryBonus; reason = "knowledge discovered";
            break;
        case IncomeSource::EfficiencyImprovement:
            base = kOptimizationBonus; reason = "efficiency improvement";
            break;
    }
    record(base * (0.5 + 0.5 * q) * m_reputation, reason);
}

void EconomyEngine::penalize(PenaltyKind kind) {
    switch (kind) {
        case PenaltyKind::FailedTask:
            adjustReputation(-kReputationStep);
            record(-kFailedTaskPenalty, "failed task");
            break;
        case PenaltyKind::UserRejection:
            adjustReputation(-kReputationStep);
            record(-kRejectionPenalty, "user rejection");
            break;
        case PenaltyKind::ResourceExhaustion:
            record(-kExhaustionPenalty, "resource exhaustion");
            break;
        case PenaltyKind::MemoryOverflow:
            record(-kOverflowPenalty, "memory overflow");
            break;
        case PenaltyKind::Atrophy:
            record(-kAtrophyPenaltyPerHour, "atrophy (extended inactivity)");
            break;
    }
}

void EconomyEngine::payUpkeep(double dtSeconds) {
    if (dtSeconds <= 0.0) return;
    record(-kUpkeepCreditsPerSecond * dtSeconds, "electricity upkeep");
    m_metabolism.consumePower(kIdleDrawKWhPerSecond * dtSeconds);
}

bool EconomyEngine::payInference(double gflop) {
    const double cost = gflop * kInferenceCostPerGflop;
    if (m_credits < cost) return false;
    if (!m_metabolism.consumeCompute(gflop)) return false;
    record(-cost, "inference");
    return true;
}

bool EconomyEngine::payStorage(double gb) {
    const double cost = gb * kStorageCostPerGb;
    if (m_credits < cost) return false;
    if (!m_metabolism.consumeStorage(gb)) return false;
    record(-cost, "storage");
    return true;
}

bool EconomyEngine::payNetwork(double mb) {
    const double cost = mb * kNetworkCostPerMb;
    if (m_credits < cost) return false;
    if (!m_metabolism.consumeNetwork(mb)) return false;
    record(-cost, "network");
    return true;
}

bool EconomyEngine::canAffordSleep() const {
    return m_credits >= kSleepConsolidationCost
        && m_metabolism.power().availableUnits >= kSleepPowerKWh;
}

bool EconomyEngine::paySleepConsolidation() {
    if (!canAffordSleep()) return false;
    m_metabolism.consumePower(kSleepPowerKWh);
    record(-kSleepConsolidationCost, "sleep consolidation");
    return true;
}

double EconomyEngine::upgradeCost(UpgradeKind kind) const {
    switch (kind) {
        case UpgradeKind::LargerModel:       return kCostLargerModel;
        case UpgradeKind::FasterInference:   return kCostFasterInference;
        case UpgradeKind::MoreMemory:        return kCostMoreMemory;
        case UpgradeKind::BetterSensors:     return kCostBetterSensors;
        case UpgradeKind::NewTool:           return kCostNewTool;
        case UpgradeKind::ComputeRedundancy: return kCostComputeRedundancy;
    }
    return 0.0;
}

bool EconomyEngine::purchase(UpgradeKind kind) {
    const double cost = upgradeCost(kind);
    if (m_credits < cost) return false;
    switch (kind) {
        case UpgradeKind::LargerModel:       m_metabolism.expandCompute(250.0); break;
        case UpgradeKind::FasterInference:   m_metabolism.expandCompute(150.0); break;
        case UpgradeKind::MoreMemory:        m_metabolism.expandStorage(10.0);  break;
        case UpgradeKind::BetterSensors:     m_metabolism.expandNetwork(256.0); break;
        case UpgradeKind::NewTool:           /* capability registered by SkillSystem */ break;
        case UpgradeKind::ComputeRedundancy: m_metabolism.expandPower(0.5);     break;
    }
    record(-cost, "upgrade purchased");
    return true;
}

const std::vector<Transaction>& EconomyEngine::ledger() const { return m_ledger; }

} // namespace yuki::organism
