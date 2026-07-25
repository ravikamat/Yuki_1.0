#include "DriveSystem.h"
#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/ValenceArousalModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

// ── M9 Implementations ───────────────────────────────────────────────────────

void DriveSystem::proposeGoals(const yuki::self::SelfModel& self,
                                const yuki::self::TheoryOfMind& tom,
                                const yuki::emotion::ValenceArousalModel& emotion) {
    active_goals_.clear();

    float mean_cap = 0.0f;
    for (float c : self.capabilityVector()) {
        mean_cap += c;
    }
    mean_cap /= static_cast<float>(yuki::self::SelfModel::kCapabilityDims);

    float curiosity_prio = (1.0f - mean_cap) * (1.0f + emotion.arousal()) * 0.5f;
    if (curiosity_prio > kGoalPriorityThreshold) {
        active_goals_.push_back({DriveGoal::Type::CURIOSITY, curiosity_prio, 0, getDriveActivations()});
    }

    float competence_prio = (1.0f - mean_cap) * (1.0f - emotion.valence()) * 0.5f;
    if (competence_prio > kGoalPriorityThreshold) {
        active_goals_.push_back({DriveGoal::Type::COMPETENCE, competence_prio, 1, getDriveActivations()});
    }

    float social_prio = (1.0f - tom.userTrust()) * 0.8f + (tom.interactionCount() < 10 ? 0.2f : 0.0f);
    if (social_prio > kGoalPriorityThreshold) {
        active_goals_.push_back({DriveGoal::Type::SOCIAL, social_prio, 2, getDriveActivations()});
    }

    float homeostasis_prio = (1.0f - self.energyLevel()) * 0.9f;
    if (homeostasis_prio > kGoalPriorityThreshold) {
        active_goals_.push_back({DriveGoal::Type::HOMEOSTASIS, homeostasis_prio, 3, getDriveActivations()});
    }

    std::sort(active_goals_.begin(), active_goals_.end(), [](const DriveGoal& a, const DriveGoal& b) {
        return a.priority > b.priority;
    });

    resolveConflicts();
}

DriveGoal DriveSystem::topGoal() const {
    if (active_goals_.empty()) {
        return DriveGoal{DriveGoal::Type::NONE, 0.0f, 0, {}};
    }
    return active_goals_.front();
}

void DriveSystem::resolveConflicts() {
    if (active_goals_.size() > kMaxActiveGoals) {
        active_goals_.erase(active_goals_.begin() + kMaxActiveGoals, active_goals_.end());
    }
}

void DriveSystem::updateFromOutcome(bool /*success*/, float reward) {
    DriveGoal goal = topGoal();
    if (goal.type != DriveGoal::Type::NONE) {
        size_t idx = static_cast<size_t>(goal.type) - 1;
        if (idx < 4) {
            drive_satisfaction_[idx] = 0.1f * reward + 0.9f * drive_satisfaction_[idx];
        }
    }
}

std::array<float, 4> DriveSystem::getDriveActivations() const {
    return {
        static_cast<float>(m_curiosity),
        static_cast<float>(m_competence),
        static_cast<float>(m_social),
        static_cast<float>(m_homeostasis)
    };
}

std::vector<uint8_t> DriveSystem::serializeGoals() const {
    std::vector<uint8_t> out;
    size_t count = active_goals_.size();
    out.reserve(8 + count * 25);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kGoalSerializationMagic;
    uint32_t goal_count = static_cast<uint32_t>(count);

    append(&magic, sizeof(magic));
    append(&goal_count, sizeof(goal_count));

    for (const auto& g : active_goals_) {
        uint8_t type_val = static_cast<uint8_t>(g.type);
        append(&type_val, sizeof(type_val));
        append(&g.priority, sizeof(g.priority));
        append(&g.target_domain, sizeof(g.target_domain));
        append(g.drive_activations.data(), sizeof(float) * 4);
    }

    return out;
}

bool DriveSystem::deserializeGoals(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;

    uint32_t magic = 0;
    uint32_t count = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&count, data.data() + 4, sizeof(count));

    if (magic != kGoalSerializationMagic) return false;
    if (data.size() < 8 + count * 25) return false;

    active_goals_.clear();
    size_t offset = 8;
    for (uint32_t i = 0; i < count; ++i) {
        DriveGoal g;
        uint8_t type_val = 0;
        std::memcpy(&type_val, data.data() + offset, sizeof(type_val));
        g.type = static_cast<DriveGoal::Type>(type_val);
        offset += sizeof(type_val);

        std::memcpy(&g.priority, data.data() + offset, sizeof(g.priority));
        offset += sizeof(g.priority);

        std::memcpy(&g.target_domain, data.data() + offset, sizeof(g.target_domain));
        offset += sizeof(g.target_domain);

        std::memcpy(g.drive_activations.data(), data.data() + offset, sizeof(float) * 4);
        offset += sizeof(float) * 4;

        active_goals_.push_back(g);
    }

    return true;
}

} // namespace yuki::organism
