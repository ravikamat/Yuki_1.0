// PatternEngine.cpp — § 3.2 Pattern Layer — multi-signal scoring implementation
#define NOMINMAX
#include "brain/reasoning/PatternEngine.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <map>

// ── Static helpers ────────────────────────────────────────────────────────────

bool PatternEngine::has(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

bool PatternEngine::hasWord(const std::string& h, const std::string& w) {
    auto p = h.find(w);
    if (p == std::string::npos) return false;
    bool lb = (p == 0)            || !std::isalpha((unsigned char)h[p-1]);
    bool rb = (p+w.size() >= h.size()) || !std::isalpha((unsigned char)h[p+w.size()]);
    return lb && rb;
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return r;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PatternEngine::PatternEngine() = default;

// ── Public: buildFrame ────────────────────────────────────────────────────────

PatternFrame PatternEngine::buildFrame(const CanonicalInputEvent& event,
                                       const GoalModel& spec) const {
    PatternFrame frame;
    frame.interactionId   = "turn_" + std::to_string(++interactionCounter_);
    frame.rawInput        = event.rawText;
    frame.normalizedInput = event.normalizedText.empty()
                            ? event.rawText : event.normalizedText;

    const std::string& raw  = frame.rawInput;
    const std::string  lower = toLower(frame.normalizedInput);

    // Step 1 — RequestMode (multi-signal scoring)
    frame.requestMode = scoreRequestMode(raw, lower, lastSignals_);

    // Step 2 — OutputMode
    frame.outputMode = detectOutputMode(lower);

    // Step 3 — Entities
    frame.entities = extractEntities(raw, lower);

    // Step 4 — Constraints
    extractConstraints(raw, lower, frame.requestMode,
                        frame.explicitConstraints,
                        frame.inferredConstraints);

    // Step 5 — History dependence
    frame.dependsOnHistory = detectHistoryDependence(lower);

    // Step 6 — Freshness
    frame.needsFreshKnowledge = detectFreshnessRequired(lower);

    // Step 7 — Unknown slots (from GoalModel Phase 1)
    frame.unknownSlots = spec.unknownSlots;

    // Step 8 — Core intent
    frame.coreIntent = extractCoreIntent(raw, lower, frame.requestMode);

    // Step 9 — Confidence
    frame.confidence = scoreConfidence(lastSignals_, frame.requestMode);

    // Desired outcome
    switch (frame.requestMode) {
        case RequestMode::QUESTION:       frame.desiredOutcome = "factual answer";       break;
        case RequestMode::COMMAND:        frame.desiredOutcome = "system action";        break;
        case RequestMode::IMPLEMENTATION: frame.desiredOutcome = "working code";         break;
        case RequestMode::RESEARCH:       frame.desiredOutcome = "comparative analysis"; break;
        case RequestMode::DESIGN:         frame.desiredOutcome = "architecture plan";    break;
        case RequestMode::CLARIFICATION:  frame.desiredOutcome = "clearer explanation";  break;
        case RequestMode::CONTINUATION:   frame.desiredOutcome = "continuation";         break;
        default:                          frame.desiredOutcome = "conversational reply"; break;
    }

    // ── Step 10: Enrich from GoalModel (Phase 1 Language + Semantic analysis) ──
    // GoalModel comes from LanguageLayer + SemanticParser which run before
    // PatternEngine. These fields are more accurate than text heuristics above.
    if (!spec.domain.empty())        frame.domain          = spec.domain;
    if (!spec.tone.empty())          frame.tone            = spec.tone;
    if (!spec.language.empty())      frame.language        = spec.language;
    if (!spec.responseStyle.empty()) frame.responseStyle   = spec.responseStyle;
    frame.isEmotional         = spec.isEmotional;
    frame.needsResearch      |= spec.needsResearch;
    frame.needsExecution     |= spec.needsExecution;
    frame.needsClarification |= spec.needsClarification;
    // Semantic slots from SemanticParser are more accurate — prefer them
    if (!spec.knownSlots.empty())   frame.knownSlots   = spec.knownSlots;

    if (!spec.goal.empty()) frame.goal = spec.goal;
    if (!spec.gaps.empty()) frame.gaps = spec.gaps;


    return frame;
}

// ── Step 1: RequestMode — multi-signal scoring ────────────────────────────────
// Each pattern adds a weighted vote. The mode with the highest total wins.
// Ties broken by priority order. Signals stored for tracing.

RequestMode PatternEngine::scoreRequestMode(
    const std::string& raw,
    const std::string& lower,
    std::vector<ModeSignal>& signals) const {

    signals.clear();

    // Accumulate votes per mode
    std::map<RequestMode, float> votes;

    // ── COMMAND signals ───────────────────────────────────────────────────────
    struct Trigger { const char* kw; float w; };
    const Trigger cmdTriggers[] = {
        {"turn on",0.9f},{"turn off",0.9f},{"enable",0.8f},{"disable",0.8f},
        {"toggle",0.8f},{"switch on",0.9f},{"switch off",0.9f},
        {"mic on",1.0f},{"mic off",1.0f},{"camera on",1.0f},{"camera off",1.0f},
        {"speaker on",1.0f},{"speaker off",1.0f},{"screen on",1.0f},{"screen off",1.0f},
        {"open ",0.6f},{"close ",0.6f},{"start ",0.5f},{"stop ",0.5f},
        {"restart",0.8f},{"shutdown",0.9f},{"quit",1.0f},
        {"set ",0.5f},{"run ",0.5f},{"launch",0.7f},{"kill ",0.7f},
    };
    for (auto& t : cmdTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::COMMAND] += t.w;
            signals.push_back({RequestMode::COMMAND, t.w, std::string("cmd:")+t.kw});
        }
    }

    // ── IMPLEMENTATION signals ────────────────────────────────────────────────
    const Trigger implTriggers[] = {
        {"implement",0.9f},{"write code",1.0f},{"write a function",1.0f},
        {"write a class",1.0f},{"write a script",1.0f},{"build ",0.6f},
        {"create a ",0.6f},{"add a ",0.5f},{"fix ",0.7f},{"fix the",0.7f},
        {"refactor",0.9f},{"patch ",0.8f},{"debug ",0.8f},{"update the",0.5f},
        {"make it ",0.5f},{"generate code",1.0f},{"code for",0.8f},
    };
    for (auto& t : implTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::IMPLEMENTATION] += t.w;
            signals.push_back({RequestMode::IMPLEMENTATION, t.w, std::string("impl:")+t.kw});
        }
    }

    // ── DESIGN signals ────────────────────────────────────────────────────────
    const Trigger designTriggers[] = {
        {"design ",0.9f},{"architecture",0.9f},{"how should i",0.7f},
        {"how should we",0.7f},{"plan ",0.6f},{"structure it",0.8f},
        {"best way to",0.7f},{"approach to",0.7f},{"system for",0.6f},
        {"schema for",0.8f},{"blueprint",0.9f},
    };
    for (auto& t : designTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::DESIGN] += t.w;
            signals.push_back({RequestMode::DESIGN, t.w, std::string("design:")+t.kw});
        }
    }

    // ── RESEARCH signals ──────────────────────────────────────────────────────
    const Trigger resTriggers[] = {
        {"research",0.9f},{"compare ",0.8f},{"comparison",0.8f},{"analyze",0.8f},
        {"analyse",0.8f},{"what is the best",0.8f},{"which is better",0.8f},
        {"pros and cons",0.9f},{"difference between",0.9f},{"vs ",0.7f},
        {"find out",0.7f},{"investigate",0.8f},{"survey",0.7f},
    };
    for (auto& t : resTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::RESEARCH] += t.w;
            signals.push_back({RequestMode::RESEARCH, t.w, std::string("res:")+t.kw});
        }
    }

    // ── CLARIFICATION signals ─────────────────────────────────────────────────
    const Trigger clarTriggers[] = {
        {"what do you mean",1.0f},{"clarify",0.9f},{"explain again",0.9f},
        {"i don't understand",0.9f},{"i dont understand",0.9f},
        {"what exactly",0.8f},{"be more specific",0.8f},{"elaborate",0.7f},
        {"confusing",0.7f},{"unclear",0.7f},
    };
    for (auto& t : clarTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::CLARIFICATION] += t.w;
            signals.push_back({RequestMode::CLARIFICATION, t.w, std::string("clar:")+t.kw});
        }
    }

    // ── CONTINUATION signals ──────────────────────────────────────────────────
    const Trigger contTriggers[] = {
        {"continue",0.9f},{"keep going",1.0f},{"go on",0.8f},{"what next",0.8f},
        {"and then",0.6f},{"more on",0.7f},{"tell me more",0.8f},
        {"next step",0.7f},{"expand on",0.7f},
    };
    for (auto& t : contTriggers) {
        if (has(lower, t.kw)) {
            votes[RequestMode::CONTINUATION] += t.w;
            signals.push_back({RequestMode::CONTINUATION, t.w, std::string("cont:")+t.kw});
        }
    }
    // Short utterances also vote for continuation
    if (lower.size() < 15) {
        votes[RequestMode::CONTINUATION] += 0.4f;
        signals.push_back({RequestMode::CONTINUATION, 0.4f, "short_utterance"});
    }

    // ── QUESTION signals (broad — lower weight, many triggers) ───────────────
    const char* qWords[] = {"what ","why ","how ","when ","where ","who ",
                             "which ","whose ","whom ","is it ","are there "};
    for (auto qw : qWords) {
        if (has(lower, qw)) {
            votes[RequestMode::QUESTION] += 0.6f;
            signals.push_back({RequestMode::QUESTION, 0.6f, std::string("q:")+qw});
            break; // one vote per question-word class
        }
    }
    if (has(lower, "?")) {
        votes[RequestMode::QUESTION] += 0.5f;
        signals.push_back({RequestMode::QUESTION, 0.5f, "question_mark"});
    }
    if (has(lower, "tell me about") || has(lower, "explain ") ||
        has(lower, "describe ")) {
        votes[RequestMode::QUESTION] += 0.6f;
        signals.push_back({RequestMode::QUESTION, 0.6f, "explain_verb"});
    }

    // ── Election: highest total wins ──────────────────────────────────────────
    if (votes.empty()) return RequestMode::UNKNOWN;

    RequestMode winner = RequestMode::UNKNOWN;
    float       best   = 0.0f;
    // Priority order for ties: COMMAND > IMPLEMENTATION > DESIGN > RESEARCH >
    //                          CLARIFICATION > QUESTION > CONTINUATION
    const RequestMode priority[] = {
        RequestMode::COMMAND, RequestMode::IMPLEMENTATION, RequestMode::DESIGN,
        RequestMode::RESEARCH, RequestMode::CLARIFICATION, RequestMode::QUESTION,
        RequestMode::CONTINUATION
    };
    for (auto m : priority) {
        auto it = votes.find(m);
        if (it != votes.end() && it->second > best) {
            best   = it->second;
            winner = m;
        }
    }
    return winner;
}

// ── Step 2: OutputMode ────────────────────────────────────────────────────────

OutputMode PatternEngine::detectOutputMode(const std::string& lower) const {
    // Scored, not first-match
    struct OMSignal { OutputMode m; const char* kw; };
    const OMSignal oms[] = {
        {OutputMode::CODE,         "write code"},
        {OutputMode::CODE,         "function for"},
        {OutputMode::CODE,         "class for"},
        {OutputMode::CODE,         "script that"},
        {OutputMode::CODE,         "implement"},
        {OutputMode::CODE,         "snippet"},
        {OutputMode::PATCH,        "patch "},
        {OutputMode::PATCH,        "diff "},
        {OutputMode::PATCH,        "fix the bug"},
        {OutputMode::BULLETS,      "list all"},
        {OutputMode::BULLETS,      "bullet"},
        {OutputMode::BULLETS,      "step by step"},
        {OutputMode::BULLETS,      "steps to"},
        {OutputMode::BULLETS,      "enumerate"},
        {OutputMode::ARCHITECTURE, "architecture"},
        {OutputMode::ARCHITECTURE, "system design"},
        {OutputMode::ARCHITECTURE, "diagram"},
        {OutputMode::REPORT,       "write a report"},
        {OutputMode::REPORT,       "detailed analysis"},
        {OutputMode::REPORT,       "full explanation"},
        {OutputMode::REPORT,       "summary of"},
        {OutputMode::MIXED,        "explain and show"},
        {OutputMode::MIXED,        "describe and implement"},
    };
    std::map<OutputMode, int> scores;
    for (auto& o : oms)
        if (has(lower, o.kw)) scores[o.m]++;

    if (scores.empty()) return OutputMode::TEXT;
    return std::max_element(scores.begin(), scores.end(),
        [](auto& a, auto& b){ return a.second < b.second; })->first;
}

// ── Step 3: Entity extraction ─────────────────────────────────────────────────
// Extracts: (a) quoted phrases, (b) capitalized multi-word sequences,
//           (c) known technical/domain keywords, (d) numeric quantities

std::vector<std::string> PatternEngine::extractEntities(
    const std::string& raw, const std::string& lower) const {

    std::vector<std::string> entities;
    auto push = [&](const std::string& e) {
        if (std::find(entities.begin(), entities.end(), e) == entities.end())
            entities.push_back(e);
    };

    // (a) Quoted strings — "like this" or 'like this'
    for (char q : {'"', '\''}) {
        size_t s = 0;
        while ((s = raw.find(q, s)) != std::string::npos) {
            auto e = raw.find(q, s+1);
            if (e != std::string::npos && e-s > 1 && e-s < 50) {
                push(raw.substr(s+1, e-s-1));
                s = e+1;
            } else break;
        }
    }

    // (b) Capitalized word sequences (proper nouns / named entities)
    // Walk the original raw string, collect runs of Title-Case words
    std::istringstream ss(raw);
    std::string word, phrase;
    while (ss >> word) {
        // Strip punctuation from edges
        while (!word.empty() && !std::isalpha((unsigned char)word.front()))
            word.erase(word.begin());
        while (!word.empty() && !std::isalpha((unsigned char)word.back()))
            word.pop_back();
        if (word.empty()) { if (!phrase.empty()) { push(phrase); phrase.clear(); } continue; }
        if (std::isupper((unsigned char)word[0]) && word.size() > 1) {
            phrase += (phrase.empty() ? "" : " ") + word;
        } else {
            if (!phrase.empty()) { push(phrase); phrase.clear(); }
        }
    }
    if (!phrase.empty()) push(phrase);

    // (c) Known technical and domain keywords (lowercase match)
    const char* known[] = {
        // Yuki subsystems
        "mic","microphone","camera","speaker","screen","stt","tts",
        "voice","brain","memory","sensor","shell","model","agent",
        // Languages & tools
        "python","c++","cpp","javascript","typescript","rust","java","kotlin",
        "react","vue","angular","node","docker","kubernetes","git","linux",
        "windows","macos","sql","sqlite","postgresql","redis","tensorflow",
        // AI/ML
        "llm","whisper","faster-whisper","neural","neural network",
        "machine learning","deep learning","transformer","embedding","vector",
        // General tech
        "api","http","json","xml","yaml","regex","thread","async","pipe",
        "socket","port","ip","url","database","cache","index","query",
    };
    for (auto kw : known)
        if (hasWord(lower, kw)) push(kw);

    // (d) Numeric quantities with units
    // e.g. "100ms", "4GB", "8 seconds", "3 threads"
    const char* units[] = {"ms","gb","mb","kb","hz","mhz","ghz","px","fps",
                            "seconds","minutes","hours","threads","items"};
    for (auto unit : units) {
        auto p = lower.find(unit);
        while (p != std::string::npos) {
            // Scan backwards for the number
            size_t s = p;
            while (s > 0 && (std::isdigit((unsigned char)lower[s-1]) || lower[s-1]==' '))
                --s;
            if (s < p) {
                std::string qty = raw.substr(s, p+strlen(unit)-s);
                // trim leading spaces
                while (!qty.empty() && qty[0]==' ') qty.erase(qty.begin());
                push(qty);
            }
            p = lower.find(unit, p+1);
        }
    }

    return entities;
}

// ── Step 4: Constraint extraction ─────────────────────────────────────────────

void PatternEngine::extractConstraints(
    const std::string& raw,
    const std::string& lower,
    RequestMode mode,
    std::vector<std::string>& explicit_,
    std::vector<std::string>& implicit_) const {

    // Explicit constraints — directly stated in the input
    struct CK { const char* kw; const char* constraint; };
    const CK expCK[] = {
        // Technology choice
        {"in python",       "language:python"},
        {"in c++",          "language:c++"},
        {"in javascript",   "language:javascript"},
        {"in rust",         "language:rust"},
        {"using python",    "language:python"},
        {"using c++",       "language:c++"},
        // Format
        {"step by step",    "format:step_by_step"},
        {"briefly",         "length:brief"},
        {"in detail",       "length:detailed"},
        {"short answer",    "length:brief"},
        {"detailed",        "length:detailed"},
        {"in one sentence", "length:one_sentence"},
        // Negation
        {"don't use",       "exclude:specified"},
        {"without using",   "exclude:specified"},
        {"no external",     "exclude:external_deps"},
        {"no dependencies", "exclude:all_deps"},
        // Safety
        {"safe",            "quality:safe"},
        {"production",      "quality:production_ready"},
        {"tested",          "quality:tested"},
        // Scope
        {"only",            "scope:restricted"},
        {"just",            "scope:restricted"},
    };
    for (auto& c : expCK)
        if (has(lower, c.kw)) explicit_.push_back(c.constraint);

    // Implicit constraints — inferred from mode + entities
    switch (mode) {
        case RequestMode::IMPLEMENTATION:
            implicit_.push_back("quality:compilable");
            implicit_.push_back("quality:complete");
            break;
        case RequestMode::COMMAND:
            implicit_.push_back("scope:current_session");
            implicit_.push_back("timing:immediate");
            break;
        case RequestMode::QUESTION:
            implicit_.push_back("quality:factually_accurate");
            break;
        case RequestMode::RESEARCH:
            implicit_.push_back("quality:sourced");
            implicit_.push_back("format:comparative");
            break;
        case RequestMode::DESIGN:
            implicit_.push_back("quality:maintainable");
            implicit_.push_back("quality:scalable");
            break;
        default:
            break;
    }

    // Infer from known entities
    if (has(lower, "production") || has(lower, "prod "))
        implicit_.push_back("quality:production_safe");
    if (has(lower, "real-time") || has(lower, "realtime") || has(lower, "live"))
        implicit_.push_back("timing:real_time");
    if (has(lower, "beginner") || has(lower, "simple") || has(lower, "basic"))
        implicit_.push_back("complexity:low");
    if (has(lower, "advanced") || has(lower, "expert") || has(lower, "deep"))
        implicit_.push_back("complexity:high");
}

// ── Step 5: History dependence ────────────────────────────────────────────────

bool PatternEngine::detectHistoryDependence(const std::string& lower) const {
    // Explicit references to prior conversation
    const char* ref[] = {
        "earlier","before","last time","previously","you said","you mentioned",
        "we were","as i mentioned","as we discussed","remember when",
        "you told me","i asked","like i said","from before","what we talked"
    };
    for (auto r : ref) if (has(lower, r)) return true;

    // Pronoun references that imply a prior noun (anaphora)
    // Only count if no explicit entity named — context-dependent
    const char* pronouns[] = {" it "," it's "," that "," this "," those ",
                               " they "," them "," the same "," the one "};
    for (auto p : pronouns)
        if (has(lower, p)) return true;

    return false;
}

// ── Step 6: Freshness ─────────────────────────────────────────────────────────

bool PatternEngine::detectFreshnessRequired(const std::string& lower) const {
    const char* fresh[] = {
        "today","right now","currently","at the moment","this week",
        "this year","latest","most recent","just released","new version",
        "update","breaking news","live","real-time","current status"
    };
    for (auto f : fresh) if (has(lower, f)) return true;
    // Year references (2020-2030)
    for (int y = 2020; y <= 2030; ++y)
        if (has(lower, std::to_string(y))) return true;
    return false;
}


// ── Step 8: Core intent ───────────────────────────────────────────────────────

std::string PatternEngine::extractCoreIntent(
    const std::string& raw,
    const std::string& lower,
    RequestMode mode) const {

    // Strip leading question words to get to the subject
    std::string intent = stripLeadingQuestionWords(lower);

    // Take up to first clause boundary
    auto end = intent.find_first_of(".,;?!");
    if (end != std::string::npos) intent = intent.substr(0, end);
    intent.erase(0, intent.find_first_not_of(" \t"));
    if (intent.size() > 80) intent = intent.substr(0, 80);

    // Prefix with mode verb for clarity
    switch (mode) {
        case RequestMode::COMMAND:        return "execute: " + intent;
        case RequestMode::IMPLEMENTATION: return "build: "   + intent;
        case RequestMode::DESIGN:         return "design: "  + intent;
        case RequestMode::RESEARCH:       return "research: " + intent;
        case RequestMode::CLARIFICATION:  return "clarify: " + intent;
        default:                          return intent;
    }
}

std::string PatternEngine::stripLeadingQuestionWords(const std::string& s) {
    const char* qw[] = {"what is ","what are ","what was ","what were ",
                         "who is ","who are ","who was ","where is ","where are ",
                         "when did ","when is ","why is ","why are ","why does ",
                         "how does ","how do ","how is ","how can ",
                         "tell me about ","explain ","describe ","define "};
    std::string r = s;
    for (auto q : qw) {
        if (r.rfind(q, 0) == 0) {
            r = r.substr(strlen(q));
            break;
        }
    }
    return r;
}

// ── Step 9: Confidence ────────────────────────────────────────────────────────

float PatternEngine::scoreConfidence(
    const std::vector<ModeSignal>& signals,
    RequestMode winner) const {

    if (signals.empty()) return 0.30f;

    float total = 0.0f, winnerTotal = 0.0f;
    for (auto& s : signals) {
        total += s.weight;
        if (s.mode == winner) winnerTotal += s.weight;
    }

    // Ratio of winner votes to all votes, scaled to [0.40, 0.95]
    float ratio = (total > 0) ? winnerTotal / total : 0.0f;
    return 0.40f + ratio * 0.55f;
}
