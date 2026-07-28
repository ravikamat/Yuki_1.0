#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <optional>
#include <unordered_map>
#include <vector>

namespace yuki::autonomy {

class ExperimentRegistry {
public:
    void registerExperiment(const ExperimentRecord& exp);
    void updateState(const std::string& experimentId, ExperimentState state, float observedGain);
    std::optional<ExperimentRecord> getById(const std::string& experimentId) const;
    std::vector<ExperimentRecord> activeExperiments() const;

private:
    std::unordered_map<std::string, ExperimentRecord> experiments_;
};

} // namespace yuki::autonomy
