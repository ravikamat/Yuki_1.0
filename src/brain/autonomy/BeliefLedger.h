#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <optional>
#include <unordered_map>
#include <vector>

namespace yuki::autonomy {

class BeliefLedger {
public:
    void upsert(const BeliefRecord& record);
    std::optional<BeliefRecord> getById(const std::string& beliefId) const;
    std::vector<BeliefRecord> byTag(const std::string& tag) const;
    BeliefRecord updateFromEvidence(const BeliefRecord& prior,
                                    float sourceScore,
                                    float consistency,
                                    float freshness,
                                    float utility,
                                    bool contradictionStrong) const;

private:
    std::unordered_map<std::string, BeliefRecord> beliefs_;
};

} // namespace yuki::autonomy
