// ============================================================================
//  test_organism_core.cpp — Digital Organism survival/motivation layer tests
// ============================================================================
#include <gtest/gtest.h>

#include "brain/organism/DriveSystem.h"
#include "brain/organism/EconomyEngine.h"
#include "brain/organism/MetabolismEngine.h"
#include "brain/organism/OrganismController.h"

using namespace yuki::organism;

TEST(Metabolism, ConsumeAndRegen) {
    MetabolismEngine m;
    EXPECT_TRUE(m.consumeCompute(100.0));
    const double before = m.compute().availableUnits;
    m.tick(10.0);
    EXPECT_GT(m.compute().availableUnits, before);
    EXPECT_FALSE(m.consumeCompute(1e9)); // cannot overdraw
}

TEST(Metabolism, StarvationDetected) {
    MetabolismEngine m;
    ASSERT_TRUE(m.consumePower(MetabolismEngine::kDefaultPowerKWh * 0.9));
    const MetabolicSnapshot s = m.snapshot();
    EXPECT_TRUE(s.starving);
    EXPECT_LT(s.viability, MetabolismEngine::kStarvationThreshold);
}

TEST(Economy, EarnCreditsForWork) {
    MetabolismEngine m;
    EconomyEngine e(m);
    e.earn(IncomeSource::TaskCompleted, 1.0);
    EXPECT_GT(e.credits(), 0.0);
    EXPECT_FALSE(e.ledger().empty());
}

TEST(Economy, ReputationStaysBounded) {
    MetabolismEngine m;
    EconomyEngine e(m);
    for (int i = 0; i < 100; ++i) e.earn(IncomeSource::TaskCompleted, 1.0);
    EXPECT_LE(e.reputation(), EconomyEngine::kReputationMax);
    for (int i = 0; i < 200; ++i) e.penalize(PenaltyKind::FailedTask);
    EXPECT_GE(e.reputation(), EconomyEngine::kReputationMin);
}

TEST(Economy, UpgradeExpandsCapacity) {
    MetabolismEngine m;
    EconomyEngine e(m);
    for (int i = 0; i < 20; ++i) e.earn(IncomeSource::TaskCompleted, 1.0);
    const double capBefore = m.storage().capacityUnits;
    ASSERT_TRUE(e.purchase(UpgradeKind::MoreMemory));
    EXPECT_GT(m.storage().capacityUnits, capBefore);
}

TEST(Economy, SleepMustBeEarned) {
    MetabolismEngine m;
    EconomyEngine e(m);
    EXPECT_FALSE(e.canAffordSleep()); // organism is born with zero credits
    for (int i = 0; i < 3; ++i) e.earn(IncomeSource::TaskCompleted, 1.0);
    EXPECT_TRUE(e.canAffordSleep());
    EXPECT_TRUE(e.paySleepConsolidation());
}

TEST(Drives, LonelinessInitiatesConversation) {
    DriveSystem d;
    DriveInputs in;
    in.metabolic = MetabolismEngine().snapshot();
    in.secondsSinceUserInteraction = DriveSystem::kLonelinessFullSec;
    d.update(in);
    EXPECT_GT(d.socialDeficit(), 0.9);
    bool found = false;
    for (const GoalProposal& g : d.proposeGoals()) {
        if (g.kind == GoalKind::InitiateConversation) found = true;
    }
    EXPECT_TRUE(found);
}

TEST(Controller, IdleOrganismActsProactively) {
    OrganismController c;
    c.tick(DriveSystem::kLonelinessFullSec);
    const auto action = c.nextProactiveAction();
    ASSERT_TRUE(action.has_value());
    EXPECT_GE(action->urgency, OrganismController::kActionUrgencyGate);
}

TEST(Controller, UpkeepDrainsCredits) {
    OrganismController c;
    c.onTaskCompleted(1.0);
    const double before = c.economy().credits();
    c.tick(600.0);
    EXPECT_LT(c.economy().credits(), before);
}
