#include <cassert>
#include "brain/action/core/ActionPlan.h"
#include "brain/policy/ExecutivePolicySelector.h"

using namespace yuki::action;
using namespace yuki;

int main() {
    policy::ExecutivePolicySelector selector(nullptr);

    float researchThreshold = selector.computeRiskAdjustedThreshold(0.5f, 0.2f);
    float actionThreshold = selector.computeActionRiskAdjustedThreshold(0.5f, 0.2f);
    assert(actionThreshold > researchThreshold);

    assert(selector.requiresHumanApprovalForAction("FILE_DELETE", 0.1f));
    assert(selector.requiresHumanApprovalForAction("SYSTEM_COMMAND", 0.1f));
    assert(!selector.requiresHumanApprovalForAction("FILE_CREATE", 0.1f));

    return 0;
}
