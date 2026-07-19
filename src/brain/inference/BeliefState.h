#pragma once
#include <array>
#include <vector>
#include <string>

namespace yuki { enum class IntentClass : uint8_t; }

namespace yuki::inference {

enum class EngagementLevel : uint8_t { LOW = 0, MEDIUM, HIGH, COUNT };
enum class UrgencyLevel : uint8_t { NORMAL = 0, URGENT, COUNT };

class BeliefState {
public:
    BeliefState();
    std::array<float, 8> q_intent;
    std::array<float, 3> q_engagement;
    std::array<float, 2> q_urgency;
    // Context from TurnCoordinator (not part of generative model, used for constraints)
    float safety_mass = 1.0f;   // Safety confidence from safety stream (0 = unsafe, 1 = safe)
    float surprise_budget = 1.0f; // Remaining surprise budget (0 = exhausted)
    std::array<float, 48> q_joint() const; // 8 intents × 3 engagement × 2 urgency = 48
    float entropy() const;
    float klFromPrior(const BeliefState& prior) const;
    void update(const std::vector<float>& prediction_error,
                const std::vector<float>& precision,
                float learning_rate = 0.1f);
    struct MAPState {
        yuki::IntentClass intent;
        EngagementLevel engagement;
        UrgencyLevel urgency;
        float probability;
    };
    MAPState getMAP() const;
    void reset();
private:
    void normalize_();
};
}
