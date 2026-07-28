#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"

namespace yuki::autonomy {

class OwnerIntentArbiter {
public:
    OwnerIntentDecision decide(bool legal,
                               bool safe,
                               bool competent,
                               bool alternativeExists,
                               bool buildableLater) const;
};

} // namespace yuki::autonomy
