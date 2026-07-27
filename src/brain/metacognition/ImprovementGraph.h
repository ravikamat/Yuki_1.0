#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "Hypothesis.h"
#include "HypothesisConsumer.h"

namespace yuki {
namespace metacognition {

struct ImprovementEdge {
    SymptomCode symptom = SymptomCode::NONE;
    ExperimentType experiment = ExperimentType::NONE;
    uint32_t target_module_id = 0;
    uint32_t target_domain = 0;
    float expected_competence_delta = 0.0f;
    float base_confidence = 0.5f;
};

class ImprovementGraph {
public:
    ImprovementGraph();

    std::vector<ActionableHypothesis> query(
        SymptomCode symptom,
        uint32_t affected_domain,
        float current_competence,
        uint64_t trigger_audit_id) const;

    void feedback(SymptomCode symptom, ExperimentType experiment, bool success);
    std::vector<ImprovementEdge> edgesFor(SymptomCode symptom) const;

    void addChainRoute(const std::string& name, const std::string& route);
    bool hasChainRoute(const std::string& name) const;
    std::string getChainRoute(const std::string& name) const;

    void addIntrospectionRoute(const std::string& name, const std::string& route);
    bool hasIntrospectionRoute(const std::string& name) const;
    std::string getIntrospectionRoute(const std::string& name) const;

    // M4: Action routing
    void addActionRoute(SymptomCode symptom, const std::string& actionTag);
    bool hasActionRoute(SymptomCode symptom) const;
    std::string getActionRoute(SymptomCode symptom) const;

private:
    std::vector<ImprovementEdge> edges_;
    std::map<uint32_t, float> confidence_map_;
    std::map<std::string, std::string> chain_routes_;
    std::map<std::string, std::string> introspection_routes_;
    std::map<SymptomCode, std::string> action_routes_;

    static uint32_t key(SymptomCode s, ExperimentType e);
    void initializeEdges();
};

} // namespace metacognition
} // namespace yuki
