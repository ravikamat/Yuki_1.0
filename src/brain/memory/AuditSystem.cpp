// AuditSystem.cpp — Trace storage + self-audit (merged from TraceStore + SelfAuditEngine)
#define NOMINMAX
#include "brain/memory/AuditSystem.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <numeric>

// ══════════════════════════════════════════════════════════════════════════════
// TraceStore
// ══════════════════════════════════════════════════════════════════════════════

static std::string jStr(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else                r += c;
    }
    return r + "\"";
}
static std::string jArr(const std::vector<std::string>& v) {
    std::string r = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        r += jStr(v[i]);
        if (i + 1 < v.size()) r += ",";
    }
    return r + "]";
}
static std::string jBool(bool b) { return b ? "true" : "false"; }
static std::string jFloat(float f) {
    std::ostringstream s; s << std::fixed << std::setprecision(3) << f;
    return s.str();
}

TraceStore::TraceStore() : filePath_("data/traces/yuki_traces.jsonl") {}
TraceStore::TraceStore(const std::string& filePath) : filePath_(filePath) {}

bool TraceStore::append(const FullTrace& trace) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::filesystem::create_directories(
            std::filesystem::path(filePath_).parent_path());
    } catch (...) {}
    std::ofstream f(filePath_, std::ios::app);
    if (!f.is_open()) {
        std::cerr << "[TraceStore] Cannot open " << filePath_ << "\n";
        return false;
    }
    f << serializeTrace(trace) << "\n";
    ++sessionCount_;
    return true;
}

int TraceStore::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessionCount_;
}

bool TraceStore::parseTraceLine(const std::string& line, FullTrace& out) const {
    if (line.empty() || line[0] != '{') return false;
    auto strField = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        auto p = line.find(search);
        if (p == std::string::npos) return "";
        p += search.size();
        if (p < line.size() && line[p] == '"') {
            ++p; std::string val;
            while (p < line.size() && line[p] != '"') {
                if (line[p]=='\\' && p+1<line.size()) { p+=2; continue; }
                val += line[p++];
            }
            return val;
        }
        return "";
    };
    auto floatField = [&](const std::string& key) -> float {
        std::string search2 = "\"" + key + "\":";
        auto p = line.find(search2);
        if (p == std::string::npos) return 0.0f;
        p += search2.size();
        while (p < line.size() && line[p] == ' ') ++p;
        if (p < line.size() && (std::isdigit((unsigned char)line[p]) || line[p]=='.')) {
            try { return std::stof(line.substr(p, 10)); } catch(...) {}
        }
        return 0.0f;
    };
    auto boolField = [&](const std::string& key) -> bool {
        return line.find("\"" + key + "\":true") != std::string::npos;
    };
    out.success                        = boolField("success");
    out.pattern.coreIntent             = strField("coreIntent");
    out.pattern.confidence             = floatField("confidence");
    out.pattern.rawInput               = strField("rawText");
    out.verification.satisfied         = boolField("satisfied");
    out.verification.satisfactionScore = floatField("satisfactionScore");
    out.synthesis.groundedConfidence   = floatField("groundedConfidence");
    out.synthesis.complete             = boolField("complete");
    auto epos = line.find("\"entities\":");
    if (epos != std::string::npos) {
        auto start = line.find('[', epos);
        auto end   = line.find(']', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string arr = line.substr(start+1, end-start-1);
            size_t p = 0;
            while ((p = arr.find('"', p)) != std::string::npos) {
                auto e = arr.find('"', p+1);
                if (e == std::string::npos) break;
                std::string ent = arr.substr(p+1, e-p-1);
                if (!ent.empty()) out.pattern.entities.push_back(ent);
                p = e+1;
            }
        }
    }
    return true;
}

std::vector<FullTrace> TraceStore::loadRecent(int maxCount) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FullTrace> result;
    std::ifstream f(filePath_);
    if (!f.is_open()) return result;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty()) lines.push_back(std::move(line));
    int start = std::max(0, static_cast<int>(lines.size()) - maxCount);
    for (int i = start; i < (int)lines.size(); ++i) {
        FullTrace trace;
        if (parseTraceLine(lines[i], trace))
            result.push_back(std::move(trace));
    }
    return result;
}

std::string TraceStore::serializeTrace(const FullTrace& t) const {
    std::ostringstream j;
    j << "{"
      << "\"traceId\":"     << jStr(t.traceId)         << ","
      << "\"startedAtMs\":" << t.startedAtMs            << ","
      << "\"endedAtMs\":"   << t.endedAtMs              << ","
      << "\"success\":"     << jBool(t.success)         << ","
      << "\"input\":{"
        << "\"eventId\":"   << jStr(t.input.eventId)    << ","
        << "\"rawText\":"   << jStr(t.input.rawText)    << ","
        << "\"source\":"    << static_cast<int>(t.input.sourceKind)
      << "},"
      << "\"pattern\":{"
        << "\"requestMode\":" << static_cast<int>(t.pattern.requestMode) << ","
        << "\"outputMode\":"  << static_cast<int>(t.pattern.outputMode)  << ","
        << "\"coreIntent\":"  << jStr(t.pattern.coreIntent)              << ","
        << "\"confidence\":"  << jFloat(t.pattern.confidence)            << ","
        << "\"entities\":"    << jArr(t.pattern.entities)                << ","
        << "\"dependsOnHistory\":"    << jBool(t.pattern.dependsOnHistory)    << ","
        << "\"needsFreshKnowledge\":" << jBool(t.pattern.needsFreshKnowledge)
      << "},"
      << "\"genome\":{"
        << "\"taskId\":"          << jStr(t.genome.taskId)                       << ","
        << "\"searchMode\":"      << static_cast<int>(t.genome.searchMode)       << ","
        << "\"complexityScore\":" << jFloat(t.genome.complexityScore)            << ","
        << "\"noveltyScore\":"    << jFloat(t.genome.noveltyScore)               << ","
        << "\"riskScore\":"       << jFloat(t.genome.riskScore)                  << ","
        << "\"families\":"        << jArr(t.genome.suggestedAgentFamilies)       << ","
        << "\"canAnswerFromMemory\":" << jBool(t.genome.canAnswerFromMemoryOnly) << ","
        << "\"candidateForSkill\":"   << jBool(t.genome.candidateForNewSkill)
      << "},"
      << "\"synthesis\":{"
        << "\"complete\":"           << jBool(t.synthesis.complete)             << ","
        << "\"groundedConfidence\":" << jFloat(t.synthesis.groundedConfidence)
      << "},"
      << "\"verification\":{"
        << "\"satisfied\":"         << jBool(t.verification.satisfied)         << ","
        << "\"satisfactionScore\":" << jFloat(t.verification.satisfactionScore)
      << "}"
    << "}";
    return j.str();
}

// ══════════════════════════════════════════════════════════════════════════════
// SelfAuditEngine
// ══════════════════════════════════════════════════════════════════════════════

std::string SelfAuditEngine::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

bool SelfAuditEngine::isDue(int currentTurnCount) const {
    return (currentTurnCount - lastAuditTurn_) >= auditEveryNTurns && currentTurnCount > 0;
}
void SelfAuditEngine::markAuditRan(int currentTurnCount) { lastAuditTurn_ = currentTurnCount; }

std::string SelfAuditEngine::extractTopic(const FullTrace& trace) const {
    if (!trace.pattern.coreIntent.empty()) {
        std::string ci = trace.pattern.coreIntent;
        auto colon = ci.find(": ");
        if (colon != std::string::npos) ci = ci.substr(colon + 2);
        if (ci.size() >= 3 && ci.size() < 60) return ci;
    }
    if (!trace.pattern.entities.empty()) return trace.pattern.entities[0];
    if (!trace.pattern.rawInput.empty())
        return trace.pattern.rawInput.substr(0, std::min((size_t)40, trace.pattern.rawInput.size()));
    return "";
}

std::map<std::string, std::vector<float>>
SelfAuditEngine::findFailuresByTopic(const std::vector<FullTrace>& traces) const {
    std::map<std::string, std::vector<float>> failMap;
    for (const auto& trace : traces) {
        bool failed = !trace.success || trace.verification.satisfactionScore < 0.42f;
        if (!failed) continue;
        std::string topic = toLower(extractTopic(trace));
        if (topic.empty()) topic = "unknown_topic_failure";
        failMap[topic].push_back(trace.verification.satisfactionScore);
    }
    return failMap;
}

std::vector<std::string>
SelfAuditEngine::findQualityIssues(const std::vector<FullTrace>& traces) const {
    std::vector<std::string> issues;
    int tooLong = 0, tooShort = 0, lowConf = 0;
    for (const auto& trace : traces) {
        int len = static_cast<int>(trace.synthesis.finalText.size());
        if (len > 1500 && trace.verification.satisfactionScore < 0.5f) tooLong++;
        if (len < 30  && trace.verification.satisfactionScore < 0.5f) tooShort++;
        if (trace.pattern.confidence < 0.40f) lowConf++;
    }
    if (tooLong  >= 3) issues.push_back("responses too long for low-confidence answers");
    if (tooShort >= 3) issues.push_back("responses too short, missing context");
    if (lowConf  >= 5) issues.push_back("pattern recognition weak — many unclear inputs");
    return issues;
}

SelfAuditReport SelfAuditEngine::runAudit(
    TraceStore&      traceStore,
    KnowledgeDaemon* knowledge,
    TaskDecomposer&  decomposer,
    ConceptVault&    vault,
    int              lookbackTraces)
{
    SelfAuditReport report;
    auto traces = traceStore.loadRecent(lookbackTraces);
    if (traces.empty()) {
        report.summary = "No traces yet — nothing to audit.";
        lastReport_ = report;
        return report;
    }
    report.tracesAnalyzed = static_cast<int>(traces.size());
    auto failMap = findFailuresByTopic(traces);
    for (auto& [topic, confidences] : failMap) {
        if (topic.empty()) continue;
        float avgConf = std::accumulate(confidences.begin(), confidences.end(), 0.0f)
                        / static_cast<float>(confidences.size());
        int count = static_cast<int>(confidences.size());
        AuditFinding f; f.topic = topic; f.failCount = count; f.avgConf = avgConf;
        if (count >= 4) {
            std::cout << "[SelfAudit] Repeated failure (" << count << "x) on '"
                      << topic << "' — building learning plan\n";
            if (knowledge) knowledge->learnTopic(topic, KnowledgeDaemon::LearnPriority::P0_URGENT);
            try {
                DecompositionTree tree = decomposer.decompose(topic);
                std::vector<std::string> topicList, whyList;
                for (const auto& atom : tree.atoms) {
                    topicList.push_back(atom.topic);
                    whyList.push_back(atom.why);
                }
                vault.indexAtoms(topic, topicList, whyList);
                f.action = "plan_built";
            } catch (...) { f.action = "queued"; }
        } else if (count >= 2) {
            std::cout << "[SelfAudit] Struggling with '" << topic << "' ("
                      << count << "x) — queuing P0 learning\n";
            if (knowledge) knowledge->learnTopic(topic, KnowledgeDaemon::LearnPriority::P0_URGENT);
            f.action = "queued";
        } else { f.action = "noted"; }
        report.findings.push_back(f);
        report.failuresFound++;
    }
    auto qualityIssues = findQualityIssues(traces);
    std::ostringstream ss;
    if (report.failuresFound == 0 && qualityIssues.empty()) {
        ss << "Self-audit complete — " << report.tracesAnalyzed << " turns reviewed, no significant issues found.";
    } else {
        ss << "Self-audit: reviewed " << report.tracesAnalyzed << " turns. ";
        if (report.failuresFound > 0) {
            ss << "Found " << report.failuresFound << " weak area(s): ";
            for (size_t i = 0; i < report.findings.size() && i < 3; ++i) {
                if (i) ss << ", ";
                ss << "'" << report.findings[i].topic << "' ("
                   << report.findings[i].failCount << "x, " << report.findings[i].action << ")";
            }
            ss << ". ";
        }
        if (!qualityIssues.empty()) ss << "Quality note: " << qualityIssues[0] << ".";
    }
    report.summary = ss.str();
    std::cout << "[SelfAudit] " << report.summary << "\n";
    lastReport_ = report;
    return report;
}
