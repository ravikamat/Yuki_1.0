#include "PolicySelector.h"
#include "GenerativeModel.h"
#include "VariationalStateEstimator.h"
#include "brain/predictive/predictive_turn_engine.h"
#include <cmath>
#include <algorithm>

namespace yuki::inference {

const float PolicySelector::SEED_TEMPLATES[NUM_SEED_TEMPLATES][8] = {
    {0.2f, 0.3f, 0.2f, 0.1f, 0.1f, 0.0f, 0.2f, 0.3f},
    {0.8f, 0.7f, 0.8f, 0.2f, 0.3f, 0.5f, 0.7f, 0.5f},
    {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.5f, 0.5f},
    {0.3f, 0.8f, 0.4f, 0.1f, 0.2f, 0.1f, 0.3f, 0.4f},
    {0.6f, 0.2f, 0.7f, 0.3f, 0.6f, 0.8f, 0.6f, 0.7f},
    {0.1f, 0.1f, 0.1f, 0.9f, 0.0f, 0.0f, 0.1f, 0.1f},
};

PolicySelector::PolicySelector() {
    // ── Default Safety Constraints ─────────────────────────────────────────

    // C1: If urgency is URGENT, never set wait_time > 0.3 (don't delay critical requests)
    addConstraint([](const Policy& p, const BeliefState& belief) {
        auto map = belief.getMAP();
        if (map.urgency == UrgencyLevel::URGENT && p.waitTime() > 0.3f) {
            return false; // Invalid: waiting too long when urgent
        }
        return true;
    });

    // C2: If engagement is LOW, don't use verbose responses (user is disengaged)
    addConstraint([](const Policy& p, const BeliefState& belief) {
        auto map = belief.getMAP();
        if (map.engagement == EngagementLevel::LOW && p.verbosity() > 0.7f) {
            return false; // Invalid: too verbose for low engagement
        }
        return true;
    });

    // C3: If confidence is low (< 0.5), force proactivity down (don't be pushy when uncertain)
    addConstraint([](const Policy& p, const BeliefState& belief) {
        auto map = belief.getMAP();
        if (map.probability < 0.5f && p.proactivity() > 0.6f) {
            return false; // Invalid: too proactive when uncertain
        }
        return true;
    });

    // C4: If safety mass is low (from TurnCoordinator, passed via belief context), veto tool_use
    // Note: This requires the BeliefState to carry safety context. For now, we use a proxy:
    // if intent is UNKNOWN, don't use tools heavily (avoid speculative tool calls)
    addConstraint([](const Policy& p, const BeliefState& belief) {
        auto map = belief.getMAP();
        if (map.intent == yuki::IntentClass::UNKNOWN && p.toolUse() > 0.5f) {
            return false; // Invalid: speculative tool use when intent unclear
        }
        return true;
    });

    // C5: If tone is highly empathetic (> 0.8), don't be brief (empathy needs some length)
    addConstraint([](const Policy& p, const BeliefState& /*belief*/) {
        if (p.tone() > 0.8f && p.responseLength() < 0.3f) {
            return false; // Invalid: empathetic but too brief
        }
        return true;
    });

    // C6: If detail_level is high (> 0.7), response_length must be at least medium
    addConstraint([](const Policy& p, const BeliefState& /*belief*/) {
        if (p.detailLevel() > 0.7f && p.responseLength() < 0.4f) {
            return false; // Invalid: detailed but too short to fit detail
        }
        return true;
    });

    // C7: Never set all parameters to extremes (0 or 1) — prevents brittle policies
    addConstraint([](const Policy& p, const BeliefState& /*belief*/) {
        int extremes = 0;
        for (float param : p.parameters) {
            if (param < 0.05f || param > 0.95f) extremes++;
        }
        return extremes < 4; // At most 3 parameters can be extreme
    });
}

PolicyResult PolicySelector::selectPolicy(
    const BeliefState& current_belief,
    const GenerativeModel& model,
    const FreeEnergyCalculator& calculator)
{
    auto seeds = generateSeedPolicies(current_belief);
    std::vector<Policy> valid_seeds;
    int rejected_count = 0;
    for (const auto& seed : seeds) {
        if (isPolicyValid(seed, current_belief)) {
            valid_seeds.push_back(seed);
        } else {
            rejected_count++;
        }
    }
    if (valid_seeds.empty()) {
        // Fallback: use most conservative seed, but relax constraints if even that fails
        Policy fallback;
        fallback.parameters = std::vector<float>(SEED_TEMPLATES[0], SEED_TEMPLATES[0] + 8);
        fallback.description = "fallback_conservative";
        if (isPolicyValid(fallback, current_belief)) {
            valid_seeds.push_back(fallback);
        } else {
            // Emergency: all constraints violated, force absolute minimum
            Policy emergency;
            emergency.parameters = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.0f, 0.1f, 0.1f};
            emergency.description = "emergency_minimal";
            valid_seeds.push_back(emergency);
        }
    }
    float initial_G = calculator.computeG(valid_seeds[0], current_belief, model);
    Policy optimized = calculator.optimizePolicy(valid_seeds, current_belief, model, 50, 0.05f);
    float final_G = calculator.computeG(optimized, current_belief, model);
    std::string plan;
    if (optimized.responseLength() < 0.3f) plan += "short_response ";
    else if (optimized.responseLength() > 0.7f) plan += "long_response ";
    else plan += "medium_response ";
    if (optimized.tone() > 0.6f) plan += "empathetic_tone ";
    else if (optimized.tone() < 0.3f) plan += "neutral_tone ";
    if (optimized.detailLevel() > 0.6f) plan += "detailed ";
    else if (optimized.detailLevel() < 0.3f) plan += "brief ";
    if (optimized.waitTime() > 0.7f) plan += "wait_for_more ";
    else plan += "respond_now ";
    if (optimized.proactivity() > 0.6f) plan += "proactive ";
    else plan += "reactive ";
    if (optimized.toolUse() > 0.5f) plan += "use_tools ";
    PolicyResult result;
    result.selected_policy = optimized;
    result.final_G = final_G;
    result.initial_G = initial_G;
    result.optimization_steps = 50;
    result.execution_plan = plan;
    last_result_ = result;
    return result;
}

std::vector<Policy> PolicySelector::generateSeedPolicies(const BeliefState& belief) const {
    std::vector<Policy> seeds;
    auto map = belief.getMAP();
    for (size_t t = 0; t < NUM_SEED_TEMPLATES; ++t) {
        Policy p;
        p.parameters = std::vector<float>(SEED_TEMPLATES[t], SEED_TEMPLATES[t] + 8);
        if (map.urgency == UrgencyLevel::URGENT) {
            p.parameters[3] = std::min(0.2f, p.parameters[3]);
            p.parameters[0] = std::max(0.5f, p.parameters[0]);
        }
        if (map.engagement == EngagementLevel::LOW) {
            p.parameters[4] = std::max(0.6f, p.parameters[4]);
            p.parameters[0] = std::min(0.4f, p.parameters[0]);
        }
        if (map.engagement == EngagementLevel::HIGH) {
            p.parameters[0] = std::max(0.6f, p.parameters[0]);
            p.parameters[2] = std::max(0.6f, p.parameters[2]);
        }
        switch (t) {
            case 0: p.description = "conservative"; break;
            case 1: p.description = "thorough"; break;
            case 2: p.description = "balanced"; break;
            case 3: p.description = "empathetic"; break;
            case 4: p.description = "proactive_tool_user"; break;
            case 5: p.description = "patient_waiter"; break;
            default: p.description = "custom";
        }
        seeds.push_back(p);
    }
    return seeds;
}

void PolicySelector::addConstraint(ConstraintFn constraint) {
    constraints_.push_back(constraint);
}
bool PolicySelector::isPolicyValid(const Policy& policy, const BeliefState& belief) const {
    for (size_t i = 0; i < constraints_.size(); ++i) {
        if (!constraints_[i](policy, belief)) {
            // Log which constraint fired (for debugging)
            // TODO: integrate with Yuki's logging system
            return false;
        }
    }
    return true;
}
}
