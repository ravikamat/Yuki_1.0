#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include <chrono>

namespace yuki {
namespace brain {
namespace inference {

struct GenerativeModelConfig {
    size_t num_intents = 8;
    size_t num_engagements = 3;
    size_t num_urgencies = 2;
    size_t num_modalities = 3;
    size_t text_obs_dim = 12;
    size_t audio_obs_dim = 16;
    size_t vision_obs_dim = 32;
    size_t hidden_dim = 64;
    float mapping_lr = 0.1f;
    float state_lr = 0.01f;
};

enum class Modality {
    TEXT = 0,
    AUDIO = 1,
    VISION = 2
};

enum class IntentClass {
    CHITCHAT = 0,
    QUERY = 1,
    COMMAND = 2,
    EMOTIONAL_VENT = 3,
    META_QUESTION = 4,
    TUTORIAL = 5,
    CREATIVE = 6,
    ABORT = 7
};

struct GenerativeBeliefState {
    std::array<float, 8> q_intent{};
    std::array<float, 3> q_engagement{};
    std::array<float, 2> q_urgency{};
    float safety_mass = 0.0f;
    float surprise_budget = 0.0f;
    float entropy = 0.0f;
    float free_energy = 0.0f;
    
    int getMAP() const;
    float computeEntropy() const;
    float computeFreeEnergy() const;
    void update(const std::vector<float>& prediction_error, const std::vector<float>& precision, float lr);
};

class GenerativeModel {
public:
    struct MappingEntry {
        std::vector<std::vector<float>> modality_obs;
        std::vector<float> prior_prob;
        float confidence = 0.0f;
    };
    
    struct ModalityProcessor {
        std::vector<float> encode_weights;
        std::vector<float> decode_weights;
        size_t input_dim = 0;
        size_t hidden_dim = 0;
    };
    
    explicit GenerativeModel(const GenerativeModelConfig& cfg = GenerativeModelConfig{});
    ~GenerativeModel() = default;
    
    std::vector<std::vector<float>> predictNext(
        const GenerativeBeliefState& belief,
        int action = -1) const;
    
    GenerativeBeliefState predictNextBelief(const GenerativeBeliefState& belief, int action = -1) const;
    
    float expectedFreeEnergy(const GenerativeBeliefState& belief, 
                             const std::vector<int>& policy,
                             int horizon = 5) const;
    
    void updateMapping(Modality modality, const std::vector<float>& observation, 
                       IntentClass intent, float lr = -1.0f);
    
    void updateTransitions(const GenerativeBeliefState& prev_belief, int action, 
                           const GenerativeBeliefState& next_belief);
    
    // Predict sensory observation from belief state
    std::vector<float> predictSensory(const GenerativeBeliefState& belief) const;
    
    const std::array<MappingEntry, 8>& getIntentMappings() const { return intent_mappings_; }
    float getMappingConfidence(IntentClass intent) const { return intent_mappings_[static_cast<size_t>(intent)].confidence; }
    
    // Get EMA mapping vector for a specific intent and modality
    const std::vector<float>& getMapping(IntentClass intent, Modality modality) const {
        return intent_mappings_[static_cast<size_t>(intent)].modality_obs[static_cast<size_t>(modality)];
    }
    
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    GenerativeModelConfig config_;
    
    std::array<MappingEntry, 8> intent_mappings_;
    std::array<std::vector<float>, 8> transition_weights_;
    std::array<std::vector<float>, 8> transition_bias_;
    std::array<ModalityProcessor, 3> modality_processors_;
    
    void initializeProcessors();
    
    std::vector<float> inferHiddenState(const GenerativeBeliefState& belief) const;
    GenerativeBeliefState decodeHiddenState(const std::vector<float>& hidden) const;
};

} // namespace inference
} // namespace brain
} // namespace yuki