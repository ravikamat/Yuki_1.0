#ifndef YUKI_RESEARCH_PLANNER_H
#define YUKI_RESEARCH_PLANNER_H

#include "brain/research/core/SubGoal.h"
#include "brain/research/core/ResearchPlan.h"
#include "brain/research/core/ToolInterface.h"
#include "brain/research/core/ToolRegistry.h"
#include <vector>
#include <memory>

namespace yuki {
namespace research {

class ResearchPlanner {
public:
    explicit ResearchPlanner(ToolRegistry* registry);

    std::vector<SubGoal> decompose(const std::string& query);
    std::vector<SubGoal> detectGaps(const std::vector<SubGoal>& goals);
    std::vector<std::vector<ToolPtr>> matchTools(const std::vector<SubGoal>& goals);
    ResearchPlan buildPlan(const std::vector<SubGoal>& goals,
                           const std::vector<std::vector<ToolPtr>>& candidates);

    static constexpr uint32_t kMaxSubGoals = 50;
    static constexpr float    kMinToolMatchScore = 0.3f;

private:
    ToolRegistry* toolRegistry_;

    float computeMatchScore(const SubGoal& goal, const ToolMetadata& meta);
    std::vector<SubGoal> decomposeInternal(const std::string& query);
};

} // namespace research
} // namespace yuki

#endif
