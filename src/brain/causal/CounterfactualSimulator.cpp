#include "brain/causal/CounterfactualSimulator.h"
#include "brain/core/Logger.h"

#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace yuki { namespace causal {

class CounterfactualSimulator::Impl {
public:
    std::shared_ptr<StructuralCausalModel> model_;

    Impl() = default;
};

CounterfactualSimulator::CounterfactualSimulator() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "CounterfactualSimulator initialized");
}

CounterfactualSimulator::~CounterfactualSimulator() = default;

CounterfactualSimulator::CounterfactualSimulator(CounterfactualSimulator&&) noexcept = default;
CounterfactualSimulator& CounterfactualSimulator::operator=(CounterfactualSimulator&&) noexcept = default;

void CounterfactualSimulator::setModel(std::shared_ptr<StructuralCausalModel> model) {
    pImpl->model_ = model;
}

std::shared_ptr<StructuralCausalModel> CounterfactualSimulator::getModel() const {
    return pImpl->model_;
}

CounterfactualResult CounterfactualSimulator::simulate(const CounterfactualQuery& query) {
    CounterfactualResult res;
    if (!pImpl->model_) {
        res.valid = false;
        res.errorMessage = "No SCM model set";
        return res;
    }

    // Step 1: Abduction - Infer noise from evidence
    res.inferredNoise = pImpl->model_->inferNoise(query.evidence);

    // Factual outcome
    auto factualSol = pImpl->model_->solve(res.inferredNoise);
    auto fit = query.evidence.observations.find(query.targetY);
    if (fit != query.evidence.observations.end()) {
        res.factualY = fit->second;
    } else {
        res.factualY = factualSol[query.targetY];
    }

    // Step 2 & 3: Action & Prediction - Intervene and solve
    Intervention in{query.interventionX, query.interventionValue};
    auto cfSol = pImpl->model_->interveneAndSolve(res.inferredNoise, in);

    res.predictedY = cfSol[query.targetY];
    res.effect = res.predictedY - res.factualY;
    res.valid = true;

    return res;
}

std::vector<CounterfactualResult> CounterfactualSimulator::simulateBatch(VariableId targetY, VariableId interventionX,
                                                                         const std::vector<Value>& possibleValues,
                                                                         const Evidence& evidence) {
    std::vector<CounterfactualResult> results;
    results.reserve(possibleValues.size());

    for (Value val : possibleValues) {
        CounterfactualQuery q;
        q.targetY = targetY;
        q.interventionX = interventionX;
        q.interventionValue = val;
        q.evidence = evidence;
        results.push_back(simulate(q));
    }

    return results;
}

RegretAnalysis CounterfactualSimulator::analyzeRegret(VariableId targetY,
                                                      const std::vector<Intervention>& possibleInterventions,
                                                      const Evidence& evidence) {
    RegretAnalysis reg;
    if (possibleInterventions.empty() || !pImpl->model_) return reg;

    Value bestVal = -1e9;
    VariableId bestVar = 0;
    Value bestTargetVal = 0.0;

    for (const auto& in : possibleInterventions) {
        CounterfactualQuery q;
        q.targetY = targetY;
        q.interventionX = in.target;
        q.interventionValue = in.value;
        q.evidence = evidence;

        auto res = simulate(q);
        reg.outcomes.push_back({in, res.predictedY});

        if (res.predictedY > bestVal) {
            bestVal = res.predictedY;
            bestVar = in.target;
            bestTargetVal = in.value;
        }
    }

    reg.bestIntervention = bestVar;
    reg.bestValue = bestTargetVal;
    reg.bestOutcome = bestVal;

    return reg;
}

CounterfactualResult CounterfactualSimulator::whatIf(VariableId outcomeVariable,
                                                     const Intervention& counterfactualAction,
                                                     const Evidence& actualEvidence) {
    CounterfactualQuery q;
    q.targetY = outcomeVariable;
    q.interventionX = counterfactualAction.target;
    q.interventionValue = counterfactualAction.value;
    q.evidence = actualEvidence;
    return simulate(q);
}

Value CounterfactualSimulator::computeATE(VariableId treatment, VariableId outcome,
                                          Value treatmentOn, Value treatmentOff,
                                          size_t numSamples) {
    if (!pImpl->model_ || numSamples == 0) return 0.0;

    size_t varCount = pImpl->model_->getVariableCount();
    std::mt19937_64 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    double sumDiff = 0.0;

    for (size_t s = 0; s < numSamples; ++s) {
        std::vector<Value> noise(varCount);
        for (size_t i = 0; i < varCount; ++i) {
            noise[i] = dist(rng);
        }

        Intervention inOn{treatment, treatmentOn};
        Intervention inOff{treatment, treatmentOff};

        auto solOn = pImpl->model_->interveneAndSolve(noise, inOn);
        auto solOff = pImpl->model_->interveneAndSolve(noise, inOff);

        sumDiff += (solOn[outcome] - solOff[outcome]);
    }

    return sumDiff / static_cast<double>(numSamples);
}

std::vector<uint8_t> CounterfactualSimulator::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x43465330; // 'CFS0'

    buf.resize(4);
    std::memcpy(buf.data(), &magic, 4);

    if (pImpl->model_) {
        auto scmBytes = pImpl->model_->serialize();
        uint32_t len = static_cast<uint32_t>(scmBytes.size());
        size_t off = buf.size();
        buf.resize(off + 4 + len);
        std::memcpy(buf.data() + off, &len, 4);
        std::memcpy(buf.data() + off + 4, scmBytes.data(), len);
    } else {
        uint32_t len = 0;
        size_t off = buf.size();
        buf.resize(off + 4);
        std::memcpy(buf.data() + off, &len, 4);
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

bool CounterfactualSimulator::deserialize(const std::vector<uint8_t>& data) {
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

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x43465330) return false;

    uint32_t scmLen = 0;
    std::memcpy(&scmLen, data.data() + 4, 4);

    if (scmLen > 0) {
        std::vector<uint8_t> scmBytes(scmLen);
        std::memcpy(scmBytes.data(), data.data() + 8, scmLen);

        auto scm = std::make_shared<StructuralCausalModel>();
        if (scm->deserialize(scmBytes)) {
            pImpl->model_ = scm;
        }
    }

    return true;
}

}} // namespace yuki::causal
