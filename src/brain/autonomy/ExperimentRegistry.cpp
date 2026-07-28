#include "src/brain/autonomy/ExperimentRegistry.h"

namespace yuki::autonomy {

void ExperimentRegistry::registerExperiment(const ExperimentRecord& exp) {
    experiments_[exp.experimentId] = exp;
}

void ExperimentRegistry::updateState(const std::string& experimentId, ExperimentState state, float observedGain) {
    auto it = experiments_.find(experimentId);
    if (it != experiments_.end()) {
        it->second.state = state;
        it->second.observedGain = observedGain;
    }
}

std::optional<ExperimentRecord> ExperimentRegistry::getById(const std::string& experimentId) const {
    auto it = experiments_.find(experimentId);
    if (it != experiments_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ExperimentRecord> ExperimentRegistry::activeExperiments() const {
    std::vector<ExperimentRecord> result;
    for (const auto& kv : experiments_) {
        if (kv.second.state == ExperimentState::PROPOSED || kv.second.state == ExperimentState::RUNNING) {
            result.push_back(kv.second);
        }
    }
    return result;
}

} // namespace yuki::autonomy
