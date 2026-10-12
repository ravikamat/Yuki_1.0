#include "MetacognitionEngine.h"
#include "brain/inference/PrecisionPredictor.h"
#include "brain/inference/VariationalStateEstimator.h"  // full brain::inference::PrecisionPredictor interface
#include "brain/organism/DriveSystem.h"
#include "brain/system/BackgroundJobEngine.h"
#include "brain/metacognition/ImprovementGraph.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/synthesis/CodeSynthesisAgent.h"
#include "brain/memory/EpisodicStore.h"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <chrono>


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
    const yuki::brain::inference::PrecisionPredictor* predictor)
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

void MetacognitionEngine::setImprovementGraph(ImprovementGraph* graph) {
    std::lock_guard<std::mutex> lock(mutex_);
    improvement_graph_ = graph;
}

ImprovementGraph* MetacognitionEngine::getImprovementGraph() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return improvement_graph_;
}

void MetacognitionEngine::setResearchPlanner(yuki::research::ResearchPlanner* planner) {
    std::lock_guard<std::mutex> lock(mutex_);
    research_planner_ = planner;
}

yuki::research::ResearchPlanner* MetacognitionEngine::getResearchPlanner() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return research_planner_;
}

void MetacognitionEngine::setCodeSynthesisAgent(yuki::synthesis::CodeSynthesisAgent* agent) {
    std::lock_guard<std::mutex> lock(mutex_);
    code_synthesis_agent_ = agent;
}

yuki::synthesis::CodeSynthesisAgent* MetacognitionEngine::getCodeSynthesisAgent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return code_synthesis_agent_;
}

bool MetacognitionEngine::triggerResearch(const std::string& gap_description) {
    std::lock_guard<std::mutex> lock(mutex_);

    float avg_comp = 0.0f;
    for (const auto& c : competence_) {
        avg_comp += c.success_rate_ema;
    }
    avg_comp /= static_cast<float>(competence_.size());

    float curiosity = 0.0f;
    if (drive_system_) {
        curiosity = static_cast<float>(drive_system_->curiosityDeficit());
    }

    if (avg_comp < 0.6f && curiosity > 0.7f) {
        uint64_t now_ts = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());

        float prio = (1.0f - avg_comp) * curiosity + 0.1f;
        learning_curriculum_.push(CurriculumTopic{gap_description, prio, avg_comp, now_ts});

        if (research_planner_) {
            research_planner_->research(gap_description);
        }

        if (improvement_graph_) {
            improvement_graph_->triggerResearchRoute(gap_description);
        }

        return true;
    }
    return false;
}

void MetacognitionEngine::buildLearningCurriculum(const std::vector<std::string>& gaps) {
    std::lock_guard<std::mutex> lock(mutex_);

    float avg_comp = 0.0f;
    for (const auto& c : competence_) {
        avg_comp += c.success_rate_ema;
    }
    avg_comp /= static_cast<float>(competence_.size());

    float curiosity = drive_system_ ? static_cast<float>(drive_system_->curiosityDeficit()) : 0.5f;
    uint64_t now_ts = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    for (const auto& gap : gaps) {
        float prio = (1.0f - avg_comp) * curiosity + 0.05f * static_cast<float>(gap.length() % 10);
        learning_curriculum_.push(CurriculumTopic{gap, prio, avg_comp, now_ts});
    }
}

bool MetacognitionEngine::requestCodeSynthesis(const std::string& gap) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (code_synthesis_agent_) {
        yuki::synthesis::SynthesisSpec spec;
        spec.existing_content = gap;
        spec.target_module_name = "generated_tool_" + std::to_string(std::hash<std::string>{}(gap) % 10000);
        code_synthesis_agent_->synthesize(spec);
    }

    if (improvement_graph_) {
        improvement_graph_->requestCodeSynthesisRoute(gap);
    }

    return true;
}

std::vector<CurriculumTopic> MetacognitionEngine::getCurriculumTopics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CurriculumTopic> topics;
    auto copy_q = learning_curriculum_;
    while (!copy_q.empty()) {
        topics.push_back(copy_q.top());
        copy_q.pop();
    }
    return topics;
}

void MetacognitionEngine::observeFreeEnergy(float free_energy,
                                              const yuki::brain::inference::PrecisionPredictor* predictor,
                                              ImprovementGraph* graph) {
    std::lock_guard<std::mutex> lock(mutex_);

    const float f_threshold = 5.0f;
    if (free_energy <= f_threshold) return;

    // Detect stagnation: check if predictor weights are stagnant or meta-precision EMA is low
    bool stagnant = false;
    if (predictor) {
        stagnant = detectStagnation(predictor->serialize());
    }

    const auto& metaPrec = competence_[static_cast<size_t>(CompetenceDomain::META_PRECISION)];
    if (metaPrec.sample_count > 5 && metaPrec.success_rate_ema < 0.4f) {
        stagnant = true;
    }

    // High Free Energy + Stagnation -> flag "ignorance gap" in ImprovementGraph & activeHypotheses
    if (stagnant) {
        Hypothesis h{
            CompetenceDomain::META_PRECISION,
            SymptomCode::KNOWLEDGE_GAP,
            ExperimentType::EXPAND_TRAINING,
            std::clamp((free_energy - f_threshold) / 10.0f + 0.5f, 0.5f, 1.0f),
            computeConfidence(CompetenceDomain::META_PRECISION)
        };
        activeHypotheses_.push_back(h);

        ImprovementGraph* targetGraph = graph ? graph : improvement_graph_;
        if (targetGraph) {
            targetGraph->addActionRoute(SymptomCode::KNOWLEDGE_GAP, "ignorance_gap_curiosity_expansion");
            targetGraph->query(SymptomCode::KNOWLEDGE_GAP, static_cast<uint32_t>(CompetenceDomain::META_PRECISION), metaPrec.success_rate_ema, 0);
        }
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

void MetacognitionEngine::processSelfReflectionText(const std::string& reflection_text) {
    if (reflection_text.empty()) return;

    std::string text_lower = reflection_text;
    std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(), ::tolower);

    static const std::vector<std::string> gap_triggers = {
        "uncertain", "ignorance", "gap", "failed", "struggle", "confused", "unknown", "error"
    };

    for (const auto& kw : gap_triggers) {
        if (text_lower.find(kw) != std::string::npos) {
            std::string gap_topic = "Metacognitive reflection gap: " + kw + " detected in '" + reflection_text.substr(0, 60) + "...'";
            triggerResearch(gap_topic);
            break;
        }
    }
}

void MetacognitionEngine::processSelfReflections(yuki::brain::memory::EpisodicStore* store) {
    if (!store) return;
    auto reflections = store->queryRecentSnapshots(10);
    for (const auto& ep : reflections) {
        processSelfReflectionText(ep.text);
    }
}

} // namespace yuki::metacognition
