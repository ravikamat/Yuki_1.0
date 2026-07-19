#pragma once
#include "brain/skills/SkillRegistry.h"
#include "../ToolExecutor.h"
#include "../BrainTypes.h"
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <ctime>

enum class TaskCategory {
    GOOGLE_SEARCH, WEATHER_CHECK, EMAIL_SEND, REMINDER_SET,
    FILE_FIND, CALCULATOR, SCREENSHOT, SYSTEM_INFO,
    TRANSLATE, WEB_OPEN, WHATSAPP_MSG, GREETING, GENERAL_QUESTION, UNKNOWN
};

struct SkillBlueprint {
    bool             shouldBuild    = false;
    TaskCategory     category       = TaskCategory::UNKNOWN;
    std::string      skillName;
    std::string      description;
    std::string      scriptPath;
    std::vector<std::string> triggerPatterns;
    std::string      actionTemplate;
    SkillActionType  actionType     = SkillActionType::CUSTOM_RESPONSE;
};

class AutonomousSkillBuilder {
public:
    AutonomousSkillBuilder() = default;
    std::string maybeLearn(const FullTrace& trace, SkillRegistry& registry, ToolExecutor& tools);
    static TaskCategory  categorize(const std::string& input, const PatternFrame& pattern);
    static SkillBlueprint buildBlueprint(TaskCategory category, const FullTrace& trace);
    static std::string   generateScript(TaskCategory category, const FullTrace& trace);
private:
    static std::string toLower(const std::string& s);
};
