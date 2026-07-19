// SkillRegistry.cpp — Skill registry implementation (split from SkillSystem.cpp)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "brain/skills/SkillRegistry.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cctype>

static const std::string SKILL_DIR = "data/skills/";

std::string SkillRegistry::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool SkillRegistry::hasWord(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
std::string SkillRegistry::extractNameAfter(const std::string& input, const std::string& marker) {
    auto pos = toLower(input).find(marker);
    if (pos == std::string::npos) return "";
    std::string rest = input.substr(pos + marker.size());
    while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    auto end = rest.find_first_of(" .,!?;:\n");
    if (end != std::string::npos) rest = rest.substr(0, end);
    if (!rest.empty()) rest[0] = static_cast<char>(toupper((unsigned char)rest[0]));
    return rest;
}
std::string SkillRegistry::makeId() const {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return "skill_" + std::to_string(ms);
}
std::string SkillRegistry::timeGreeting() const {
    std::time_t now = std::time(nullptr);
    std::tm* lt = std::localtime(&now);
    int hour = lt ? lt->tm_hour : 12;
    if (hour >=  5 && hour < 12) return "Good morning";
    if (hour >= 12 && hour < 17) return "Good afternoon";
    if (hour >= 17 && hour < 21) return "Good evening";
    return "Good night";
}

SkillRegistry::SkillRegistry() {
    try { std::filesystem::create_directories(SKILL_DIR); } catch (...) {}
    load();
}

void SkillRegistry::load() {
    std::lock_guard<std::mutex> lock(mu_);
    namespace fs = std::filesystem;
    const auto skillsDir = fs::path("data/skills");
    if (fs::exists(skillsDir)) {
        auto currentMtime = fs::last_write_time(skillsDir);
        if (currentMtime == lastMtime_ && !skills_.empty()) {
            return;  // No changes, use cache
        }
        lastMtime_ = currentMtime;
    }
    skills_.clear();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(SKILL_DIR)) {
            if (entry.path().extension() != ".json") continue;
            std::ifstream f(entry.path()); if (!f.is_open()) continue;
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            RuntimeSkill skill;
            auto getStr = [&](const std::string& key) -> std::string {
                auto kpos = content.find("\"" + key + "\"");
                if (kpos == std::string::npos) return "";
                auto vstart = content.find('"', kpos + key.size() + 2);
                if (vstart == std::string::npos) return "";
                ++vstart; auto vend = vstart;
                while (vend < content.size()) {
                    if (content[vend] == '"' && (vend == 0 || content[vend-1] != '\\')) break;
                    ++vend;
                }
                return content.substr(vstart, vend - vstart);
            };
            auto getInt = [&](const std::string& key, int def = 0) -> int {
                auto kpos = content.find("\"" + key + "\"");
                if (kpos == std::string::npos) return def;
                auto colon = content.find(':', kpos);
                if (colon == std::string::npos) return def;
                try { return std::stoi(content.substr(colon + 1)); } catch (...) { return def; }
            };
            skill.id             = getStr("id");
            skill.name           = getStr("name");
            skill.description    = getStr("description");
            skill.createdFrom    = getStr("createdFrom");
            skill.actionTemplate = getStr("actionTemplate");
            skill.timesUsed      = getInt("timesUsed");
            std::string at = getStr("actionType");
            if      (at == "GREET_TIME_AWARE") skill.actionType = SkillActionType::GREET_TIME_AWARE;
            else if (at == "SEND_MESSAGE")     skill.actionType = SkillActionType::SEND_MESSAGE;
            else if (at == "BROWSER_NAVIGATE") skill.actionType = SkillActionType::BROWSER_NAVIGATE;
            else if (at == "CUSTOM_RESPONSE")  skill.actionType = SkillActionType::CUSTOM_RESPONSE;
            else if (at == "RUN_SCRIPT")       skill.actionType = SkillActionType::RUN_SCRIPT;
            else                               skill.actionType = SkillActionType::UNKNOWN;
            auto arrStart = content.find("\"triggerPatterns\"");
            if (arrStart != std::string::npos) {
                auto bOpen = content.find('[', arrStart), bClose = content.find(']', bOpen);
                if (bOpen != std::string::npos && bClose != std::string::npos) {
                    std::string arr = content.substr(bOpen+1, bClose-bOpen-1);
                    std::istringstream ss(arr); std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        auto q1 = tok.find('"'), q2 = tok.rfind('"');
                        if (q1 != std::string::npos && q2 != q1)
                            skill.triggerPatterns.push_back(tok.substr(q1+1, q2-q1-1));
                    }
                }
            }
            if (!skill.id.empty()) skills_.push_back(skill);
        }
    } catch (...) {}
    std::cout << "[SkillRegistry] Loaded " << skills_.size() << " skills\n";
}

void SkillRegistry::saveSkill(const RuntimeSkill& skill) const {
    std::string path = SKILL_DIR + skill.id + ".json";
    std::ofstream f(path); if (!f.is_open()) return;
    auto actionTypeStr = [](SkillActionType t) -> const char* {
        switch (t) {
            case SkillActionType::GREET_TIME_AWARE: return "GREET_TIME_AWARE";
            case SkillActionType::SEND_MESSAGE:     return "SEND_MESSAGE";
            case SkillActionType::BROWSER_NAVIGATE: return "BROWSER_NAVIGATE";
            case SkillActionType::CUSTOM_RESPONSE:  return "CUSTOM_RESPONSE";
            case SkillActionType::RUN_SCRIPT:       return "RUN_SCRIPT";
            default:                                return "UNKNOWN";
        }
    };
    f << "{\n"
      << "  \"id\": \""             << skill.id           << "\",\n"
      << "  \"name\": \""           << skill.name         << "\",\n"
      << "  \"description\": \""    << skill.description  << "\",\n"
      << "  \"createdFrom\": \""    << skill.createdFrom  << "\",\n"
      << "  \"actionType\": \""     << actionTypeStr(skill.actionType) << "\",\n"
      << "  \"actionTemplate\": \"" << skill.actionTemplate << "\",\n"
      << "  \"timesUsed\": "        << skill.timesUsed    << ",\n"
      << "  \"triggerPatterns\": [";
    for (size_t i = 0; i < skill.triggerPatterns.size(); ++i) {
        if (i) f << ", ";
        f << "\"" << skill.triggerPatterns[i] << "\"";
    }
    f << "]\n}\n";
    std::cout << "[SkillRegistry] Saved skill: " << skill.name << " → " << path << "\n";
}
void SkillRegistry::saveAll() const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& s : skills_) saveSkill(s);
}

bool SkillRegistry::isTeachingInstruction(const std::string& input) {
    const std::string lower = toLower(input);
    return hasWord(lower,"learn this")||hasWord(lower,"learn that")||
           hasWord(lower,"remember this rule")||hasWord(lower,"always when")||
           hasWord(lower,"whenever someone")||hasWord(lower,"teach yourself")||
           (hasWord(lower,"always")&&hasWord(lower,"greet"))||
           hasWord(lower,"from now on")||hasWord(lower,"every time someone");
}

RuntimeSkill SkillRegistry::parseGreetingSkill(const std::string&) const {
    RuntimeSkill s;
    s.name        = "Time-Aware Greeting";
    s.description = "Greet with time-appropriate greeting when someone introduces themselves";
    s.actionType  = SkillActionType::GREET_TIME_AWARE;
    s.actionTemplate = "{greeting}, {name}! It's wonderful to meet you. How can I help you today?";
    s.priority    = 0.9f;
    s.triggerPatterns = {"hi i am","hello i am","hey i am","hi, i am","hello, i am",
                          "hey, i am","hi! i am","hello! i am","my name is","i am ","i'm ",
                          "namaste i am","hii i am"};
    return s;
}
RuntimeSkill SkillRegistry::parseBrowserSkill(const std::string&) const {
    RuntimeSkill s;
    s.name="Browser Navigation"; s.description="Open browser and navigate to a URL or app";
    s.actionType=SkillActionType::BROWSER_NAVIGATE; s.actionTemplate="Opening browser as requested.";
    s.triggerPatterns={"open browser","navigate to","go to website"}; return s;
}
RuntimeSkill SkillRegistry::parseCustomSkill(const std::string& raw) const {
    RuntimeSkill s;
    s.name="Custom Response"; s.description="Respond with a fixed template";
    s.actionType=SkillActionType::CUSTOM_RESPONSE;
    const std::string lower = toLower(raw);
    auto extract = [&](const std::string& marker) -> std::string {
        auto pos = lower.find(marker);
        if (pos==std::string::npos) return "";
        return raw.substr(pos+marker.size());
    };
    std::string tmpl = extract("respond with ");
    if (tmpl.empty()) tmpl = extract("say ");
    if (tmpl.empty()) tmpl = raw;
    s.actionTemplate=tmpl; s.triggerPatterns={}; return s;
}

RuntimeSkill SkillRegistry::teach(const std::string& instruction) {
    const std::string lower = toLower(instruction);
    RuntimeSkill skill;
    bool isGreeting = hasWord(lower,"greet")||hasWord(lower,"greeting")||hasWord(lower,"good morning")||
                      hasWord(lower,"good evening")||hasWord(lower,"good afternoon")||
                      hasWord(lower,"time")||(hasWord(lower,"hi i am")||hasWord(lower,"hello i am"));
    bool isBrowser  = hasWord(lower,"browser")||hasWord(lower,"open")||hasWord(lower,"navigate")||hasWord(lower,"website");
    if (isGreeting) skill = parseGreetingSkill(instruction);
    else if (isBrowser) skill = parseBrowserSkill(instruction);
    else skill = parseCustomSkill(instruction);
    skill.id = makeId(); skill.createdFrom = instruction;
    { std::lock_guard<std::mutex> lock(mu_); skills_.push_back(skill); }
    saveSkill(skill); return skill;
}

SkillHit SkillRegistry::check(const std::string& input) const {
    std::lock_guard<std::mutex> lock(mu_);
    const std::string lower = toLower(input);
    for (const auto& skill : skills_) {
        for (const auto& pattern : skill.triggerPatterns) {
            if (hasWord(lower, pattern)) {
                SkillHit m; m.matched=true; m.skill=&skill;
                if (skill.actionType == SkillActionType::GREET_TIME_AWARE) {
                    for (const auto& marker : {"hi i am ","hello i am ","hey i am ","hi, i am ","i am ","i'm ","my name is "}) {
                        std::string name = extractNameAfter(input, marker);
                        if (!name.empty() && name.size()<30) { m.extractedName=name; break; }
                    }
                }
                return m;
            }
        }
    }
    return SkillHit{};
}

std::string SkillRegistry::execute(const SkillHit& match, const std::string&) const {
    if (!match.matched || !match.skill) return "";
    const RuntimeSkill& skill = *match.skill;
    const_cast<RuntimeSkill&>(skill).timesUsed++;
    switch (skill.actionType) {
    case SkillActionType::GREET_TIME_AWARE: {
        std::string greeting = timeGreeting();
        std::string name = match.extractedName.empty() ? "there" : match.extractedName;
        std::string tmpl = skill.actionTemplate;
        auto replace = [&](std::string& s, const std::string& from, const std::string& to) {
            auto pos = s.find(from); if (pos!=std::string::npos) s.replace(pos, from.size(), to);
        };
        replace(tmpl,"{greeting}",greeting); replace(tmpl,"{name}",name); return tmpl;
    }
    case SkillActionType::CUSTOM_RESPONSE:  return skill.actionTemplate;
    case SkillActionType::BROWSER_NAVIGATE: return "Navigating as requested. " + skill.actionTemplate;
    case SkillActionType::RUN_SCRIPT: {
        std::string scriptPath = skill.actionTemplate;
        if (scriptPath.find("SCRIPT:") == 0) scriptPath = scriptPath.substr(7);
        std::string param = match.extractedParam.empty() ? match.extractedName : match.extractedParam;
        std::string cmd = "python " + scriptPath;
        if (!param.empty()) cmd += " \"" + param + "\"";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) return "Running: " + scriptPath;
        std::string output; char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) output += buf;
        _pclose(pipe);
        auto rpos = output.find("\"result\":");
        if (rpos != std::string::npos) {
            auto vstart = output.find('"', rpos+9);
            if (vstart!=std::string::npos) { ++vstart; auto vend=vstart; while (vend<output.size()&&output[vend]!='"') ++vend; return output.substr(vstart, vend-vstart); }
        }
        return output.empty() ? "Done." : output.substr(0, 200);
    }
    default: return skill.actionTemplate;
    }
}

std::string SkillRegistry::listSkills() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (skills_.empty()) return "No skills learned yet.";
    std::ostringstream ss; ss << "I have " << skills_.size() << " learned skills:\n";
    for (size_t i=0; i<skills_.size(); ++i)
        ss << "  " << (i+1) << ". " << skills_[i].name << " — " << skills_[i].description
           << " (used " << skills_[i].timesUsed << "x)\n";
    return ss.str();
}
int SkillRegistry::count() const { std::lock_guard<std::mutex> lock(mu_); return (int)skills_.size(); }
