// SkillSystem.cpp — Autonomous skill builder implementation (split from legacy SkillSystem.cpp)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "brain/skills/SkillSystem.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cctype>

static constexpr std::string_view SCRIPT_DIR_ASB     = "data/scripts/";
static constexpr float FACTUAL_CONFIDENCE_THRESHOLD  = 0.55f;
static constexpr float SKILL_CONFIDENCE_THRESHOLD    = 0.35f;
static constexpr size_t TRIGGER_KEY_WORD_LIMIT       = 6;
static constexpr size_t ACTION_TEMPLATE_CAP          = 500;
static constexpr size_t CREATED_FROM_CAP             = 80;
static constexpr size_t SKILL_NAME_CAP               = 40;
static constexpr size_t FACTUAL_CREATED_CAP          = 60;

std::string AutonomousSkillBuilder::toLower(const std::string& s) {
    std::string r; r.reserve(s.size());
    for (unsigned char c : s) r += static_cast<char>(std::tolower(c));
    return r;
}
static std::string asb_cap(const std::string& s, size_t n) { return s.substr(0,std::min(s.size(),n)); }
static uint64_t asb_nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

struct CategoryRule { TaskCategory category; std::initializer_list<std::string_view> keywords; };
static const CategoryRule CATEGORY_TABLE[] = {
    { TaskCategory::GREETING,       { "hi i am","hello i am","hey i am","good morning","good evening" } },
    { TaskCategory::GOOGLE_SEARCH,  { "search for ","google ","look up ","search about " } },
    { TaskCategory::WEATHER_CHECK,  { "weather","temperature","forecast","rain today","will it rain" } },
    { TaskCategory::EMAIL_SEND,     { "send email","send mail","email to","write email","mail to" } },
    { TaskCategory::REMINDER_SET,   { "remind me","set reminder","set alarm","notify me","alert me at" } },
    { TaskCategory::SCREENSHOT,     { "screenshot","screen capture","capture screen" } },
    { TaskCategory::SYSTEM_INFO,    { "cpu usage","ram usage","memory usage","disk space","system info","battery" } },
    { TaskCategory::TRANSLATE,      { "translate","in spanish","in hindi","in french","in german","translate to" } },
    { TaskCategory::FILE_FIND,      { "find file","locate file","search file" } },
    { TaskCategory::CALCULATOR,     { "calculate ","compute " } },
    { TaskCategory::WEB_OPEN,       { "open ","go to ","navigate to",".com",".org",".in" } },
    { TaskCategory::WHATSAPP_MSG,   { "whatsapp","msg ","message " } },
};
static constexpr std::string_view ACTION_VERBS[] = {
    "book ","schedule ","order ","generate ","track ","monitor ","organize ","manage ",
    "automate ","create report","make report","send report","check status","fetch data",
    "download ","upload ","backup ","sync ","parse "
};
static constexpr std::string_view QUESTION_PREFIXES[] = {
    "what is ","who is ","how does ","explain ","define ","tell me about ","what are ","why is ","when did "
};

TaskCategory AutonomousSkillBuilder::categorize(const std::string& input, const PatternFrame&) {
    const std::string L = toLower(input);
    for (const auto& rule : CATEGORY_TABLE)
        for (std::string_view kw : rule.keywords)
            if (L.find(kw) != std::string::npos) return rule.category;
    for (std::string_view v : ACTION_VERBS) if (L.find(v)!=std::string::npos) return TaskCategory::UNKNOWN;
    for (std::string_view p : QUESTION_PREFIXES) if (L.find(p)!=std::string::npos) return TaskCategory::GENERAL_QUESTION;
    return TaskCategory::UNKNOWN;
}

struct BlueprintTemplate {
    TaskCategory category; std::string_view skillName, description, scriptName, actionTemplate;
    std::initializer_list<std::string_view> triggers;
};
static const BlueprintTemplate BLUEPRINT_TABLE[] = {
    { TaskCategory::GOOGLE_SEARCH, "Google Search","Search Google and return top results","search_google.py","Searching Google for '{query}'...",{ "search for ","google ","look up ","search about ","find information about " } },
    { TaskCategory::WEATHER_CHECK, "Weather Check","Check weather for any city","weather_check.py","Checking weather for '{location}'...",{ "weather in ","weather at ","temperature in ","forecast for ","will it rain","how hot is" } },
    { TaskCategory::EMAIL_SEND,    "Send Email","Send an email via system email client","send_email.py","Sending email to '{recipient}'...",{ "send email to ","email to ","send mail to ","write email to ","mail to " } },
    { TaskCategory::REMINDER_SET,  "Set Reminder","Set a timed reminder / alarm","set_reminder.py","Reminder set for '{time}'!",{ "remind me ","set reminder ","set alarm ","notify me at ","alert me at ","remember to " } },
    { TaskCategory::SCREENSHOT,    "Take Screenshot","Capture the screen","screenshot.py","Screenshot saved to data/screenshots/",{ "screenshot","take screenshot","screen capture","capture screen","snap screen" } },
    { TaskCategory::SYSTEM_INFO,   "System Info","Report CPU, RAM, disk, battery usage","system_info.py","Getting system information...",{ "cpu usage","ram usage","memory usage","disk space","system info","battery level","how much ram","processor speed" } },
    { TaskCategory::TRANSLATE,     "Translate Text","Translate text between languages","translate.py","Translating '{text}' to {language}...",{ "translate ","translate to ","in spanish","in hindi","in french","in german","say in " } },
    { TaskCategory::FILE_FIND,     "Find File","Search for files on the computer","find_file.py","Searching for '{filename}'...",{ "find file ","where is ","locate file ","search file ","find my " } },
    { TaskCategory::CALCULATOR,    "Calculator","Evaluate mathematical expressions","calculate.py","Result: {result}",{ "calculate ","compute ","how much is ","evaluate " } },
    { TaskCategory::WEB_OPEN,      "Open Website","Open any URL or website in the default browser","open_url.py","Opening '{url}'...",{ "open ","go to ","navigate to ","launch website","open website" } },
};

SkillBlueprint AutonomousSkillBuilder::buildBlueprint(TaskCategory category, const FullTrace&) {
    for (const auto& t : BLUEPRINT_TABLE) {
        if (t.category != category) continue;
        SkillBlueprint bp; bp.shouldBuild=true; bp.category=category;
        bp.actionType=SkillActionType::RUN_SCRIPT; bp.skillName=std::string(t.skillName);
        bp.description=std::string(t.description);
        bp.scriptPath=std::string(SCRIPT_DIR_ASB)+std::string(t.scriptName);
        bp.actionTemplate=std::string(t.actionTemplate);
        for (std::string_view tr : t.triggers) bp.triggerPatterns.emplace_back(tr);
        return bp;
    }
    SkillBlueprint bp; bp.shouldBuild=false; return bp;
}

static constexpr std::string_view SCRIPT_GOOGLE_SEARCH = R"PYEOF(
#!/usr/bin/env python3
import sys, json, webbrowser, urllib.parse
query = ' '.join(sys.argv[1:]) if len(sys.argv) > 1 else ''
webbrowser.open('https://www.google.com/search?q=' + urllib.parse.quote(query))
print(json.dumps({'success': True, 'result': 'Opened Google for: ' + query}))
)PYEOF";
static constexpr std::string_view SCRIPT_WEATHER = R"PYEOF(
#!/usr/bin/env python3
import sys, json, urllib.parse, urllib.request
loc = ' '.join(sys.argv[1:]) if len(sys.argv) > 1 else 'Mumbai'
try:
    url = 'https://wttr.in/' + urllib.parse.quote(loc) + '?format=3'
    r = urllib.request.urlopen(url, timeout=8).read().decode()
    print(json.dumps({'success': True, 'result': r.strip()}))
except Exception as e:
    print(json.dumps({'success': False, 'result': str(e)}))
)PYEOF";
static constexpr std::string_view SCRIPT_CALCULATOR = R"PYEOF(
#!/usr/bin/env python3
import sys, json, math, re
expr = ' '.join(sys.argv[1:]) if len(sys.argv) > 1 else ''
clean = re.sub(r'[^0-9+\-*/().\s]', '', expr.replace('x', '*'))
try:
    res = eval(clean, {'__builtins__': {}, 'math': math})
    print(json.dumps({'success': True, 'result': expr + ' = ' + str(res)}))
except Exception as e:
    print(json.dumps({'success': False, 'result': str(e)}))
)PYEOF";
static constexpr std::string_view SCRIPT_SCREENSHOT = R"PYEOF(
#!/usr/bin/env python3
import sys, json, os, time
os.makedirs('data/screenshots', exist_ok=True)
fn = 'data/screenshots/shot_' + str(int(time.time())) + '.png'
try:
    import pyautogui
    pyautogui.screenshot().save(fn)
    print(json.dumps({'success': True, 'result': 'Saved: ' + fn}))
except Exception as e:
    print(json.dumps({'success': False, 'result': str(e)}))
)PYEOF";
static constexpr std::string_view SCRIPT_SYSTEM_INFO = R"PYEOF(
#!/usr/bin/env python3
import sys, json
try:
    import psutil
    cpu = psutil.cpu_percent(interval=1)
    ram = psutil.virtual_memory()
    r = 'CPU: ' + str(cpu) + '% | RAM: ' + str(ram.percent) + '% used'
    print(json.dumps({'success': True, 'result': r}))
except Exception as e:
    print(json.dumps({'success': False, 'result': str(e)}))
)PYEOF";
static constexpr std::string_view SCRIPT_OPEN_URL = R"PYEOF(
#!/usr/bin/env python3
import sys, json, webbrowser
url = sys.argv[1] if len(sys.argv) > 1 else ''
if not url.startswith('http'):
    url = 'https://' + url
webbrowser.open(url)
print(json.dumps({'success': True, 'result': 'Opening: ' + url}))
)PYEOF";

struct ScriptEntry { TaskCategory category; std::string_view content; };
static const ScriptEntry SCRIPT_TABLE[] = {
    { TaskCategory::GOOGLE_SEARCH, SCRIPT_GOOGLE_SEARCH },
    { TaskCategory::WEATHER_CHECK, SCRIPT_WEATHER },
    { TaskCategory::CALCULATOR,    SCRIPT_CALCULATOR },
    { TaskCategory::SCREENSHOT,    SCRIPT_SCREENSHOT },
    { TaskCategory::SYSTEM_INFO,   SCRIPT_SYSTEM_INFO },
    { TaskCategory::WEB_OPEN,      SCRIPT_OPEN_URL },
};

std::string AutonomousSkillBuilder::generateScript(TaskCategory category, const FullTrace&) {
    for (const auto& e : SCRIPT_TABLE) if (e.category==category) return std::string(e.content);
    return {};
}

static bool asb_ensureScript(const std::string& path, std::string_view content) {
    namespace fs = std::filesystem; std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    if (ec) { std::cerr << "[SkillSystem] mkdir fail: " << ec.message() << "\n"; return false; }
    if (fs::exists(path)) return true;
    std::ofstream f(path); if (!f.is_open()) return false;
    f << content; return f.good();
}
static std::string asb_makeTriggerKey(const std::string& input, size_t wordLimit) {
    std::istringstream ss(input); std::string word, key; size_t count=0;
    while (count<wordLimit && ss>>word) {
        if (!key.empty()) key+=' ';
        for (unsigned char c : word) key+=static_cast<char>(std::tolower(c));
        ++count;
    }
    return key;
}

std::string AutonomousSkillBuilder::maybeLearn(const FullTrace& trace, SkillRegistry& registry, ToolExecutor&) {
    if (trace.synthesis.finalText.empty()) return {};
    if (!trace.success && trace.synthesis.groundedConfidence < SKILL_CONFIDENCE_THRESHOLD) return {};
    const TaskCategory cat = categorize(trace.input.rawText, trace.pattern);
    if (cat == TaskCategory::GENERAL_QUESTION) {
        if (trace.synthesis.groundedConfidence < FACTUAL_CONFIDENCE_THRESHOLD) return {};
        if (registry.check(trace.input.rawText).matched) return {};
        const std::string triggerKey = asb_makeTriggerKey(trace.input.rawText, TRIGGER_KEY_WORD_LIMIT);
        if (triggerKey.empty()) return {};
        RuntimeSkill skill;
        skill.id             = "fact_" + std::to_string(asb_nowMs());
        skill.name           = "Recall: " + asb_cap(trace.input.rawText, SKILL_NAME_CAP);
        skill.description    = "Cached factual answer";
        skill.createdFrom    = "[factual-recall] " + asb_cap(trace.input.rawText, FACTUAL_CREATED_CAP);
        skill.triggerPatterns= { triggerKey };
        skill.actionTemplate = asb_cap(trace.synthesis.finalText, ACTION_TEMPLATE_CAP);
        skill.actionType     = SkillActionType::CUSTOM_RESPONSE;
        skill.priority       = 0.6f;
        registry.saveSkill(skill); registry.load();
        std::cout << "[SkillSystem] Cached answer: " << skill.name << '\n';
        return skill.id;
    }
    if (cat==TaskCategory::UNKNOWN||cat==TaskCategory::GREETING||cat==TaskCategory::WHATSAPP_MSG) return {};
    if (registry.check(trace.input.rawText).matched) return {};
    const SkillBlueprint bp = buildBlueprint(cat, trace);
    if (!bp.shouldBuild) return {};
    const std::string scriptContent = generateScript(cat, trace);
    if (!scriptContent.empty() && !asb_ensureScript(bp.scriptPath, scriptContent)) return {};
    RuntimeSkill skill;
    skill.id             = "auto_" + std::to_string(asb_nowMs());
    skill.name           = bp.skillName;
    skill.description    = bp.description;
    skill.createdFrom    = "[auto] " + asb_cap(trace.input.rawText, CREATED_FROM_CAP);
    skill.triggerPatterns= bp.triggerPatterns;
    skill.actionTemplate = bp.scriptPath.empty() ? bp.actionTemplate : "SCRIPT:"+bp.scriptPath;
    skill.actionType     = SkillActionType::RUN_SCRIPT;
    skill.priority       = 0.8f;
    registry.saveSkill(skill); registry.load();
    std::cout << "[SkillSystem] Auto-built: " << skill.name << '\n';
    return skill.id;
}
