#pragma once
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include "brain/predictive/CognitiveTypes.h"

namespace yuki {
namespace brain {
namespace inference {

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
    float free_energy = 0.0f;   // Variational free energy
    float belief_entropy = 0.0f;       // Belief entropy
    std::array<float, 48> q_joint() const; // 8 intents x 3 engagement x 2 urgency = 48
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
    void normalize() { normalize_(); }
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);
    void serialize(std::ostream& os) const;
    bool deserialize(std::istream& is);
    
private:
    void normalize_();
};

} // namespace inference
} // namespace brain
} // namespace yuki