#include "src/brain/autonomy/OwnerIntentArbiter.h"

namespace yuki::autonomy {

OwnerIntentDecision OwnerIntentArbiter::decide(bool legal,
                                               bool safe,
                                               bool competent,
                                               bool alternativeExists,
                                               bool buildableLater) const {
    OwnerIntentDecision out;
    if (legal && safe && competent) {
        out.mode = OwnerDecisionMode::COMPLY;
        out.rationale = "safe_and_competent";
        out.requiresApproval = false;
    } else if ((!safe || !competent) && alternativeExists) {
        out.mode = OwnerDecisionMode::SAFE_ALTERNATIVE;
        out.rationale = "alternative_path_available";
        out.requiresApproval = true;
    } else if (buildableLater) {
        out.mode = OwnerDecisionMode::DEFER_BUILD_PATH;
        out.rationale = "not_possible_now_but_buildable";
        out.requiresApproval = false;
    } else {
        out.mode = OwnerDecisionMode::DECLINE;
        out.rationale = "blocked_by_hard_constraint";
        out.requiresApproval = false;
    }
    return out;
}

} // namespace yuki::autonomy
