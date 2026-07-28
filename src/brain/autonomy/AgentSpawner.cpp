#include "src/brain/autonomy/AgentSpawner.h"
#include <algorithm>
#include <chrono>

namespace yuki::autonomy {

AgentContext AgentSpawner::spawnAgent(AgentRoleKind role, const std::string& taskGoal, float riskCeiling, float budgetCeiling) {
    AgentContext ctx;
    ctx.agentId = "agent_" + std::to_string(role == AgentRoleKind::RESEARCH_AGENT ? 1 : 2) + "_"
                + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    ctx.role = role;
    ctx.taskGoal = taskGoal;
    ctx.riskCeiling = riskCeiling;
    ctx.budgetCeiling = budgetCeiling;
    ctx.active = true;
    agents_.push_back(ctx);
    return ctx;
}

bool AgentSpawner::terminateAgent(const std::string& agentId) {
    for (auto& a : agents_) {
        if (a.agentId == agentId) {
            a.active = false;
            return true;
        }
    }
    return false;
}

std::vector<AgentContext> AgentSpawner::activeAgents() const {
    std::vector<AgentContext> result;
    for (const auto& a : agents_) {
        if (a.active) {
            result.push_back(a);
        }
    }
    return result;
}

} // namespace yuki::autonomy
