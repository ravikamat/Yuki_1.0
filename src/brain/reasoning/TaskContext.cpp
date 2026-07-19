// TaskContext.cpp — Task genome + cognitive situation (merged from TaskGenomeBuilder + SituationBuilder)
#define NOMINMAX
#include "brain/reasoning/TaskContext.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <cctype>

static uint64_t tctx_nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ══════════════════════════════════════════════════════════════════════════════
// TaskGenomeBuilder
// ══════════════════════════════════════════════════════════════════════════════

TaskGenome TaskGenomeBuilder::build(const CognitiveSituation& situation, int turnIndex) const {
    const PatternFrame& frame = situation.pattern;
    TaskGenome g;
    g.taskId       = "genome_" + std::to_string(tctx_nowMs());
    g.coreGoal     = frame.coreIntent;
    g.searchMode   = chooseSearchMode(frame);
    g.complexityScore = scoreComplexity(frame);
    g.noveltyScore    = scoreNovelty(frame, turnIndex);
    g.riskScore       = scoreRisk(frame);
    g.suggestedAgentFamilies = chooseFamilies(frame, g.complexityScore);
    g.canAnswerFromMemoryOnly = (g.complexityScore < 0.4f &&
                                  frame.requestMode == RequestMode::QUESTION &&
                                  frame.unknownSlots.empty());
    g.requiresExternalGrounding = frame.needsFreshKnowledge ||
                                   g.searchMode == SearchMode::WEB_HEAVY;
    g.candidateForNewSkill = (g.complexityScore < 0.5f &&
                               frame.confidence > 0.75f &&
                               (frame.requestMode == RequestMode::COMMAND ||
                                frame.requestMode == RequestMode::QUESTION));
    for (const auto& sg : situation.goals.secondaryGoals) g.subGoals.push_back(sg);
    g.unresolvedFacts = frame.unknownSlots;
    return g;
}

float TaskGenomeBuilder::scoreComplexity(const PatternFrame& frame) const {
    float score = 0.0f;
    switch (frame.requestMode) {
        case RequestMode::IMPLEMENTATION: score += 0.5f;  break;
        case RequestMode::DESIGN:         score += 0.6f;  break;
        case RequestMode::RESEARCH:       score += 0.4f;  break;
        case RequestMode::QUESTION:       score += 0.2f;  break;
        case RequestMode::COMMAND:        score += 0.15f; break;
        default:                          score += 0.1f;  break;
    }
    score += frame.unknownSlots.size() * 0.05f;
    score += frame.entities.size()     * 0.02f;
    if (frame.dependsOnHistory)    score += 0.1f;
    if (frame.needsFreshKnowledge) score += 0.15f;
    return std::min(score, 1.0f);
}

float TaskGenomeBuilder::scoreNovelty(const PatternFrame& frame, int turnIndex) const {
    float novelty = 0.5f;
    if (turnIndex < 2)           novelty += 0.3f;
    if (!frame.dependsOnHistory) novelty += 0.1f;
    if (frame.entities.empty())  novelty += 0.1f;
    return std::min(novelty, 1.0f);
}

float TaskGenomeBuilder::scoreRisk(const PatternFrame& frame) const {
    float risk = 0.05f;
    if (frame.requestMode == RequestMode::COMMAND)        risk += 0.2f;
    if (frame.requestMode == RequestMode::IMPLEMENTATION) risk += 0.15f;
    if (frame.needsFreshKnowledge) risk += 0.1f;
    return std::min(risk, 1.0f);
}

SearchMode TaskGenomeBuilder::chooseSearchMode(const PatternFrame& frame) const {
    if (frame.requestMode == RequestMode::IMPLEMENTATION ||
        frame.requestMode == RequestMode::DESIGN)
        return SearchMode::CODEBASE_HEAVY;
    if (frame.needsFreshKnowledge || frame.requestMode == RequestMode::RESEARCH)
        return SearchMode::HYBRID_LOCAL_WEB;
    return SearchMode::INTERNAL_ONLY;
}

std::vector<std::string> TaskGenomeBuilder::chooseFamilies(
    const PatternFrame& frame, float complexity) const {
    std::vector<std::string> families;
    families.push_back("IntentAnalyst");
    if (frame.dependsOnHistory) families.push_back("HistoryDiver");
    if (complexity > 0.3f) families.push_back("LocalKnowledgeScout");
    if (frame.requestMode == RequestMode::IMPLEMENTATION ||
        frame.requestMode == RequestMode::DESIGN)
        families.push_back("CodeArchaeologist");
    SearchMode sm = chooseSearchMode(frame);
    if (frame.needsFreshKnowledge || sm == SearchMode::WEB_HEAVY ||
        sm == SearchMode::HYBRID_LOCAL_WEB)
        families.push_back("WebReconAgent");
    if (complexity > 0.5f) families.push_back("ContradictionHunter");
    families.push_back("Verifier");
    return families;
}

// ══════════════════════════════════════════════════════════════════════════════
// SituationBuilder
// ══════════════════════════════════════════════════════════════════════════════

SituationBuilder::SituationBuilder() = default;

CognitiveSituation SituationBuilder::build(
    const PatternFrame&       frame,
    const ConversationMemory& memory,
    int                       turnIndex) const
{
    CognitiveSituation sit;
    sit.pattern = frame;
    sit.goals.primaryGoal = frame.coreIntent;
    if (!frame.desiredOutcome.empty())
        sit.goals.secondaryGoals.push_back(frame.desiredOutcome);
    for (const auto& c : frame.inferredConstraints)
        sit.goals.secondaryGoals.push_back(c);
    sit.momentum.turnIndex           = turnIndex;
    sit.momentum.activeProjectThread = (turnIndex > 3);
    if (frame.dependsOnHistory) {
        auto turns = memory.getRecentTurns(5);
        for (const auto& t : turns)
            if ((t.speaker == "You" || t.speaker == "Voice") && !t.wasCommand)
                sit.momentum.unresolvedFromHistory.push_back(t.text);
        if (sit.momentum.unresolvedFromHistory.size() > 3)
            sit.momentum.unresolvedFromHistory.erase(
                sit.momentum.unresolvedFromHistory.begin(),
                sit.momentum.unresolvedFromHistory.end() - 3);
    }
    sit.userState        = estimateUserState(frame, memory, turnIndex);
    sit.likelyMemoryZones = inferMemoryZones(frame);
    sit.likelyToolZones  = inferToolZones(frame);
    return sit;
}

UserStateEstimate SituationBuilder::estimateUserState(
    const PatternFrame& frame, const ConversationMemory&, int turnIndex) const
{
    UserStateEstimate s;
    switch (frame.requestMode) {
        case RequestMode::COMMAND:        s.urgency = 0.80f; break;
        case RequestMode::CLARIFICATION:  s.urgency = 0.65f; break;
        case RequestMode::IMPLEMENTATION: s.urgency = 0.55f; break;
        default:                          s.urgency = 0.25f; break;
    }
    if (frame.requestMode == RequestMode::CLARIFICATION) s.confusion = 0.75f;
    else if (frame.dependsOnHistory && turnIndex > 2)    s.confusion = 0.40f;
    const std::string& lower = frame.normalizedInput;
    auto has = [&](const char* k){ return lower.find(k) != std::string::npos; };
    if (has("wrong") || has("incorrect") || has("that's not") || has("not what i"))
        s.frustration = 0.70f;
    else if (has("again") || has("still") || has("why didn't"))
        s.frustration = 0.45f;
    switch (frame.requestMode) {
        case RequestMode::RESEARCH:       s.depthExpectation = 0.85f; break;
        case RequestMode::DESIGN:         s.depthExpectation = 0.80f; break;
        case RequestMode::IMPLEMENTATION: s.depthExpectation = 0.70f; break;
        case RequestMode::QUESTION:       s.depthExpectation = 0.50f; break;
        default:                          s.depthExpectation = 0.30f; break;
    }
    for (const auto& c : frame.explicitConstraints)
        if (c.find("brief") != std::string::npos)
            s.depthExpectation = std::min(s.depthExpectation, 0.25f);
    return s;
}

std::vector<std::string> SituationBuilder::inferMemoryZones(const PatternFrame& frame) const {
    std::vector<std::string> zones;
    zones.push_back("conversation");
    const std::string& lower = frame.normalizedInput;
    auto has = [&](const char* k){ return lower.find(k) != std::string::npos; };
    if (frame.requestMode == RequestMode::IMPLEMENTATION || frame.requestMode == RequestMode::DESIGN)
        zones.push_back("codebase");
    if (has("yuki") || has("system") || has("subsystem") || has("sensor"))
        zones.push_back("system_knowledge");
    if (frame.needsFreshKnowledge) zones.push_back("external_web");
    else zones.push_back("knowledge_db");
    if (frame.requestMode == RequestMode::RESEARCH) zones.push_back("trace_history");
    return zones;
}

std::vector<std::string> SituationBuilder::inferToolZones(const PatternFrame& frame) const {
    std::vector<std::string> tools;
    const std::string& lower = frame.normalizedInput;
    auto has = [&](const char* k){ return lower.find(k) != std::string::npos; };
    if (frame.requestMode == RequestMode::COMMAND) tools.push_back("subsystem_control");
    if (has("file") || has("folder") || has("directory") || has("disk")) tools.push_back("filesystem");
    if (frame.needsFreshKnowledge) tools.push_back("web_search");
    if (has("screen") || has("window") || has("app")) tools.push_back("screen_capture");
    if (has("camera") || has("face") || has("object")) tools.push_back("camera_vision");
    return tools;
}
