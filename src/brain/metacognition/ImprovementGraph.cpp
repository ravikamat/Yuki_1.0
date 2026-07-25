#include "ImprovementGraph.h"
#include <algorithm>

namespace yuki {
namespace metacognition {

ImprovementGraph::ImprovementGraph() {
    initializeEdges();
}

void ImprovementGraph::initializeEdges() {
    edges_.push_back({SymptomCode::PRECISION_TOO_HIGH,
                      ExperimentType::REWIRE_FEATURES, 1, 0, 0.15f, 0.6f});
    edges_.push_back({SymptomCode::PRECISION_TOO_HIGH,
                      ExperimentType::ADJUST_LR, 1, 0, 0.08f, 0.5f});
    edges_.push_back({SymptomCode::FEATURE_STAGNATION,
                      ExperimentType::REWIRE_FEATURES, 1, 0, 0.20f, 0.5f});
    edges_.push_back({SymptomCode::INTENT_CONFUSION,
                      ExperimentType::EXPAND_TRAINING, 1, 0, 0.10f, 0.4f});
    edges_.push_back({SymptomCode::COMPETENCE_DEGRADATION,
                      ExperimentType::TRIGGER_SLEEP, 3, 1, 0.12f, 0.5f});
    edges_.push_back({SymptomCode::SYNTHESIS_FAILURE,
                      ExperimentType::EXPAND_TRAINING, 1, 0, 0.18f, 0.55f});

    for (const auto& e : edges_) {
        confidence_map_[key(e.symptom, e.experiment)] = e.base_confidence;
    }
}

std::vector<ActionableHypothesis> ImprovementGraph::query(
    SymptomCode symptom,
    uint32_t affected_domain,
    float current_competence,
    uint64_t trigger_audit_id) const {

    std::vector<ActionableHypothesis> results;

    for (const auto& e : edges_) {
        if (e.symptom != symptom) continue;

        ActionableHypothesis ah;
        ah.symptom = e.symptom;
        ah.experiment = e.experiment;
        ah.target_module_id = e.target_module_id;
        ah.target_domain = affected_domain;
        ah.expected_competence_delta = e.expected_competence_delta;
        ah.trigger_audit_id = trigger_audit_id;

        auto it = confidence_map_.find(key(e.symptom, e.experiment));
        float conf = (it != confidence_map_.end()) ? it->second : e.base_confidence;
        ah.action_confidence = conf;

        ah.priority_score = (1.0f - current_competence) *
                            ah.expected_competence_delta *
                            ah.action_confidence;

        results.push_back(ah);
    }

    std::sort(results.begin(), results.end(),
              [](const ActionableHypothesis& a, const ActionableHypothesis& b) {
                  return a.priority_score > b.priority_score;
              });

    return results;
}

void ImprovementGraph::feedback(SymptomCode symptom, ExperimentType experiment, bool success) {
    uint32_t k = key(symptom, experiment);
    auto it = confidence_map_.find(k);
    if (it == confidence_map_.end()) return;

    float alpha = 0.1f;
    if (success) {
        it->second = it->second * (1.0f - alpha) + alpha * 1.0f;
    } else {
        it->second = it->second * (1.0f - alpha) + alpha * 0.0f;
    }
}

std::vector<ImprovementEdge> ImprovementGraph::edgesFor(SymptomCode symptom) const {
    std::vector<ImprovementEdge> result;
    for (const auto& e : edges_) {
        if (e.symptom == symptom) result.push_back(e);
    }
    return result;
}

void ImprovementGraph::addChainRoute(const std::string& name, const std::string& route) {
    chain_routes_[name] = route;
}

bool ImprovementGraph::hasChainRoute(const std::string& name) const {
    return chain_routes_.find(name) != chain_routes_.end();
}

std::string ImprovementGraph::getChainRoute(const std::string& name) const {
    auto it = chain_routes_.find(name);
    return (it != chain_routes_.end()) ? it->second : "";
}

void ImprovementGraph::addIntrospectionRoute(const std::string& name, const std::string& route) {
    introspection_routes_[name] = route;
}

bool ImprovementGraph::hasIntrospectionRoute(const std::string& name) const {
    return introspection_routes_.find(name) != introspection_routes_.end();
}

std::string ImprovementGraph::getIntrospectionRoute(const std::string& name) const {
    auto it = introspection_routes_.find(name);
    return (it != introspection_routes_.end()) ? it->second : "";
}

void ImprovementGraph::addActionRoute(SymptomCode symptom, const std::string& actionTag) {
    action_routes_[symptom] = actionTag;
}

bool ImprovementGraph::hasActionRoute(SymptomCode symptom) const {
    return action_routes_.find(symptom) != action_routes_.end();
}

std::string ImprovementGraph::getActionRoute(SymptomCode symptom) const {
    auto it = action_routes_.find(symptom);
    return (it != action_routes_.end()) ? it->second : "";
}

uint32_t ImprovementGraph::key(SymptomCode s, ExperimentType e) {
    return (static_cast<uint32_t>(s) << 8) | static_cast<uint32_t>(e);
}

} // namespace metacognition
} // namespace yuki
