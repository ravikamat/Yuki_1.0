#pragma once

#include <array>
#include <vector>
#include <chrono>
#include <memory>
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/BeliefState.h"
#include "brain/inference/PrecisionPredictor.h"
#include "brain/infra/CognitiveConfig.h"

namespace yuki {
namespace brain {
namespace inference {

struct PrecisionState {
    float intent = 0.5f;
    float entity = 0.5f;
    float tone = 0.5f;
    float safety = 0.5f;
    float source = 0.5f;
    
    void update(const std::string& dimension, float prediction_error, float stream_agreement);
    void recover();
    float get(const std::string& dim) const;
    void set(const std::string& dim, float value);
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);
    void serialize(std::ostream& os) const;
    bool deserialize(std::istream& is);
};

class VariationalStateEstimator {
public:
    explicit VariationalStateEstimator(const yuki::brain::infra::CognitiveConfig::VSEConfig& cfg = yuki::brain::infra::CognitiveConfig::VSEConfig{});
    ~VariationalStateEstimator() = default;
    
    void setGenerativeModel(GenerativeModel* gm) { generative_model_ = gm; }
    GenerativeModel& generativeModel() { return *generative_model_; }
    const GenerativeModel& generativeModel() const { return *generative_model_; }
    
    const BeliefState& currentBelief() const { return belief_; }
    BeliefState& currentBelief() { return belief_; }
    void setBeliefState(const BeliefState& belief) { belief_ = belief; }
    
    void updateBeliefFromTextObs(const std::vector<float>& text_features,
                                 float precision, const std::string& text,
                                 const std::string& prev_text,
                                 const std::vector<std::string>& entities);
    float trainPrecision(float predicted_precision, float target_precision,
                         const std::string& text, const std::string& prev_text,
                         const std::vector<std::string>& entities);
    void updateBelief(const std::vector<float>& prediction_error,
                      const std::vector<float>& precision, float lr);
    
    const PrecisionState& precision() const { return precision_; }
    PrecisionState& precision() { return precision_; }
    
    PrecisionPredictor* precisionPredictor() { return precision_predictor_.get(); }
    void setPrecisionPredictor(std::unique_ptr<PrecisionPredictor> predictor);
    
    float getSurprise() const { return belief_.surprise_budget; }
    float getEntropy() const { return belief_.entropy(); }
    float getFreeEnergy() const { return belief_.free_energy; }
    
    float getSurpriseBudget() const { return belief_.surprise_budget; }
    float getSafetyMass() const { return belief_.safety_mass; }
    
    void serialize(std::ostream& os) const;
    bool deserialize(std::istream& is);
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    BeliefState belief_;
    PrecisionState precision_;
    yuki::brain::infra::CognitiveConfig::VSEConfig config_;
    std::unique_ptr<PrecisionPredictor> precision_predictor_;
    GenerativeModel* generative_model_ = nullptr;
    
    void updatePrecision(const std::string& dimension, float prediction_error, float agreement);
    float computeEntropy() const;
    float computeFreeEnergy() const;
};

} // namespace inference
} // namespace brain
} // namespace yuki