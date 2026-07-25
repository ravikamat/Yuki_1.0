#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>

namespace yuki { namespace causal {

using VariableId = size_t;
using Value = double;
using StructuralFunction = std::function<Value(const std::vector<Value>& parentValues, Value noise)>;

struct Variable {
    VariableId id = 0;
    std::string name;
    std::vector<VariableId> parents;
    StructuralFunction function;
    Value noiseMean = 0.0;
    Value noiseStd = 1.0;
    std::vector<Value> linearWeights; // for linear variables
};

struct Intervention {
    VariableId target = 0;
    Value value = 0.0;
};

struct Evidence {
    std::unordered_map<VariableId, Value> observations;
};

class StructuralCausalModel {
public:
    StructuralCausalModel();
    ~StructuralCausalModel();
    StructuralCausalModel(const StructuralCausalModel&) = delete;
    StructuralCausalModel& operator=(const StructuralCausalModel&) = delete;
    StructuralCausalModel(StructuralCausalModel&&) noexcept;
    StructuralCausalModel& operator=(StructuralCausalModel&&) noexcept;

    VariableId addVariable(const std::string& name, const std::vector<VariableId>& parents,
                           StructuralFunction function, Value noiseMean = 0.0, Value noiseStd = 1.0);

    VariableId addLinearVariable(const std::string& name, const std::vector<VariableId>& parents,
                                 const std::vector<Value>& weights, Value noiseMean = 0.0, Value noiseStd = 1.0);

    const Variable* getVariable(VariableId id) const;
    const Variable* getVariableByName(const std::string& name) const;
    size_t getVariableCount() const;

    std::unordered_map<VariableId, Value> solve(const std::vector<Value>& noiseValues) const;

    std::unordered_map<VariableId, Value> interveneAndSolve(const std::vector<Value>& noiseValues,
                                                            const Intervention& intervention) const;

    std::unordered_map<VariableId, Value> interveneAndSolve(const std::vector<Value>& noiseValues,
                                                            const std::vector<Intervention>& interventions) const;

    std::vector<Value> inferNoise(const Evidence& evidence) const;
    bool isAcyclic() const;
    std::vector<VariableId> getTopologicalOrder() const;

    // Binary serialization: magic = 0x53434D30 ('SCM0')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::causal
