#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <vector>

namespace yuki::autonomy {

enum class AgentRoleKind {
    RESEARCH_AGENT = 0,
    PLANNER_AGENT,
    TESTER_AGENT,
    CODE_AGENT,
    VERIFIER_AGENT,
    ECONOMY_AGENT,
    WATCHDOG_AGENT,
    KNOWLEDGE_LINKER_AGENT
};

struct AgentContext {
    std::string agentId;
    AgentRoleKind role = AgentRoleKind::RESEARCH_AGENT;
    std::string taskGoal;
    float riskCeiling = 0.5f;
    float budgetCeiling = 10.0f;
    bool active = true;
};

class AgentSpawner {
public:
    AgentContext spawnAgent(AgentRoleKind role, const std::string& taskGoal, float riskCeiling, float budgetCeiling);
    bool terminateAgent(const std::string& agentId);
    std::vector<AgentContext> activeAgents() const;

private:
    std::vector<AgentContext> agents_;
};

} // namespace yuki::autonomy
