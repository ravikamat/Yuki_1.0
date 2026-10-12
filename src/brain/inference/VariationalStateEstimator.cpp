#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/inference/BeliefState.h"
#include "brain/inference/PrecisionPredictor.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace yuki {
namespace brain {
namespace inference {

VariationalStateEstimator::VariationalStateEstimator(const VSEConfig& cfg)
    : config_(cfg) {
    // yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "VSE", "Initialized");
}

void VariationalStateEstimator::updateBeliefFromTextObs(const std::vector<float>& text_features,
                                                        float precision,
                                                        const std::string& text,
                                                        const std::string& prev_text,
                                                        const std::vector<std::string>& entities) {
    // Update belief based on text observation
    // This is a simplified implementation
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "VSE",
        "Updating belief from text observation");
}

float VariationalStateEstimator::trainPrecision(float predicted_precision, float target_precision,
                                                const std::string& text, const std::string& prev_text,
                                                const std::vector<std::string>& entities) {
    float alpha = 0.1f;
    float error = target_precision - predicted_precision;
    // Update precision predictor
    return alpha * error;
}

void VariationalStateEstimator::updateBelief(const std::vector<float>& prediction_error,
                                              const std::vector<float>& precision, float lr) {
    // Update belief using prediction error and precision
    // Simplified implementation
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "VSE",
        "Updating belief with prediction error");
}

void VariationalStateEstimator::serialize(std::ostream& os) const {
    std::string belief_str = belief_.serialize();
    std::string precision_str = precision_.serialize();
    
    // Write lengths and data
    os << belief_str.size() << ';';
    os << belief_str << ';';
    os << precision_str.size() << ';';
    os << precision_str << ';';
    
    // Serialize config
    os << config_.surprise_threshold << ';'
       << config_.entropy_threshold << ';'
       << config_.precision_floor << ';'
       << config_.precision_ceil << ';'
       << config_.recovery_rate << ';'
       << config_.baseline_intent << ';'
       << config_.baseline_entity << ';'
       << config_.baseline_tone << ';'
       << config_.baseline_source << ';'
       << config_.calibration_warmup_alpha << ';'
       << config_.calibration_steady_alpha << ';';
    
    // Serialize precision predictor weights if exists
    if (precision_predictor_) {
        os << "1;";
        os << precision_predictor_->serialize() << ';';
    } else {
        os << "0;";
    }
    
    // Serialize generative model reference (just a flag, actual model serialized separately)
    os << (generative_model_ ? "1;" : "0;");
}

bool VariationalStateEstimator::deserialize(std::istream& is) {
    // Read belief
    std::string token;
    if (!std::getline(is, token, ';')) return false;
    size_t belief_len = std::stoul(token);
    
    std::string belief_str(belief_len, '\0');
    is.read(&belief_str[0], belief_len);
    if (!belief_.deserialize(belief_str)) return false;
    
    // Read separator
    std::getline(is, token, ';');
    
    // Read precision
    if (!std::getline(is, token, ';')) return false;
    size_t precision_len = std::stoul(token);
    
    std::string precision_str(precision_len, '\0');
    is.read(&precision_str[0], precision_len);
    if (!precision_.deserialize(precision_str)) return false;
    
    // Read separator
    std::getline(is, token, ';');
    
    // Read config
    if (!std::getline(is, token, ';')) return false; config_.surprise_threshold = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.entropy_threshold = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.precision_floor = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.precision_ceil = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.recovery_rate = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.baseline_intent = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.baseline_entity = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.baseline_tone = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.baseline_source = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.calibration_warmup_alpha = std::stof(token);
    if (!std::getline(is, token, ';')) return false; config_.calibration_steady_alpha = std::stof(token);
    
    // Precision predictor
    if (!std::getline(is, token, ';')) return false;
    if (token == "1") {
        std::string pred_data;
        std::getline(is, token, ';');
        // In production, would deserialize predictor here
    }
    
    // Generative model reference
    if (!std::getline(is, token, ';')) return false;
    // Note: generative_model_ is set externally
    
    return true;
}

std::string VariationalStateEstimator::serialize() const {
    std::ostringstream oss;
    serialize(oss);
    return oss.str();
}

bool VariationalStateEstimator::deserialize(const std::string& data) {
    std::istringstream iss(data);
    return deserialize(iss);
}

} // namespace inference
} // namespace brain
} // namespace yuki

// PrecisionState serialization methods
namespace yuki {
namespace brain {
namespace inference {

// ── PrecisionState dimension accessors + [D2] update rule ────────────────────
// Mirrors the rule used by yuki::PrecisionState (TurnCoordinator): the driver
// is stream disagreement, with extreme prediction error as a secondary penalty.
float PrecisionState::get(const std::string& dim) const {
    if (dim == "intent") return intent;
    if (dim == "entity") return entity;
    if (dim == "tone")   return tone;
    if (dim == "safety") return safety;
    if (dim == "source") return source;
    return 0.5f;
}

void PrecisionState::set(const std::string& dim, float value) {
    if      (dim == "intent") intent = value;
    else if (dim == "entity") entity = value;
    else if (dim == "tone")   tone   = value;
    else if (dim == "safety") safety = value;
    else if (dim == "source") source = value;
}

void PrecisionState::update(const std::string& dimension, float prediction_error,
                            float stream_agreement) {
    constexpr float kFloor = 0.05f;   // constants::PRECISION_FLOOR
    constexpr float kCeil  = 0.99f;   // constants::PRECISION_CEIL
    const float disagreement_penalty = std::max(0.0f, 0.85f - stream_agreement) * 0.5f;
    const float error_penalty        = std::max(0.0f, prediction_error - 0.60f) * 0.3f;
    const float updated = get(dimension) - (disagreement_penalty + error_penalty);
    set(dimension, std::clamp(updated, kFloor, kCeil));
}

void PrecisionState::recover() {
    // Drift non-safety dimensions toward the neutral prior; safety never
    // auto-recovers (only an explicit reset can raise it again).
    constexpr float kPrior = 0.5f;
    constexpr float kRate  = 0.05f;
    auto drift = [](float v, float base, float rate) {
        return v < base ? std::min(v + rate, base) : v;
    };
    intent = drift(intent, kPrior, kRate);
    entity = drift(entity, kPrior, kRate);
    tone   = drift(tone,   kPrior, kRate);
    source = drift(source, kPrior, kRate);
}

std::string PrecisionState::serialize() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << intent << ';' << entity << ';' << tone << ';' << safety << ';' << source;
    return oss.str();
}

bool PrecisionState::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    if (!std::getline(iss, token, ';')) return false; intent = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; entity = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; tone = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; safety = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; source = std::stof(token);
    return true;
}

void PrecisionState::serialize(std::ostream& os) const {
    os << serialize();
}

bool PrecisionState::deserialize(std::istream& is) {
    std::string data((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    return deserialize(data);
}

} // namespace inference
} // namespace brain
} // namespace yuki