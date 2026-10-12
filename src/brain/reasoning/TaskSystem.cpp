// TaskSystem.cpp — Plan generation + deep atomic decomposition (merged from TaskPlanner + TaskDecomposer)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "brain/reasoning/TaskSystem.h"
#include "brain/swarm/CognitiveStore.h"
#include "core/SystemConfig.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <filesystem>

using yuki::brain::learning::KnowledgeDaemon;

// ══════════════════════════════════════════════════════════════════════════════
// TaskPlanner
// ══════════════════════════════════════════════════════════════════════════════

static std::string ts_toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
static bool ts_has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

std::string TaskPlanner::makePlanId() {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return "plan_" + std::to_string(ms);
}

TaskPlan TaskPlanner::buildPlan(const CognitiveSituation& sit) const {
    const std::string lower = ts_toLower(sit.pattern.rawInput);
    TaskPlan plan;
    if      (ts_has(lower,"website")||ts_has(lower,"web page")||ts_has(lower,"webpage")||ts_has(lower,"html")||ts_has(lower,"frontend"))
        plan = buildWebsitePlan(sit);
    else if (ts_has(lower,"trading")||ts_has(lower,"trade")||ts_has(lower,"stock")||ts_has(lower,"invest")||ts_has(lower,"market")||ts_has(lower,"crypto"))
        plan = buildTradingPlan(sit);
    else if (ts_has(lower,"research")||ts_has(lower,"analyse")||ts_has(lower,"analyze")||ts_has(lower,"compare")||ts_has(lower,"study")||ts_has(lower,"investigate"))
        plan = buildResearchPlan(sit);
    else
        plan = buildGenericPlan(sit);
    plan.planId = makePlanId(); plan.status = PlanStatus::PENDING_APPROVAL;
    savePlan(plan); lastPlan_ = plan; return plan;
}

TaskPlan TaskPlanner::buildWebsitePlan(const CognitiveSituation& sit) const {
    TaskPlan plan;
    plan.goal = "Create a website: " + sit.pattern.coreIntent;
    plan.requestMode = "IMPLEMENTATION"; plan.estimatedDuration = "5–15 minutes";
    std::string siteType = "general website";
    const auto& entities = sit.pattern.entities;
    if (!entities.empty()) siteType = entities[0] + " website";
    plan.steps = {
        {1,"Research: gather requirements for the "+siteType,"LocalKnowledgeScout","list of features and structure",true,false,false},
        {2,"Design: choose layout, color scheme, and page structure","SynthesisEngine","design document",false,false,false},
        {3,"Create project folder: website_"+(entities.empty()?"project":entities[0]),"ToolExecutor:CreateFolder","folder created",false,true,false},
        {4,"Generate index.html with semantic HTML structure","ToolExecutor:WriteFile","index.html created",false,true,false},
        {5,"Generate styles.css with responsive layout","ToolExecutor:WriteFile","styles.css created",false,true,false},
        {6,"Generate script.js with interactive functionality","ToolExecutor:WriteFile","script.js created",false,true,false},
        {7,"Open website in default browser for preview","ToolExecutor:OpenBrowser","browser opened",false,false,false},
    };
    plan.requiredTools = {"ToolExecutor:CreateFolder","ToolExecutor:WriteFile","ToolExecutor:OpenBrowser"};
    plan.requiredPermissions = {"WRITE_FILES (website folder in current directory)","OPEN_BROWSER"};
    plan.risks = {"Will create files in current working directory","Will open browser window"};
    return plan;
}

TaskPlan TaskPlanner::buildTradingPlan(const CognitiveSituation& sit) const {
    TaskPlan plan;
    plan.goal = "Trading R&D and automation: " + sit.pattern.coreIntent;
    plan.requestMode = "RESEARCH"; plan.estimatedDuration = "30–60 minutes (R&D phase only)";
    plan.steps = {
        {1,"R&D: research trading strategies from knowledge base","LocalKnowledgeScout","strategy summary",true,false,false},
        {2,"R&D: learn about risk management and position sizing","LocalKnowledgeScout","risk framework",true,false,false},
        {3,"Design: create a trading strategy specification document","ToolExecutor:WriteFile","strategy spec",false,true,false},
        {4,"Code: write a paper-trading simulator (no real money)","ToolExecutor:WriteFile","simulator.py",false,true,true},
        {5,"Test: run simulator on historical data (paper trading only)","ToolExecutor:RunScript","backtest results",false,false,true},
        {6,"Report: generate performance report","ToolExecutor:WriteFile","report.txt",false,true,false},
        {7,"[REQUIRES EXPLICIT APPROVAL] Live trading: connect to broker API","ToolExecutor:RunScript","live connection",true,false,true},
    };
    plan.requiredTools = {"LocalKnowledgeScout","ToolExecutor:WriteFile","ToolExecutor:RunScript"};
    plan.requiredPermissions = {"WRITE_FILES (trading_project/ folder)","RUN_PYTHON_SCRIPT","INTERNET_ACCESS (for live data — only after separate approval)","⚠️  Step 7 requires SEPARATE EXPLICIT APPROVAL for live trading"};
    plan.risks = {"Paper trading only by default — no real money at risk","Live trading (Step 7) requires separate permission and API keys","Past performance does not guarantee future results"};
    return plan;
}

TaskPlan TaskPlanner::buildResearchPlan(const CognitiveSituation& sit) const {
    TaskPlan plan;
    plan.goal = "Research: " + sit.pattern.coreIntent;
    plan.requestMode = "RESEARCH"; plan.estimatedDuration = "2–5 minutes";
    std::string topic = sit.pattern.coreIntent;
    auto colon = topic.find(": "); if (colon!=std::string::npos) topic=topic.substr(colon+2);
    plan.steps = {
        {1,"Search internal knowledge base for '"+topic+"'","LocalKnowledgeScout","known facts",false,false,false},
        {2,"Search conversation history for prior context","HistoryDiver","context summary",false,false,false},
        {3,"Identify what is unknown or needs fresh data","Verifier","gap list",false,false,false},
        {4,"Synthesize findings into a structured report","SynthesisEngine","research report",false,false,false},
        {5,"Save report to data/research/<topic>.txt","ToolExecutor:WriteFile","saved report",false,true,false},
    };
    plan.requiredTools={"LocalKnowledgeScout","ToolExecutor:WriteFile"};
    plan.requiredPermissions={"WRITE_FILES (data/research/ folder)"};
    plan.risks={"Creates a file in data/research/"}; return plan;
}

TaskPlan TaskPlanner::buildGenericPlan(const CognitiveSituation& sit) const {
    TaskPlan plan;
    plan.goal=sit.pattern.coreIntent; plan.requestMode="IMPLEMENTATION"; plan.estimatedDuration="Varies";
    plan.steps = {
        {1,"Understand: clarify what is needed for '"+sit.pattern.coreIntent+"'","IntentAnalyst","requirements",false,false,false},
        {2,"Research: gather knowledge relevant to the task","LocalKnowledgeScout","knowledge base",false,false,false},
        {3,"Plan: identify tools and steps required","TaskGenomeBuilder","detailed sub-steps",false,false,false},
        {4,"Execute: implement the plan step by step","ToolExecutor","result",false,true,false},
        {5,"Verify: check output against requirements","Verifier","verification report",false,false,false},
    };
    plan.requiredTools={"LocalKnowledgeScout","ToolExecutor"};
    plan.requiredPermissions={"DEPENDS_ON_TASK"};
    plan.risks={"Will be clarified once task is better understood"}; return plan;
}

std::string TaskPlanner::formatForApproval(const TaskPlan& plan) const {
    std::ostringstream ss;
    ss << "\n🧠 YUKI TASK PLAN\n" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    ss << "Goal:      " << plan.goal << "\n" << "Duration:  " << plan.estimatedDuration << "\n" << "Plan ID:   " << plan.planId << "\n\n";
    ss << "STEPS:\n";
    for (const auto& s : plan.steps) {
        ss << "  " << s.stepNo << ". " << s.action << "\n" << "     Tool: " << s.tool << "\n";
        if (s.writesFiles) ss << "     ⚠️  Writes files\n";
        if (s.requiresNet) ss << "     🌐  Needs internet\n";
        if (s.runsCode)    ss << "     ⚙️  Runs code\n";
    }
    ss << "\nPERMISSIONS REQUIRED:\n";
    for (const auto& p : plan.requiredPermissions) ss << "  • " << p << "\n";
    ss << "\nRISKS:\n";
    for (const auto& r : plan.risks) ss << "  • " << r << "\n";
    ss << "\nPlan saved: " << plan.savedPath << "\n";
    ss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    ss << "Say 'yes' or 'approve' to start, 'no' or 'cancel' to abort.\n";
    return ss.str();
}

bool TaskPlanner::savePlan(TaskPlan& plan) const {
    try { std::filesystem::create_directories("data/plans"); } catch (...) {}
    std::string path = "data/plans/" + plan.planId + ".json";
    plan.savedPath = path;
    std::ofstream f(path); if (!f.is_open()) return false;
    f << "{\n  \"planId\": \"" << plan.planId << "\",\n  \"goal\": \"" << plan.goal << "\",\n  \"status\": \"PENDING_APPROVAL\",\n  \"steps\": [\n";
    for (size_t i=0; i<plan.steps.size(); ++i) {
        const auto& s=plan.steps[i];
        f << "    {\"step\":" << s.stepNo << ",\"action\":\"" << s.action << "\",\"tool\":\"" << s.tool << "\"}";
        if (i+1<plan.steps.size()) f << ","; f << "\n";
    }
    f << "  ]\n}\n";
    std::cout << "[TaskPlanner] Plan saved: " << path << "\n"; return true;
}

bool TaskPlanner::approvePlan(const std::string& planId) {
    lastPlan_.status = PlanStatus::APPROVED;
    std::string path = "data/plans/" + planId + ".json";
    std::ifstream in(path); if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()); in.close();
    auto p = content.find("PENDING_APPROVAL");
    if (p!=std::string::npos) content.replace(p, 16, "APPROVED");
    std::ofstream out(path); out << content; return true;
}

bool TaskPlanner::isApprovalSignal(const std::string& input) const {
    std::string low = ts_toLower(input);
    return ts_has(low,"yes")||ts_has(low,"approve")||ts_has(low,"go ahead")||ts_has(low,"confirmed")||
           ts_has(low,"ok do it")||ts_has(low,"start it")||ts_has(low,"proceed")||ts_has(low,"execute")||ts_has(low,"run it");
}
bool TaskPlanner::isRejectionSignal(const std::string& input) const {
    std::string low = ts_toLower(input);
    // Word-boundary check for "no" — avoids false matches on "know", "not", "now", etc.
    auto hasWordNo = [&]() -> bool {
        size_t pos = 0;
        while ((pos = low.find("no", pos)) != std::string::npos) {
            bool prevOk = (pos == 0 || !std::isalpha(static_cast<unsigned char>(low[pos-1])));
            bool nextOk = (pos + 2 >= low.size() || !std::isalpha(static_cast<unsigned char>(low[pos+2])));
            if (prevOk && nextOk) return true;
            pos += 2;
        }
        return false;
    };
    // Word-boundary check for "stop" — avoids "nonstop", "stop" inside other words
    auto hasWordStop = [&]() -> bool {
        size_t pos = 0;
        while ((pos = low.find("stop", pos)) != std::string::npos) {
            bool prevOk = (pos == 0 || !std::isalpha(static_cast<unsigned char>(low[pos-1])));
            bool nextOk = (pos + 4 >= low.size() || !std::isalpha(static_cast<unsigned char>(low[pos+4])));
            if (prevOk && nextOk) return true;
            pos += 4;
        }
        return false;
    };
    return hasWordNo() || ts_has(low,"cancel") || hasWordStop() ||
           ts_has(low,"abort") || ts_has(low,"don't do") || ts_has(low,"reject");
}

// ══════════════════════════════════════════════════════════════════════════════
// TaskDecomposer
// ══════════════════════════════════════════════════════════════════════════════

static const std::string CUSTOM_TASK_FILE = "data/brain/custom_tasks.json";
static const std::string SCRIPT_DIR_TD    = "data/scripts/";

std::string TaskDecomposer::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool TaskDecomposer::hasWord(const std::string& h, const std::string& n) { return h.find(n)!=std::string::npos; }

sqlite3* TaskDecomposer::getProceduralDb() {
    auto store = yuki::CognitiveStoreFactory::instance().getStore(yuki::StoreScope::SHARED_PROCEDURAL, "shared");
    if (!store) return nullptr;
    return store->raw();
}

void TaskDecomposer::loadDomainKnowledgeFromDB() {
    sqlite3* db = getProceduralDb();
    if (!db) {
        loadDomainKnowledgeFallback();
        return;
    }

    const char* sql = R"(
        SELECT d.id, d.domain_name, d.description, d.fallback_priority
        FROM domain_knowledge_templates d
        ORDER BY d.fallback_priority ASC, d.id ASC
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        loadDomainKnowledgeFallback();
        return;
    }

    domainDB_.clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int domainId = sqlite3_column_int(stmt, 0);
        DomainKnowledgeEntry entry;
        const char* dname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.domainName = dname ? dname : "";

        // Load detection keywords
        sqlite3_stmt* kstmt = nullptr;
        const char* ksql = "SELECT keyword, weight FROM domain_detection_keywords WHERE domain_id = ?";
        if (sqlite3_prepare_v2(db, ksql, -1, &kstmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(kstmt, 1, domainId);
            while (sqlite3_step(kstmt) == SQLITE_ROW) {
                const char* kw = reinterpret_cast<const char*>(sqlite3_column_text(kstmt, 0));
                if (kw) entry.detectionKeywords.emplace_back(kw);
            }
            sqlite3_finalize(kstmt);
        }

        // Load trigger patterns
        sqlite3_stmt* pstmt = nullptr;
        const char* psql = "SELECT pattern, is_regex FROM domain_trigger_patterns WHERE domain_id = ?";
        if (sqlite3_prepare_v2(db, psql, -1, &pstmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(pstmt, 1, domainId);
            while (sqlite3_step(pstmt) == SQLITE_ROW) {
                const char* pat = reinterpret_cast<const char*>(sqlite3_column_text(pstmt, 0));
                if (pat) entry.triggerPatterns.emplace_back(pat);
            }
            sqlite3_finalize(pstmt);
        }

        // Load action atoms
        sqlite3_stmt* astmt = nullptr;
        const char* asql = R"(
            SELECT atom_key, atom_name, atom_description, required_libraries, required_apis, execution_order
            FROM action_atoms WHERE domain_id = ? ORDER BY execution_order ASC
        )";
        if (sqlite3_prepare_v2(db, asql, -1, &astmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(astmt, 1, domainId);
            while (sqlite3_step(astmt) == SQLITE_ROW) {
                AtomicTask atom;
                const char* key = reinterpret_cast<const char*>(sqlite3_column_text(astmt, 0));
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(astmt, 1));
                const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(astmt, 2));
                atom.id = key ? key : "";
                atom.topic = name ? name : "";
                atom.why = desc ? desc : "";
                atom.learnQuery = atom.topic + " tutorial how to";
                atom.estimatedMins = 3;
                atom.isBlocker = (sqlite3_column_int(astmt, 5) <= 2);
                entry.atoms.push_back(std::move(atom));
            }
            sqlite3_finalize(astmt);
        }

        domainDB_.push_back(std::move(entry));
    }
    sqlite3_finalize(stmt);
    domainsLoaded_ = true;

    if (domainDB_.empty()) {
        loadDomainKnowledgeFallback();
    }
}

void TaskDecomposer::loadDomainKnowledgeFallback() {
    domainDB_.clear();
    domainsLoaded_ = true;
}

TaskDecomposer::TaskDecomposer() {
    loadDomainKnowledgeFromDB();
    load();
}

bool TaskDecomposer::isNewTaskRequest(const std::string& input) {
    const std::string L = toLower(input);
    if (hasWord(L,"learn task")||hasWord(L,"new task")||hasWord(L,"task type")||hasWord(L,"i want to learn")||
        hasWord(L,"teach yourself")||hasWord(L,"learn to do")||hasWord(L,"learn how to build")||
        hasWord(L,"learn how to create")||hasWord(L,"want yuki to learn")||hasWord(L,"yuki learn"))
        return true;
    if ((hasWord(L,"split")||hasWord(L,"break")||hasWord(L,"decompose"))&&
        (hasWord(L,"subtask")||hasWord(L,"sub-task")||hasWord(L,"step")||hasWord(L,"task")||hasWord(L,"learn")))
        return true;
    if (hasWord(L,"learn")&&(hasWord(L,"task")||hasWord(L,"it")||hasWord(L,"this")||hasWord(L,"that")||hasWord(L,"about")||hasWord(L,"to do")))
        return true;
    return false;
}

std::string TaskDecomposer::detectDomain(const std::string& input) const {
    const std::string L=toLower(input); int bestScore=0; std::string bestDomain;
    for (const auto& entry : domainDB_) {
        int score=0; for (const auto& kw : entry.detectionKeywords) if (hasWord(L,kw)) score++;
        if (score>bestScore) { bestScore=score; bestDomain=entry.domainName; }
    }
    for (const auto& ct : customTasks_) {
        int score=0; for (const auto& kw : ct.keywords) if (hasWord(L,kw)) score++;
        if (score>bestScore) { bestScore=score; bestDomain=ct.name; }
    }
    return bestDomain;
}

std::string TaskDecomposer::extractTaskSubject(const std::string& raw) const {
    const std::string L=toLower(raw);
    for (const char* trigger : {"split ","break "}) {
        auto p=L.find(trigger);
        if (p!=std::string::npos) {
            std::string after=raw.substr(p+strlen(trigger));
            for (const char* trim : {" into subtask"," into step"," into sub-task"," and learn"," in subtask"," in sub-task"}) {
                auto tp=toLower(after).find(trim); if (tp!=std::string::npos) after=after.substr(0,tp);
            }
            for (const char* art : {"the ","this ","a ","an "}) {
                if (toLower(after).rfind(art,0)==0) after=after.substr(strlen(art));
            }
            if (after.size()>3) return after;
        }
    }
    for (const char* pfx : {"i want to learn ","yuki learn ","want yuki to learn ","learn how to build ","learn how to create ","learn to do ","learn task: ","new task: ","new task type: ","learn task type: ","teach yourself ","learn "}) {
        auto pos=L.find(pfx);
        if (pos!=std::string::npos) {
            std::string after=raw.substr(pos+strlen(pfx));
            for (const char* trim : {" and learn"," then learn"," it"," please"}) {
                auto tp=toLower(after).rfind(trim);
                if (tp!=std::string::npos&&tp>after.size()/2) after=after.substr(0,tp);
            }
            if (after.size()>3) return after;
        }
    }
    return raw;
}

DecompositionTree TaskDecomposer::decompose(const std::string& desc) {
    std::string domain = detectDomain(desc);
    for (const auto& entry : domainDB_) if (entry.domainName==domain) return decomposeDomain(entry, desc);
    return decomposeGeneric(desc);
}

DecompositionTree TaskDecomposer::decomposeDomain(const DomainKnowledgeEntry& e, const std::string&) const {
    DecompositionTree tree;
    tree.domain=e.domainName; tree.goalSummary="Learn and automate: "+e.domainName;
    tree.atoms=e.atoms; tree.libraries=e.libraries; tree.apis=e.apis;
    tree.triggerPatterns=e.triggerPatterns; tree.totalAtoms=(int)e.atoms.size();
    int totalMins=0; for (const auto& a : e.atoms) totalMins+=a.estimatedMins;
    tree.estimatedLearningHours=(totalMins+59)/60;
    std::string scriptName=toLower(e.domainName);
    std::replace(scriptName.begin(), scriptName.end(), ' ', '_');
    tree.scaffoldPath=SCRIPT_DIR_TD+scriptName+".py";
    tree.scaffoldCode=generateScaffold(tree); return tree;
}

DecompositionTree TaskDecomposer::decomposeGeneric(const std::string& desc) const {
    DecompositionTree tree;
    tree.domain="Custom: "+desc.substr(0,40); tree.goalSummary="Learn and automate: "+desc;
    auto kws=extractKeywords(desc); int idx=1;
    for (const auto& kw : kws) {
        AtomicTask a; a.id="g"+std::to_string(idx++); a.topic=kw;
        a.why="Required for: "+desc.substr(0,40); a.learnQuery=kw+" python tutorial how to";
        a.estimatedMins=3; a.isBlocker=(idx<=2); tree.atoms.push_back(a);
    }
    tree.totalAtoms=(int)tree.atoms.size();
    int half=(int)kws.size()/2; tree.estimatedLearningHours=half>1?half:1;
    tree.triggerPatterns=kws;
    std::string sn=toLower(desc.substr(0,20));
    std::replace(sn.begin(), sn.end(), ' ', '_');
    tree.scaffoldPath=SCRIPT_DIR_TD+sn+".py";
    tree.scaffoldCode=generateScaffold(tree); return tree;
}

std::vector<std::string> TaskDecomposer::extractKeywords(const std::string& text) const {
    const std::vector<std::string> STOP={"i","want","to","learn","a","the","and","or","how","build","create","make","do","new","task","type","yuki","please","can","you"};
    std::istringstream ss(toLower(text)); std::string word; std::vector<std::string> kws;
    while (ss>>word) {
        word.erase(std::remove_if(word.begin(), word.end(), [](char c){ return !std::isalpha(c); }), word.end());
        if (word.size()<3) continue;
        bool stop=false; for (const auto& sw : STOP) if (word==sw) { stop=true; break; }
        if (!stop) kws.push_back(word); if (kws.size()>=8) break;
    }
    return kws;
}

std::string TaskDecomposer::generateScaffold(const DecompositionTree& tree) const {
    const std::string NL="\n"; std::string s;
    s+="#!/usr/bin/env python3"+NL+"# Auto-generated scaffold for: "+tree.domain+NL;
    s+="# Built by Yuki TaskDecomposer"+NL+"# Learning atoms: "+std::to_string(tree.totalAtoms)+NL+NL;
    s+="import sys, json, os"+NL;
    for (const auto& lib : tree.libraries) s+="# requires: pip install "+lib+NL;
    s+=NL+"# ── Learning atoms (each becomes a function) ──"+NL;
    for (const auto& atom : tree.atoms) {
        std::string fn=toLower(atom.topic); std::replace(fn.begin(), fn.end(), ' ', '_');
        fn.erase(std::remove_if(fn.begin(), fn.end(), [](char c){ return !std::isalnum(c)&&c!='_'; }), fn.end());
        s+="def "+fn+"():"+NL+"    \"\"\""+atom.topic+" — "+atom.why+"\"\"\""+NL;
        s+="    # TODO: implement after learning: "+atom.learnQuery+NL+"    pass"+NL+NL;
    }
    s+="def main():"+NL+"    args = ' '.join(sys.argv[1:])"+NL;
    for (const auto& atom : tree.atoms) {
        std::string fn=toLower(atom.topic); std::replace(fn.begin(), fn.end(), ' ', '_');
        fn.erase(std::remove_if(fn.begin(), fn.end(), [](char c){ return !std::isalnum(c)&&c!='_'; }), fn.end());
        s+="    "+fn+"()"+NL;
    }
    s+="    print(json.dumps({'success': True, 'result': 'Task complete: "+tree.domain+"'}))"+NL;
    s+=NL+"if __name__ == '__main__':"+NL+"    main()"+NL; return s;
}

void TaskDecomposer::queueLearning(const DecompositionTree& tree, KnowledgeDaemon* daemon) {
    if (!daemon) return;
    std::cout << "[TaskSystem] Queuing " << tree.atoms.size() << " atoms for " << tree.domain << "\n";
    for (const auto& atom : tree.atoms) {
        auto prio = atom.isBlocker ? KnowledgeDaemon::LearnPriority::P0_URGENT : KnowledgeDaemon::LearnPriority::P1_HIGH;
        daemon->learnTopic(atom.learnQuery, prio);
        std::cout << "  [Queue] " << atom.topic << " → " << atom.learnQuery << "\n";
    }
    for (const auto& api : tree.apis)
        daemon->learnTopic(api+" python tutorial", KnowledgeDaemon::LearnPriority::P1_HIGH);
}

RuntimeSkill TaskDecomposer::buildSkill(const DecompositionTree& tree) const {
    using namespace std::chrono;
    auto ms=duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    RuntimeSkill skill;
    skill.id="task_"+std::to_string(ms); skill.name=tree.domain; skill.description=tree.goalSummary;
    skill.createdFrom="[TaskDecomposer] "+tree.domain; skill.triggerPatterns=tree.triggerPatterns;
    skill.actionTemplate="SCRIPT:"+tree.scaffoldPath; skill.actionType=SkillActionType::RUN_SCRIPT;
    skill.priority=0.85f; return skill;
}

bool TaskDecomposer::writeScaffold(const DecompositionTree& tree) const {
    try {
        std::filesystem::create_directories(SCRIPT_DIR_TD);
        std::ofstream f(tree.scaffoldPath); if (!f.is_open()) { std::cerr<<"[TaskSystem] Failed to open "<<tree.scaffoldPath<<"\n"; return false; }
        f<<tree.scaffoldCode; std::cout<<"[TaskSystem] Wrote scaffold: "<<tree.scaffoldPath<<"\n"; return true;
    } catch (const std::exception& e) { std::cerr<<"[TaskSystem] Exception: "<<e.what()<<"\n"; return false; }
    catch (...) { std::cerr<<"[TaskSystem] Unknown exception\n"; return false; }
}

std::string TaskDecomposer::formatPlan(const DecompositionTree& tree) const {
    const std::string LINE="────────────────────────────────────────────────\n";
    std::ostringstream ss;
    ss<<LINE<<" LEARNING PLAN: "<<tree.domain<<"\n"<<" Goal: "<<tree.goalSummary<<"\n";
    ss<<" Subtasks: "<<tree.totalAtoms<<"  |  Est. time: ~"<<tree.estimatedLearningHours<<" hour(s)\n"<<LINE;
    std::vector<const AtomicTask*> phase1, phase2, phase3;
    for (const auto& a : tree.atoms) {
        if (a.isBlocker) phase1.push_back(&a);
        else if (phase1.size()+phase2.size()<(size_t)(tree.totalAtoms*0.6)) phase2.push_back(&a);
        else phase3.push_back(&a);
    }
    auto printAtom = [&](const AtomicTask* a, int idx, bool started) {
        ss<<"  "<<(started?"◆":"○")<<" ["<<idx<<"] "<<a->topic<<"\n";
        ss<<"      Why: "<<a->why<<"\n";
        ss<<"      Status: "<<(started?"Queued for background learning ✓":"Pending")<<"\n";
    };
    if (!phase1.empty()) { ss<<"\nPHASE 1 — FOUNDATION (learning starts now)\n"; int idx=1; for (const auto* a:phase1) printAtom(a,idx++,true); }
    if (!phase2.empty()) { ss<<"\nPHASE 2 — CORE SKILLS\n"; int idx=(int)phase1.size()+1; for (const auto* a:phase2) printAtom(a,idx++,false); }
    if (!phase3.empty()) { ss<<"\nPHASE 3 — ADVANCED\n"; int idx=(int)phase1.size()+(int)phase2.size()+1; for (const auto* a:phase3) printAtom(a,idx++,false); }
    ss<<"\n";
    if (!tree.libraries.empty()) { ss<<"LIBRARIES: "; for (size_t j=0;j<tree.libraries.size();j++){if(j)ss<<", ";ss<<tree.libraries[j];} ss<<"\n"; }
    if (!tree.apis.empty())      { ss<<"APIS:      "; for (size_t j=0;j<tree.apis.size();j++){if(j)ss<<", ";ss<<tree.apis[j];} ss<<"\n"; }
    ss<<"SCAFFOLD:  "<<tree.scaffoldPath<<"  ← generated\n\n";
    ss<<"Background learning is queued at P0 priority.\n";
    ss<<"Say 'task status'       → check progress\n";
    ss<<"Say 'what is [subtask]' → recall any concept instantly\n";
    ss<<"Say 'run "<<tree.domain<<"' → execute when ready\n"<<LINE;
    return ss.str();
}

DecompositionTree TaskDecomposer::registerCustomTask(const std::string& name, const std::vector<std::string>& hints) {
    CustomTaskDef ct; ct.name=name; ct.description="User-defined task type: "+name;
    ct.keywords=extractKeywords(name);
    for (const auto& h : hints) { auto hkw=extractKeywords(h); ct.keywords.insert(ct.keywords.end(),hkw.begin(),hkw.end()); }
    ct.userHints=hints; customTasks_.push_back(ct); save();
    std::string desc=name; for (const auto& h : hints) desc+=" "+h;
    return decomposeGeneric(desc);
}

void TaskDecomposer::save() const {
    try {
        std::filesystem::create_directories("data/brain");
        std::ofstream f(CUSTOM_TASK_FILE); if (!f.is_open()) return;
        f<<"[\n";
        for (size_t i=0;i<customTasks_.size();i++) {
            const auto& ct=customTasks_[i];
            f<<"  {\"name\":\""<<ct.name<<"\",\"description\":\""<<ct.description<<"\",\"keywords\":[";
            for (size_t j=0;j<ct.keywords.size();j++){if(j)f<<",";f<<"\""<<ct.keywords[j]<<"\"";} f<<"]}";
            if (i+1<customTasks_.size()) f<<","; f<<"\n";
        }
        f<<"]\n";
    } catch (const std::exception& e) { std::cerr<<"[TaskSystem] Exception saving: "<<e.what()<<"\n"; }
    catch (...) { std::cerr<<"[TaskSystem] Unknown exception saving.\n"; }
}

void TaskDecomposer::load() {
    if (!std::filesystem::exists(CUSTOM_TASK_FILE)) return;
    std::ifstream f(CUSTOM_TASK_FILE); if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t p=0;
    while ((p=content.find("\"name\":",p))!=std::string::npos) {
        auto q1=content.find('"',p+7), q2=content.find('"',q1+1);
        if (q1==std::string::npos||q2==std::string::npos) break;
        CustomTaskDef ct; ct.name=content.substr(q1+1,q2-q1-1);
        ct.keywords=extractKeywords(ct.name); customTasks_.push_back(ct); p=q2+1;
    }
    if (!customTasks_.empty()) std::cout<<"[TaskSystem] Loaded "<<customTasks_.size()<<" custom task types\n";
}

std::string TaskDecomposer::makeAtomId(int i) const { return "g"+std::to_string(i); }
