#pragma once
#include "CapabilityGraph.h"
#include "PathFinder.h"
#include "ResourceOptimizer.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/ActionPlan.h"
#include <optional>

namespace yuki::capability {

class SequencingEngine {
public:
    SequencingEngine() = default;

    std::optional<::yuki::action::ActionPlan> toActionPlan(const PathResult& path,
                                                           const WaveSchedule& schedule,
                                                           const CapabilityGraph& graph);
    bool validatePlan(const ::yuki::action::ActionPlan& plan);

private:
    ::yuki::action::ActionGoal nodeToActionGoal(uint32_t node_id, const CapabilityGraph& graph);
    std::vector<::yuki::action::Precondition> generatePreconditions(uint32_t node_id, const CapabilityGraph& graph);
    std::vector<::yuki::action::Postcondition> generatePostconditions(uint32_t node_id, const CapabilityGraph& graph);
    ::yuki::action::ActionType mapNodeTypeToActionType(const CapabilityNode& node);
};

} // namespace yuki::capability
