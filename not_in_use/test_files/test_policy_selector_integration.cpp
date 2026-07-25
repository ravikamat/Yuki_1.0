// test_policy_selector_integration.cpp -- PolicySelector + YNC ensemble integration
#include "brain/policy/PolicySelector.h"
#include "brain/policy/LearnedEnsemblePolicy.h"
#include "brain/metacognition/CompetenceRecord.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <memory>

using namespace yuki::policy;
using namespace yuki::metacognition;

static void test_select_no_ync_baseline() {
    CompetenceRecord cr[static_cast<int>(CompetenceDomain::COUNT)];
    for (auto& c : cr) c.success_rate_ema = 0.8f;
    PolicySelector sel(cr);

    std::vector<float> intent = {0.0f, 0.8f, 0.1f, 0.1f};
    auto result = sel.select(intent, "test input", 0);
    // High competence + dominant intent -> EXECUTE
    assert(result.execution_mode == ExecutionMode::EXECUTE ||
           result.execution_mode == ExecutionMode::CLARIFY);
    std::puts("test_select_no_ync_baseline PASS");
}

static void test_select_low_competence_defers() {
    CompetenceRecord cr[static_cast<int>(CompetenceDomain::COUNT)];
    for (auto& c : cr) c.success_rate_ema = 0.05f; // very low
    PolicySelector sel(cr);

    std::vector<float> intent = {0.5f, 0.5f};
    auto result = sel.select(intent, "x", 0);
    assert(result.execution_mode == ExecutionMode::DEFER);
    std::puts("test_select_low_competence_defers PASS");
}

static void test_adapt_threshold_clamps() {
    CompetenceRecord cr[static_cast<int>(CompetenceDomain::COUNT)];
    for (auto& c : cr) c.success_rate_ema = 0.5f;
    PolicySelector sel(cr);

    float thresh_before = sel.currentThreshold();
    sel.adaptThreshold(1.0f); // very high trend -> lower threshold
    float thresh_after = sel.currentThreshold();
    assert(thresh_after >= PolicySelector::THRESHOLD_MIN);
    assert(thresh_after <= PolicySelector::THRESHOLD_MAX);
    (void)thresh_before;
    std::puts("test_adapt_threshold_clamps PASS");
}

static void test_set_learned_policy_no_crash() {
    CompetenceRecord cr[static_cast<int>(CompetenceDomain::COUNT)];
    for (auto& c : cr) c.success_rate_ema = 0.5f;
    PolicySelector sel(cr);

    auto policy = std::make_unique<LearnedEnsemblePolicy>();
    sel.setLearnedPolicy(std::move(policy));
    // Should not crash and ensemble is advisory-only (not trained yet)
    std::vector<float> intent = {0.6f, 0.4f};
    auto result = sel.select(intent, "test", 0);
    (void)result;
    std::puts("test_set_learned_policy_no_crash PASS");
}

static void test_requires_approval_high_risk() {
    CompetenceRecord cr[static_cast<int>(CompetenceDomain::COUNT)];
    for (auto& c : cr) c.success_rate_ema = 0.5f;
    PolicySelector sel(cr);

    bool needs = sel.requiresApproval("execute_action", 0.8f);
    assert(needs && "risk >= 0.7 must require approval");
    std::puts("test_requires_approval_high_risk PASS");
}

int main() {
    test_select_no_ync_baseline();
    test_select_low_competence_defers();
    test_adapt_threshold_clamps();
    test_set_learned_policy_no_crash();
    test_requires_approval_high_risk();
    std::puts("=== test_policy_selector_integration: ALL PASS ===");
    return 0;
}
