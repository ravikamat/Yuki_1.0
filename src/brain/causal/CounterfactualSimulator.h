#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "brain/causal/StructuralCausalModel.h"

namespace yuki { namespace causal {

struct CounterfactualQuery {
    VariableId targetY = 0;
    VariableId interventionX = 0;
    Value interventionValue = 0.0;
    Evidence evidence;
};

struct CounterfactualResult {
    Value predictedY = 0.0;
    Value factualY = 0.0;
    Value effect = 0.0;
    std::vector<Value> inferredNoise;
    bool valid = true;
    std::string errorMessage;
};

struct RegretAnalysis {
    VariableId bestIntervention = 0;
    Value bestValue = 0.0;
    Value bestOutcome = 0.0;
    std::vector<std::pair<Intervention, Value>> outcomes;
};

class CounterfactualSimulator {
public:
    CounterfactualSimulator();
    ~CounterfactualSimulator();
    CounterfactualSimulator(const CounterfactualSimulator&) = delete;
    CounterfactualSimulator& operator=(const CounterfactualSimulator&) = delete;
    CounterfactualSimulator(CounterfactualSimulator&&) noexcept;
    CounterfactualSimulator& operator=(CounterfactualSimulator&&) noexcept;

    void setModel(std::shared_ptr<StructuralCausalModel> model);
    std::shared_ptr<StructuralCausalModel> getModel() const;

    CounterfactualResult simulate(const CounterfactualQuery& query);
    std::vector<CounterfactualResult> simulateBatch(VariableId targetY, VariableId interventionX,
                                                      const std::vector<Value>& possibleValues,
                                                      const Evidence& evidence);

    RegretAnalysis analyzeRegret(VariableId targetY, const std::vector<Intervention>& possibleInterventions,
                                  const Evidence& evidence);

    CounterfactualResult whatIf(VariableId outcomeVariable, const Intervention& counterfactualAction,
                                 const Evidence& actualEvidence);

    Value computeATE(VariableId treatment, VariableId outcome, Value treatmentOn, Value treatmentOff,
                     size_t numSamples = 1000);

    // Binary serialization: magic = 0x43465330 ('CFS0')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::causal
