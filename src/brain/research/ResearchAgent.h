#ifndef YUKI_RESEARCH_AGENT_H
#define YUKI_RESEARCH_AGENT_H

#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/KnowledgePack.h"
#include "brain/research/ResearchRequest.h"
#include "brain/research/RiskGate.h"
#include "brain/metacognition/HypothesisConsumer.h"
#include <memory>

namespace yuki {
namespace research {

class ResearchAgent : public metacognition::HypothesisConsumer {
public:
    ResearchAgent(ToolRegistry* registry, 
                  security::SecuritySandbox* sandbox);

    KnowledgePack research(const ResearchRequest& request);

    bool consume(const metacognition::ActionableHypothesis& hypothesis) override;
    size_t pendingCount() const override { return pendingCount_; }
    size_t completedCount() const override { return completedCount_; }

private:
    std::unique_ptr<ResearchPlanner> planner_;
    std::unique_ptr<RiskGate> riskGate_;
    ToolRegistry* registry_;
    size_t pendingCount_ = 0;
    size_t completedCount_ = 0;

    std::vector<ToolResult> executePlan(const ResearchPlan& plan);
    ToolResult executeNode(const PlanNode& node);
};

} // namespace research
} // namespace yuki

#endif
