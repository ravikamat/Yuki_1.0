#include "src/brain/autonomy/BeliefLedger.h"
#include "src/brain/core/ConfigManager.h"
#include <algorithm>

namespace yuki::autonomy {

static float blendBeliefConfidence(float oldConf, float sourceScore, float consistency, float freshness, float utility) {
    const float alpha = ConfigManager::instance().loadFloatConfig("belief.alpha", 0.55f);
    const float evidence = 0.35f * sourceScore + 0.30f * consistency + 0.20f * freshness + 0.15f * utility;
    return std::clamp(alpha * oldConf + (1.0f - alpha) * evidence, 0.0f, 1.0f);
}

void BeliefLedger::upsert(const BeliefRecord& record) {
    beliefs_[record.beliefId] = record;
}

std::optional<BeliefRecord> BeliefLedger::getById(const std::string& beliefId) const {
    auto it = beliefs_.find(beliefId);
    if (it != beliefs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<BeliefRecord> BeliefLedger::byTag(const std::string& tag) const {
    std::vector<BeliefRecord> result;
    for (const auto& kv : beliefs_) {
        for (const auto& t : kv.second.tags) {
            if (t == tag) {
                result.push_back(kv.second);
                break;
            }
        }
    }
    return result;
}

BeliefRecord BeliefLedger::updateFromEvidence(const BeliefRecord& prior,
                                               float sourceScore,
                                               float consistency,
                                               float freshness,
                                               float utility,
                                               bool contradictionStrong) const {
    BeliefRecord updated = prior;
    updated.confidence = blendBeliefConfidence(prior.confidence, sourceScore, consistency, freshness, utility);

    if (contradictionStrong) {
        updated.status = BeliefStatus::HYPOTHESIS;
    } else if (updated.confidence >= 0.80f) {
        updated.status = BeliefStatus::VERIFIED;
    } else if (updated.confidence >= 0.50f) {
        updated.status = BeliefStatus::LIKELY;
    } else {
        updated.status = BeliefStatus::HYPOTHESIS;
    }

    return updated;
}

} // namespace yuki::autonomy
