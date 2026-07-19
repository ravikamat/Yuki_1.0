#pragma once
#include "../ToolExecutor.h"
#include "../BrainTypes.h"
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <functional>
#include <chrono>
#include <filesystem>

enum class SkillActionType {
    GREET_TIME_AWARE, SEND_MESSAGE, BROWSER_NAVIGATE,
    RECALL_FACT, CUSTOM_RESPONSE, RUN_SCRIPT, UNKNOWN
};

struct RuntimeSkill {
    std::string    id, name, description, createdFrom;
    int64_t        createdAt   = 0;
    int            timesUsed   = 0;
    float          priority    = 0.5f;
    std::vector<std::string> triggerKeywords;
    std::vector<std::string> triggerPatterns;
    SkillActionType          actionType = SkillActionType::UNKNOWN;
    std::string              actionTemplate;
    std::vector<std::string> actionParams;
};

struct SkillHit {
    bool              matched  = false;
    const RuntimeSkill* skill  = nullptr;
    std::string       extractedName;
    std::string       extractedParam;
};

class SkillRegistry {
public:
    SkillRegistry();
    void load();
    void saveAll() const;
    void saveSkill(const RuntimeSkill& skill) const;
    RuntimeSkill teach(const std::string& instruction);
    static bool isTeachingInstruction(const std::string& input);
    SkillHit    check(const std::string& input) const;
    std::string execute(const SkillHit& match, const std::string& userName = "") const;
    std::string listSkills() const;
    int         count() const;
private:
    std::string  timeGreeting() const;
    std::string  makeId() const;
    RuntimeSkill parseGreetingSkill(const std::string& raw) const;
    RuntimeSkill parseBrowserSkill(const std::string& raw) const;
    RuntimeSkill parseCustomSkill(const std::string& raw) const;
    static std::string toLower(const std::string& s);
    static bool hasWord(const std::string& h, const std::string& n);
    static std::string extractNameAfter(const std::string& input, const std::string& marker);
    mutable std::mutex        mu_;
    std::vector<RuntimeSkill> skills_;
    std::filesystem::file_time_type lastMtime_;
};
