#include "brain/causal/StructuralCausalModel.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <cstring>
#include <queue>
#include <stack>

namespace yuki { namespace causal {

class StructuralCausalModel::Impl {
public:
    std::vector<Variable> variables_;
    std::unordered_map<std::string, VariableId> nameToId_;

    Impl() = default;

    std::vector<VariableId> getTopologicalOrder() const {
        size_t n = variables_.size();
        std::vector<int> inDegree(n, 0);
        std::vector<std::vector<VariableId>> adj(n);

        for (size_t i = 0; i < n; ++i) {
            for (VariableId parent : variables_[i].parents) {
                if (parent < n) {
                    adj[parent].push_back(i);
                    inDegree[i]++;
                }
            }
        }

        std::queue<VariableId> q;
        for (size_t i = 0; i < n; ++i) {
            if (inDegree[i] == 0) q.push(i);
        }

        std::vector<VariableId> order;
        order.reserve(n);

        while (!q.empty()) {
            VariableId u = q.front();
            q.pop();
            order.push_back(u);

            for (VariableId v : adj[u]) {
                inDegree[v]--;
                if (inDegree[v] == 0) {
                    q.push(v);
                }
            }
        }

        return order;
    }

    bool isAcyclic() const {
        return getTopologicalOrder().size() == variables_.size();
    }
};

StructuralCausalModel::StructuralCausalModel() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "StructuralCausalModel initialized");
}

StructuralCausalModel::~StructuralCausalModel() = default;

StructuralCausalModel::StructuralCausalModel(StructuralCausalModel&&) noexcept = default;
StructuralCausalModel& StructuralCausalModel::operator=(StructuralCausalModel&&) noexcept = default;

VariableId StructuralCausalModel::addVariable(const std::string& name,
                                              const std::vector<VariableId>& parents,
                                              StructuralFunction function,
                                              Value noiseMean, Value noiseStd) {
    Variable v;
    v.id = pImpl->variables_.size();
    v.name = name;
    v.parents = parents;
    v.function = function;
    v.noiseMean = noiseMean;
    v.noiseStd = noiseStd;

    pImpl->variables_.push_back(v);
    pImpl->nameToId_[name] = v.id;
    return v.id;
}

VariableId StructuralCausalModel::addLinearVariable(const std::string& name,
                                                    const std::vector<VariableId>& parents,
                                                    const std::vector<Value>& weights,
                                                    Value noiseMean, Value noiseStd) {
    auto linearFn = [weights](const std::vector<Value>& pVals, Value noise) -> Value {
        Value sum = noise;
        for (size_t i = 0; i < pVals.size() && i < weights.size(); ++i) {
            sum += weights[i] * pVals[i];
        }
        return sum;
    };

    VariableId id = addVariable(name, parents, linearFn, noiseMean, noiseStd);
    pImpl->variables_[id].linearWeights = weights;
    return id;
}

const Variable* StructuralCausalModel::getVariable(VariableId id) const {
    if (id < pImpl->variables_.size()) return &pImpl->variables_[id];
    return nullptr;
}

const Variable* StructuralCausalModel::getVariableByName(const std::string& name) const {
    auto it = pImpl->nameToId_.find(name);
    if (it != pImpl->nameToId_.end()) return getVariable(it->second);
    return nullptr;
}

size_t StructuralCausalModel::getVariableCount() const {
    return pImpl->variables_.size();
}

bool StructuralCausalModel::isAcyclic() const {
    return pImpl->isAcyclic();
}

std::vector<VariableId> StructuralCausalModel::getTopologicalOrder() const {
    return pImpl->getTopologicalOrder();
}

std::unordered_map<VariableId, Value> StructuralCausalModel::solve(const std::vector<Value>& noiseValues) const {
    return interveneAndSolve(noiseValues, std::vector<Intervention>{});
}

std::unordered_map<VariableId, Value> StructuralCausalModel::interveneAndSolve(const std::vector<Value>& noiseValues,
                                                                               const Intervention& intervention) const {
    return interveneAndSolve(noiseValues, std::vector<Intervention>{intervention});
}

std::unordered_map<VariableId, Value> StructuralCausalModel::interveneAndSolve(const std::vector<Value>& noiseValues,
                                                                               const std::vector<Intervention>& interventions) const {
    std::unordered_map<VariableId, Value> solution;
    auto order = pImpl->getTopologicalOrder();

    std::unordered_map<VariableId, Value> interMap;
    for (const auto& in : interventions) {
        interMap[in.target] = in.value;
    }

    for (VariableId id : order) {
        auto it = interMap.find(id);
        if (it != interMap.end()) {
            solution[id] = it->second;
        } else {
            const auto& var = pImpl->variables_[id];
            std::vector<Value> pVals;
            pVals.reserve(var.parents.size());
            for (VariableId pid : var.parents) {
                pVals.push_back(solution[pid]);
            }
            Value noise = (id < noiseValues.size()) ? noiseValues[id] : var.noiseMean;
            if (var.function) {
                solution[id] = var.function(pVals, noise);
            } else {
                solution[id] = noise;
            }
        }
    }

    return solution;
}

std::vector<Value> StructuralCausalModel::inferNoise(const Evidence& evidence) const {
    std::vector<Value> noise(pImpl->variables_.size(), 0.0);
    auto order = pImpl->getTopologicalOrder();

    for (VariableId id : order) {
        const auto& var = pImpl->variables_[id];
        auto it = evidence.observations.find(id);
        if (it != evidence.observations.end()) {
            Value observed = it->second;
            Value parentSum = 0.0;
            for (size_t i = 0; i < var.parents.size() && i < var.linearWeights.size(); ++i) {
                VariableId pid = var.parents[i];
                auto pit = evidence.observations.find(pid);
                if (pit != evidence.observations.end()) {
                    parentSum += var.linearWeights[i] * pit->second;
                }
            }
            noise[id] = observed - parentSum;
        } else {
            noise[id] = var.noiseMean;
        }
    }

    return noise;
}

std::vector<uint8_t> StructuralCausalModel::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x53434D30; // 'SCM0'
    uint32_t count = static_cast<uint32_t>(pImpl->variables_.size());

    buf.resize(8);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &count, 4);

    for (const auto& var : pImpl->variables_) {
        uint32_t nameLen = static_cast<uint32_t>(var.name.size());
        uint32_t parentCount = static_cast<uint32_t>(var.parents.size());
        uint32_t weightCount = static_cast<uint32_t>(var.linearWeights.size());

        size_t off = buf.size();
        buf.resize(off + 12 + nameLen + parentCount * sizeof(VariableId) + weightCount * sizeof(Value) + 2 * sizeof(Value));

        std::memcpy(buf.data() + off, &nameLen, 4);
        std::memcpy(buf.data() + off + 4, &parentCount, 4);
        std::memcpy(buf.data() + off + 8, &weightCount, 4);
        off += 12;

        std::memcpy(buf.data() + off, var.name.data(), nameLen);
        off += nameLen;

        if (parentCount > 0) {
            std::memcpy(buf.data() + off, var.parents.data(), parentCount * sizeof(VariableId));
            off += parentCount * sizeof(VariableId);
        }

        if (weightCount > 0) {
            std::memcpy(buf.data() + off, var.linearWeights.data(), weightCount * sizeof(Value));
            off += weightCount * sizeof(Value);
        }

        std::memcpy(buf.data() + off, &var.noiseMean, sizeof(Value));
        std::memcpy(buf.data() + off + sizeof(Value), &var.noiseStd, sizeof(Value));
    }

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool StructuralCausalModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0, count = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x53434M30) return false;

    std::memcpy(&count, data.data() + 4, 4);
    pImpl->variables_.clear();
    pImpl->nameToId_.clear();

    size_t cursor = 8;
    for (size_t i = 0; i < count; ++i) {
        if (cursor + 12 > payload_len) return false;
        uint32_t nameLen = 0, parentCount = 0, weightCount = 0;
        std::memcpy(&nameLen, data.data() + cursor, 4);
        std::memcpy(&parentCount, data.data() + cursor + 4, 4);
        std::memcpy(&weightCount, data.data() + cursor + 8, 4);
        cursor += 12;

        std::string name(reinterpret_cast<const char*>(data.data() + cursor), nameLen);
        cursor += nameLen;

        std::vector<VariableId> parents(parentCount);
        if (parentCount > 0) {
            std::memcpy(parents.data(), data.data() + cursor, parentCount * sizeof(VariableId));
            cursor += parentCount * sizeof(VariableId);
        }

        std::vector<Value> weights(weightCount);
        if (weightCount > 0) {
            std::memcpy(weights.data(), data.data() + cursor, weightCount * sizeof(Value));
            cursor += weightCount * sizeof(Value);
        }

        Value noiseMean = 0.0, noiseStd = 1.0;
        std::memcpy(&noiseMean, data.data() + cursor, sizeof(Value));
        std::memcpy(&noiseStd, data.data() + cursor + sizeof(Value), sizeof(Value));
        cursor += 2 * sizeof(Value);

        addLinearVariable(name, parents, weights, noiseMean, noiseStd);
    }

    return true;
}

}} // namespace yuki::causal
