#include "brain/research/core/ResearchPlanner.h"
#include <sstream>
#include <algorithm>

namespace yuki {
namespace research {

ResearchPlanner::ResearchPlanner(ToolRegistry* registry)
    : toolRegistry_(registry) {}

std::vector<SubGoal> ResearchPlanner::decompose(const std::string& query) {
    return decomposeInternal(query);
}

std::vector<SubGoal> ResearchPlanner::decomposeInternal(const std::string& query) {
    std::vector<SubGoal> goals;
    uint64_t baseHash = 0x811c9dc5;
    for (char c : query) {
        baseHash = (baseHash ^ static_cast<uint64_t>(c)) * 0x01000193;
    }

    SubGoal rootGoal;
    rootGoal.goalId = baseHash;
    rootGoal.descriptionHash = baseHash;
    rootGoal.confidence = 0.5f;
    goals.push_back(rootGoal);

    std::istringstream stream(query);
    std::string token;
    uint32_t goalIndex = 1;
    while (stream >> token) {
        uint64_t hash = 0x811c9dc5;
        for (char c : token) {
            hash = (hash ^ static_cast<uint64_t>(c)) * 0x01000193;
        }

        if (token.length() > 5) {
            SubGoal sg;
            sg.goalId = baseHash ^ (hash << (goalIndex % 8));
            sg.descriptionHash = hash;
            sg.confidence = 0.3f + (static_cast<float>(token.length()) / 20.0f);
            if (sg.confidence > 0.9f) sg.confidence = 0.9f;
            sg.dependencies.push_back(rootGoal.goalId);
            goals.push_back(sg);
            goalIndex++;
            if (goals.size() >= kMaxSubGoals) break;
        }
    }

    return goals;
}

std::vector<SubGoal> ResearchPlanner::detectGaps(const std::vector<SubGoal>& goals) {
    std::vector<SubGoal> result = goals;
    for (auto& goal : result) {
        if (goal.confidence < SubGoal::kMinConfidenceThreshold) {
            goal.status = GoalStatus::NEEDS_RESEARCH;
        } else {
            goal.status = GoalStatus::NEEDS_VERIFICATION;
        }
    }
    return result;
}

std::vector<std::vector<ToolPtr>> ResearchPlanner::matchTools(
    const std::vector<SubGoal>& goals) {
    std::vector<std::vector<ToolPtr>> matches;
    if (!toolRegistry_) return matches;

    for (const auto& goal : goals) {
        std::vector<ToolPtr> candidates;
        auto allTools = toolRegistry_->getAllTools();
        for (const auto& tool : allTools) {
            float score = computeMatchScore(goal, tool->getMetadata());
            if (score >= kMinToolMatchScore) {
                candidates.push_back(tool);
            }
        }
        matches.push_back(candidates);
    }
    return matches;
}

float ResearchPlanner::computeMatchScore(const SubGoal& goal, 
                                          const ToolMetadata& meta) {
    float schemaOverlap = 0.0f;
    for (uint64_t reqHash : goal.requiredSchemaHashes) {
        if (meta.schema.outputSchemaHash == reqHash) {
            schemaOverlap = 1.0f;
            break;
        }
    }

    float reliabilityWeight = meta.reliability * 0.3f;

    float riskPenalty = 0.0f;
    if (meta.riskLevel == ToolRiskLevel::MEDIUM) riskPenalty = 0.1f;
    if (meta.riskLevel == ToolRiskLevel::HIGH) riskPenalty = 0.2f;
    if (meta.riskLevel == ToolRiskLevel::CRITICAL) riskPenalty = 0.4f;

    return (schemaOverlap * 0.5f) + reliabilityWeight - riskPenalty;
}

ResearchPlan ResearchPlanner::buildPlan(
    const std::vector<SubGoal>& goals,
    const std::vector<std::vector<ToolPtr>>& candidates) {

    ResearchPlan plan;
    plan.planId = 0x811c9dc5;

    uint64_t nodeId = 1;
    for (size_t i = 0; i < goals.size(); ++i) {
        PlanNode node;
        node.nodeId = nodeId++;
        node.associatedGoalId = goals[i].goalId;

        if (i < candidates.size() && !candidates[i].empty()) {
            node.type = NodeType::TOOL_EXECUTION;
            node.toolId = candidates[i][0]->getMetadata().toolId;
        } else {
            node.type = NodeType::HUMAN_CLARIFICATION;
            node.toolId = "";
        }

        for (uint64_t dep : goals[i].dependencies) {
            for (const auto& n : plan.nodes) {
                if (n.associatedGoalId == dep) {
                    node.inputNodeIds.push_back(n.nodeId);
                }
            }
        }

        plan.nodes.push_back(node);
    }

    PlanNode synth;
    synth.nodeId = nodeId;
    synth.type = NodeType::SYNTHESIS;
    for (const auto& node : plan.nodes) {
        if (node.type == NodeType::TOOL_EXECUTION) {
            synth.inputNodeIds.push_back(node.nodeId);
        }
    }
    plan.nodes.push_back(synth);

    plan.buildWaves();
    return plan;
}

} // namespace research
} // namespace yuki
