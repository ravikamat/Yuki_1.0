#include "MetacognitionEngine.h"
#include "brain/inference/PrecisionPredictor.h"
#include "brain/organism/DriveSystem.h"
#include "brain/system/BackgroundJobEngine.h"
#include <sstream>
#include <cmath>
#include <algorithm>


namespace yuki::metacognition {

MetacognitionEngine::~MetacognitionEngine() = default;

MetacognitionEngine::MetacognitionEngine() {
    competence_.fill(CompetenceRecord{});
}

void MetacognitionEngine::setDriveSystem(yuki::organism::DriveSystem* ptr) {
    drive_system_.reset(ptr);
}

bool MetacognitionEngine::evaluateSuccess(const TurnOutcome& outcome) const {
    // Success = direct response without clarification or correction
    if (outcome.clarification_triggered) return false;
    if (outcome.user_corrected) return false;
    if (outcome.actual_response_family.find("clarif") != std::string::npos) return false;
    if (outcome.actual_response_family.find("veto") != std::string::npos) return false;
    return true;
}

void MetacognitionEngine::generateHypothesis(CompetenceDomain domain, const TurnOutcome& outcome) {
    Hypothesis h{
        domain,
        SymptomCode::COMPETENCE_DEGRADATION,
        ExperimentType::EXPAND_TRAINING,
        computePriority(domain, outcome),
        computeConfidence(domain)
    };
    activeHypotheses_.push_back(h);
}

void MetacognitionEngine::observeTurnOutcome(const TurnOutcome& outcome) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Map predicted intent to domain
    CompetenceDomain domain = static_cast<CompetenceDomain>(
        std::min(static_cast<uint8_t>(CompetenceDomain::INTENT_POLARITY),
                 outcome.predicted_intent));

    bool success = evaluateSuccess(outcome);
    competence_[static_cast<size_t>(domain)].update(success);

    // Also update meta-precision domain
    CompetenceRecord& metaPrec = competence_[static_cast<size_t>(CompetenceDomain::META_PRECISION)];
    bool precisionSuccess = !outcome.clarification_triggered;
    metaPrec.update(precisionSuccess);

    // Generate hypothesis if competence degraded significantly
    float currentEma = competence_[static_cast<size_t>(domain)].success_rate_ema;
    if (competence_[static_cast<size_t>(domain)].sample_count > 5 && currentEma < 0.4f) {
        generateHypothesis(domain, outcome);
    }

    // Generate hypothesis for precision if degraded
    if (metaPrec.sample_count > 5 && metaPrec.success_rate_ema < 0.4f) {
        SymptomCode sym = outcome.clarification_triggered
            ? SymptomCode::PRECISION_TOO_HIGH
            : SymptomCode::PRECISION_TOO_LOW;
        Hypothesis h{
            CompetenceDomain::META_PRECISION,
            sym,
            ExperimentType::ADJUST_LR,
            computePriority(CompetenceDomain::META_PRECISION, outcome),
            computeConfidence(CompetenceDomain::META_PRECISION)
        };
        activeHypotheses_.push_back(h);
    }
}

void MetacognitionEngine::observePrecisionPredictor(
    const yuki::inference::PrecisionPredictor* predictor)
{
    if (!predictor) return;
    std::lock_guard<std::mutex> lock(mutex_);

    std::string weights = predictor->serialize();
    if (detectStagnation(weights)) {
        Hypothesis h{
            CompetenceDomain::META_PRECISION,
            SymptomCode::FEATURE_STAGNATION,
            ExperimentType::REWIRE_FEATURES,
            0.7f,
            computeConfidence(CompetenceDomain::META_PRECISION)
        };
        activeHypotheses_.push_back(h);
    }
}

const CompetenceRecord& MetacognitionEngine::getCompetence(CompetenceDomain domain) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return competence_[static_cast<size_t>(domain)];
}

std::vector<Hypothesis> MetacognitionEngine::getActiveHypotheses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeHypotheses_;
}

void MetacognitionEngine::clearHypotheses() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeHypotheses_.clear();
}

float MetacognitionEngine::computePriority(CompetenceDomain domain,
                                           const TurnOutcome& outcome) const {
    const auto& rec = competence_[static_cast<size_t>(domain)];
    if (rec.sample_count < 2) return 0.5f;
    // Priority = inverse of EMA, scaled by entropy (higher entropy = more urgent)
    float basePriority = 1.0f - rec.success_rate_ema;
    float entropyFactor = std::min(1.0f, outcome.belief_entropy / 2.0f);
    return std::clamp(basePriority * (1.0f + entropyFactor), 0.0f, 1.0f);
}

float MetacognitionEngine::computeConfidence(CompetenceDomain domain) const {
    const auto& rec = competence_[static_cast<size_t>(domain)];
    // Confidence increases with sample count, saturating around 100 samples
    return std::clamp(1.0f - std::exp(-static_cast<float>(rec.sample_count) / 30.0f), 0.0f, 1.0f);
}

bool MetacognitionEngine::detectStagnation(const std::string& serializedWeights) const {
    // Minimal JSON parser: check if all weights are near zero after many samples
    size_t bracket = serializedWeights.find('[');
    size_t endBracket = serializedWeights.find(']', bracket);
    if (bracket == std::string::npos || endBracket == std::string::npos) return false;

    std::string nums = serializedWeights.substr(bracket + 1, endBracket - bracket - 1);
    std::istringstream iss(nums);
    float val = 0.0f;
    float sumAbs = 0.0f;
    size_t count = 0;
    while (iss >> val) {
        sumAbs += std::abs(val);
        count++;
        if (iss.peek() == ',') iss.ignore();
    }
    if (count == 0) return false;
    float avgAbs = sumAbs / static_cast<float>(count);
    return avgAbs < 0.01f;
}

std::string MetacognitionEngine::serializeCompetence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{\"competence\":[";
    for (size_t i = 0; i < competence_.size(); ++i) {
        const auto& rec = competence_[i];
        oss << "{\"domain\":" << i
            << ",\"ema\":" << rec.success_rate_ema
            << ",\"samples\":" << rec.sample_count
            << ",\"success\":" << rec.success_count
            << ",\"failure\":" << rec.failure_count << "}";
        if (i + 1 < competence_.size()) oss << ",";
    }
    oss << "]}";
    return oss.str();
}

void MetacognitionEngine::deserializeCompetence(const std::string& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t pos = 0;
    while ((pos = json.find("\"domain\":", pos)) != std::string::npos) {
        pos += 9;
        size_t comma = json.find(',', pos);
        int domainIdx = std::stoi(json.substr(pos, comma - pos));
        if (domainIdx < 0 || domainIdx >= static_cast<int>(competence_.size())) continue;

        auto extract = [&](const std::string& key) -> float {
            size_t k = json.find("\"" + key + "\":", pos);
            if (k == std::string::npos) return 0.0f;
            k += key.size() + 3;
            size_t end = json.find_first_of(",}", k);
            return std::stof(json.substr(k, end - k));
        };

        competence_[domainIdx].success_rate_ema = extract("ema");
        competence_[domainIdx].sample_count = static_cast<uint64_t>(extract("samples"));
        competence_[domainIdx].success_count = static_cast<uint64_t>(extract("success"));
        competence_[domainIdx].failure_count = static_cast<uint64_t>(extract("failure"));
        pos = comma;
    }
}

void MetacognitionEngine::setBackgroundJobEngine(yuki::system::BackgroundJobEngine* ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    job_engine_.reset(ptr);
}

} // namespace yuki::metacognition
