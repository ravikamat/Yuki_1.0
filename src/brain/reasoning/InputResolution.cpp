// InputResolution.cpp — Clarification engine + unknown topic flow (merged)
#define NOMINMAX
#include "brain/reasoning/InputResolution.h"
#include "brain/retrieval/RetrievalSystem.h"   // WebReconAgent definition
#include "brain/reasoning/TaskSystem.h"
#include "brain/reasoning/GoalModel.h"
#include "brain/memory/UserMemory.h"        // required for new generateBlockingQuestion overload
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

// ══════════════════════════════════════════════════════════════════════════════
// ClarificationEngine
// ══════════════════════════════════════════════════════════════════════════════

void ClarificationEngine::setDependencies(VectorStore* vectorStore, EmbeddingEngine* embeddingEngine) {
    vectorStore_ = vectorStore;
    embeddingEngine_ = embeddingEngine;
}

std::string ClarificationEngine::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool ClarificationEngine::has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

void ClarificationEngine::recordAsked(const std::string& topic) {
    if (!alreadyAsked(topic)) askedTopics_.push_back(toLower(topic));
}
bool ClarificationEngine::alreadyAsked(const std::string& topic) const {
    const std::string L = toLower(topic);
    for (const auto& t : askedTopics_)
        if (t==L || L.find(t)!=std::string::npos || t.find(L)!=std::string::npos) return true;
    return false;
}
void ClarificationEngine::clearSession() { askedTopics_.clear(); }

ClarificationNeeded ClarificationEngine::evaluate(const VerificationReport& report,
                                                    const StreamParseResult& stream,
                                                    const PatternFrame& frame) const {
    ClarificationNeeded result;
    std::vector<MiniIntent> taskIntents, unclearIntents;
    for (const auto& i : stream.intents) {
        if (i.type==IntentType::TASK)    taskIntents.push_back(i);
        if (i.type==IntentType::UNCLEAR) unclearIntents.push_back(i);
    }
    if (taskIntents.size() >= 2) {
        const std::string topic = "task_priority:" + taskIntents[0].subject + "_" + taskIntents[1].subject;
        if (!alreadyAsked(topic)) {
            result.needed=true; result.missingPiece="task priority"; result.urgency=0.60f;
            std::ostringstream q;
            q << "I can see a few things you want to work on — "
              << "**" << taskIntents[0].subject << "** and **" << taskIntents[1].subject << "**. "
              << "Should I tackle one first, or build a single plan that covers both?";
            result.question = q.str();
            const_cast<ClarificationEngine*>(this)->recordAsked(topic); return result;
        }
    }
    if (report.satisfactionScore < 0.40f && !frame.entities.empty()) {
        const std::string& entity = frame.entities[0];
        if (!alreadyAsked(entity)) {
            result.needed=true; result.missingPiece=entity; result.urgency=0.75f;
            result.question = generateQuestion(entity, frame, unclearIntents);
            const_cast<ClarificationEngine*>(this)->recordAsked(entity); return result;
        }
    }
    if (!unclearIntents.empty() && !unclearIntents[0].subject.empty()) {
        const std::string& sub = unclearIntents[0].subject;
        if (!alreadyAsked(sub)) {
            result.needed=true; result.missingPiece=sub; result.urgency=0.50f;
            result.question = "I caught that you want to do something with **" + sub +
                              "** — could you tell me a bit more about what the end result should look like?";
            const_cast<ClarificationEngine*>(this)->recordAsked(sub); return result;
        }
    }
    if (!report.missingNeeds.empty() && report.satisfactionScore < 0.50f) {
        const std::string& missing = report.missingNeeds[0];
        if (!alreadyAsked(missing)) {
            result.needed=true; result.missingPiece=missing; result.urgency=0.65f;
            result.question = "To give you a complete answer I need one more thing: **" + missing + "**. Can you tell me?";
            const_cast<ClarificationEngine*>(this)->recordAsked(missing); return result;
        }
    }
    return result;
}

std::string ClarificationEngine::generateQuestion(const std::string& missingPiece,
                                                    const PatternFrame&,
                                                    const std::vector<MiniIntent>&) const {
    const std::string L = toLower(missingPiece);
    
    // Fallback static rules if semantic dependencies are missing
    if (!vectorStore_ || !embeddingEngine_) {
        if (has(L,"trading")||has(L,"stock")||has(L,"invest")||has(L,"market"))
            return "When you mention **"+missingPiece+"** — are you thinking about intraday trading, long-term investing, or a fully automated algo-trading bot?";
        if (has(L,"bot")||has(L,"automation")||has(L,"automate"))
            return "For the **"+missingPiece+"** — should it run on a schedule automatically, or only when you trigger it manually?";
        if (has(L,"api")||has(L,"integration")||has(L,"connect"))
            return "Which **"+missingPiece+"** specifically? If you have a preferred platform or provider in mind, that'll help me plan the right approach.";
        return "I want to make sure I get this right — when it comes to **"+missingPiece+"**, what's the most important outcome you're expecting from it?";
    }
    
    // Enhanced Semantic Category Matching
    // We dynamically embed the missing piece and search the vector store for known clarification profiles
    std::vector<float> vec = embeddingEngine_->embed(missingPiece);
    if (!vec.empty()) {
        auto hits = vectorStore_->search(vec, 1);
        if (!hits.empty() && hits[0].distance < 0.40f) { // High semantic similarity
            // Based on semantic proximity to known domains, we generate contextual questions
            std::string meta = hits[0].metadata;
            if (has(toLower(meta), "finance") || has(toLower(meta), "market")) {
                return "Since **" + missingPiece + "** relates to markets, are you aiming for short-term automated trades or long-term tracking?";
            }
            if (has(toLower(meta), "software") || has(toLower(meta), "code")) {
                return "When building **" + missingPiece + "**, do you have a specific tech stack in mind, or should I choose the best one?";
            }
            if (has(toLower(meta), "infrastructure") || has(toLower(meta), "database")) {
                return "For **" + missingPiece + "**, do we need real-time syncing or is local file storage sufficient?";
            }
        }
    }
    
    return "I want to make sure I get this right — when it comes to **"+missingPiece+"**, what's the most important outcome you're expecting from it?";
}

std::string ClarificationEngine::generateBlockingQuestion(const GoalModel& model) const {
    if (model.unknownSlots.empty()) return "Could you clarify your request?";
    
    const std::string& missing = model.unknownSlots[0];
    
    if (model.goal == "build app" || model.goal == "BUILD_APP") {
        std::string domain = model.domain.empty() ? "app" : model.domain + " app";
        
        if (missing == "features" || missing == "core features") {
            if (model.domain == "fitness" || model.domain == "workout") {
                return "What kind of " + domain + " do you want me to build \xE2\x80\x94 workout tracker, diet planner, or both?";
            }
            return "What are the core features you want in this " + domain + "?";
        }
        if (missing == "platform") {
            return "Should this " + domain + " be for Android, iOS, or Web?";
        }
        if (missing == "login requirement" || missing == "account") {
            return "Do you need user accounts and login functionality for this " + domain + "?";
        }
        if (missing == "offline mode" || missing == "offline/online") {
            return "Does this " + domain + " need to work entirely offline?";
        }
    }
    
    return "To proceed with " + model.goal + ", I need to know about: " + missing + ". Can you provide that?";
}

// ── GoalModel-driven, memory-aware, confidence-aware implementation ────────────
// Logic:
//   1. Build slot priority order (blocking-first).
//   2. For each slot in priority order:
//      a. Skip if already in knownSlots.
//      b. Skip if already inferred (ClarificationState).
//      c. Try to infer from UserMemory (getUserFact / getPersonByRelation).
//      d. Try to infer from context heuristics (obvious defaults).
//      e. If ask-count exhausted (>= MAX_ASK_PER_SLOT): set needsResearch flag, skip.
//      f. If first ask: return primary question.
//      g. If second ask (retry): return narrower re-phrase.
//   3. If no slot needs asking: return "" (nothing to clarify).

std::string ClarificationEngine::generateBlockingQuestion(
        const GoalModel&      model,
        const UserMemory&     memory,
        ClarificationState&   state,
        bool&                 needsResearchOut) const
{
    needsResearchOut = false;

    if (model.unknownSlots.empty()) return "";

    // ── Slot priority table ──────────────────────────────────────────────────
    // Lower number = ask first (more blocking).
    // Any slot not in this table gets priority 99 (lowest).
    static const std::map<std::string, int> SLOT_PRIORITY = {
        {"target_person",   0},   // who — blocks everything
        {"platform",        1},   // which app / service
        {"message_content", 2},   // what to say / send
        {"device",          3},   // which hardware
        {"features",        4},   // core features of a build
        {"core features",   4},
        {"purpose",         5},   // what it's for
        {"language",        6},   // programming language
        {"format",          7},   // output format
        {"time",            8},   // when / schedule
        {"login requirement",9},
        {"offline mode",   10},
        {"offline/online", 10},
    };

    // Sort unknown slots by priority
    std::vector<std::string> sorted = model.unknownSlots;
    std::sort(sorted.begin(), sorted.end(),
        [&](const std::string& a, const std::string& b) {
            int pa = 99, pb = 99;
            auto ia = SLOT_PRIORITY.find(a); if (ia != SLOT_PRIORITY.end()) pa = ia->second;
            auto ib = SLOT_PRIORITY.find(b); if (ib != SLOT_PRIORITY.end()) pb = ib->second;
            return pa < pb;
        });

    // Helper: build domain label
    std::string domainLabel = model.domain.empty() ? model.goal : model.domain;

    for (const std::string& slot : sorted) {

        // 1. Already in knownSlots? skip.
        if (model.knownSlots.count(slot) && !model.knownSlots.at(slot).empty())
            continue;

        // 2. Already inferred this session? skip.
        if (state.isInferred(slot))
            continue;

        // 3. Try UserMemory lookup (non-blocking — never crashes if memory empty)
        {
            std::string memVal;
            if (slot == "target_person") {
                // try relationship lookups — friend, colleague, boss, wife, etc.
                for (const std::string& rel : {"friend","colleague","boss","wife","husband","partner","brother","sister","mom","dad"}) {
                    memVal = memory.getPersonByRelation(rel);
                    if (!memVal.empty()) break;
                }
                if (memVal.empty()) memVal = memory.getUserFact("contact");
            } else if (slot == "device") {
                memVal = memory.getUserFact("device");
                if (memVal.empty()) memVal = memory.getUserFact("phone");
            } else if (slot == "language") {
                memVal = memory.getUserFact("preferred_language");
            } else {
                memVal = memory.getUserFact(slot);
            }

            if (!memVal.empty()) {
                // Inferred from memory — record and skip asking
                state.markInferred(slot, memVal);
                continue;
            }
        }

        // 4. Context heuristics — infer obvious defaults when domain is clear
        {
            std::string inferred;
            const std::string lowerGoal = toLower(model.goal);
            const std::string lowerDomain = toLower(model.domain);

            if (slot == "platform") {
                if (has(lowerGoal,"whatsapp") || has(lowerDomain,"whatsapp")) inferred = "WhatsApp";
                else if (has(lowerGoal,"email") || has(lowerDomain,"email"))  inferred = "Email";
                else if (has(lowerGoal,"sms")   || has(lowerDomain,"sms"))    inferred = "SMS";
            }
            if (slot == "device") {
                if (has(lowerGoal,"phone") || has(lowerDomain,"mobile"))  inferred = "mobile_phone";
                if (has(lowerGoal,"computer") || has(lowerDomain,"pc"))   inferred = "computer";
            }
            if (slot == "language") {
                if (has(lowerDomain,"android") || has(lowerGoal,"android")) inferred = "Kotlin";
                else if (has(lowerDomain,"ios") || has(lowerGoal,"ios"))    inferred = "Swift";
                else if (has(lowerDomain,"web") || has(lowerGoal,"web"))    inferred = "JavaScript";
            }

            if (!inferred.empty()) {
                state.markInferred(slot, inferred);
                continue;
            }
        }

        // 5. Exhausted retries? Escalate to research.
        if (state.exhausted(slot)) {
            needsResearchOut = true;
            // Do NOT return a question — continue to find a non-exhausted slot
            continue;
        }

        // 6. Build question — narrow on retry
        state.recordAsked(slot);
        const bool isRetry = (state.askCount.at(slot) >= 2);

        // ── Slot-specific primary questions ─────────────────────────────────
        if (!isRetry) {
            if (slot == "target_person")
                return std::string("Who should I ") + (has(toLower(model.goal),"send")?"send this to":"contact") + "?";
            if (slot == "platform")
                return "Which platform should I use — WhatsApp, Email, SMS, or another?";
            if (slot == "message_content")
                return "What would you like the message to say?";
            if (slot == "device")
                return "Which device should I use for this — your phone or your computer?";
            if (slot == "features" || slot == "core features") {
                if (!domainLabel.empty())
                    return "What are the core features you want in this " + domainLabel + " app?";
                return "What features should this have?";
            }
            if (slot == "purpose")
                return "What is this " + domainLabel + " meant to do?";
            if (slot == "language")
                return "Which programming language or framework do you prefer for this?";
            if (slot == "format")
                return "What format do you want the output in?";
            if (slot == "time")
                return "When should this happen — right now, or at a specific time?";
            if (slot == "login requirement" || slot == "account")
                return "Does this need user accounts and login?";
            if (slot == "offline mode" || slot == "offline/online")
                return "Does this need to work offline?";
            // Generic fallback primary
            return "To continue, I need one piece of information: what is the " + slot + "?";
        }

        // ── Retry — narrower re-phrase ───────────────────────────────────────
        if (slot == "target_person")
            return "Just the name is fine — who should receive this?";
        if (slot == "platform")
            return "A quick clarification: should I use WhatsApp, Email, or something else?";
        if (slot == "message_content")
            return "What exact text should the message contain?";
        if (slot == "device")
            return "Your phone, or your computer — which one?";
        if (slot == "features" || slot == "core features")
            return "Name the single most important feature for the " + domainLabel + " app.";
        // Generic retry
        return "I still need the " + slot + " to proceed — can you be more specific?";
    }

    // All slots were either known, inferred, or exhausted
    return "";
}


// ══════════════════════════════════════════════════════════════════════════════
// UnknownTopicFlow
// ══════════════════════════════════════════════════════════════════════════════

std::string UnknownTopicFlow::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool UnknownTopicFlow::has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

std::string UnknownTopicFlow::extractUnknownTerm(const std::string& lower, const PatternFrame& frame) const {
    if (!frame.entities.empty()) return frame.entities[0];
    for (const char* pfx : {"what is ","what are ","what does ","who is ","how does ","explain ","tell me about ","meaning of ","definition of "}) {
        auto p = lower.find(pfx);
        if (p != std::string::npos) {
            std::string rest = lower.substr(p + strlen(pfx));
            while (!rest.empty() && (rest.back()=='?'||rest.back()=='.'||rest.back()==' ')) rest.pop_back();
            if (rest.size()>=3 && rest.size()<60) return rest;
        }
    }
    if (!frame.coreIntent.empty()) {
        std::string ci = frame.coreIntent;
        auto colon = ci.find(": "); if (colon!=std::string::npos) ci=ci.substr(colon+2);
        if (ci.size()>=3) return ci;
    }
    return lower.substr(0, std::min((size_t)50, lower.size()));
}

bool UnknownTopicFlow::tryVault(const std::string& term, ConceptVault& vault, UnknownTopicResult& out) const {
    LearnedConcept concept;
    if (!vault.recall(term, concept) || concept.definition.empty()) return false;
    out.handled=true; out.source=UnknownResolutionSource::VAULT;
    out.confidence=concept.confidence; out.learnedTerm=concept.term;
    out.response = "**"+concept.term+"**: "+concept.definition;
    if (!concept.domain.empty()) out.response += "\n*(Learned from: "+concept.domain+")*";
    std::cout << "[InputResolution] Stage 1 — Vault hit: " << concept.term << "\n";
    return true;
}

bool UnknownTopicFlow::tryDaemon(const std::string& term, KnowledgeDaemon* kd, UnknownTopicResult& out) const {
    if (!kd || !kd->isRunning()) return false;
    KnowledgeAnswer ans = kd->query(term, 800);
    if (!ans.found || ans.text.empty() || ans.confidence < 0.65f) return false;
    out.handled=true; out.source=UnknownResolutionSource::DAEMON;
    out.confidence=ans.confidence; out.learnedTerm=ans.topic.empty()?term:ans.topic;
    out.response = "**"+out.learnedTerm+"**: "+ans.text;
    std::cout << "[InputResolution] Stage 2 — KD hit: " << out.learnedTerm << " (conf=" << ans.confidence << ")\n";
    return true;
}

bool UnknownTopicFlow::tryWeb(const std::string&, const std::string& term,
                               WebReconAgent& webRecon, ConceptVault& vault,
                               UnknownTopicResult& out) const {
    if (!webRecon.isAvailable()) return false;
    auto snippets = webRecon.search(term, 2, 3000);
    if (snippets.empty()) return false;
    const WebSnippet* best = &snippets[0];
    for (const auto& s : snippets) if (s.relevance>best->relevance) best=&s;
    if (best->snippet.empty() || best->snippet.size()<20) return false;
    std::string text = best->snippet;
    if (text.size()>300) text=text.substr(0,300)+"...";
    out.handled=true; out.source=UnknownResolutionSource::WEB;
    out.confidence=0.62f; out.learnedTerm=term;
    out.response = "**"+term+"** (from the web): "+text;
    vault.indexFromKnowledge(term, text, 0.62f);
    std::cout << "[InputResolution] Stage 3 — Web hit for: " << term << "\n";
    return true;
}

std::string UnknownTopicFlow::buildLearningResponse(const std::string& term, bool questionAsked) const {
    if (questionAsked) return "";
    // Vary by term hash so each topic gets a different phrasing
    int variant = 0;
    for (char c : term) variant = (variant * 31 + (unsigned char)c) % 4;

    std::ostringstream ss;
    switch (variant) {
    case 0:
        ss << "I don't have data on **" << term << "** yet \u2014 queuing it for urgent learning right now.\n\n"
           << "Ask me again in a few moments and I should have something useful.";
        break;
    case 1:
        ss << "**" << term << "** is new to me. I've started learning it in the background.\n\n"
           << "Try: *'what is " << term << "'* in a moment when I've had time to study.";
        break;
    case 2:
        ss << "My knowledge of **" << term << "** is thin right now. I've flagged it for immediate research.\n\n"
           << "Give me a moment and ask again \u2014 I'll have more to say.";
        break;
    default:
        ss << "I'm learning about **" << term << "** as we speak. Check back shortly!\n\n"
           << "Tip: *'tell me about " << term << "'* will trigger a fresh lookup.";
        break;
    }
    return ss.str();
}

UnknownTopicResult UnknownTopicFlow::handle(const std::string& rawInput,
                                             const PatternFrame& frame,
                                             const std::string& pipelineAnswer,
                                             KnowledgeDaemon* knowledge,
                                             ConceptVault& vault,
                                             WebReconAgent& webRecon,
                                             ClarificationEngine& clarif,
                                             TaskDecomposer&) const {
    UnknownTopicResult result;
    const std::string lower = toLower(rawInput);
    const std::string term  = extractUnknownTerm(lower, frame);
    if (term.empty()) return result;
    result.learnedTerm = term;

    if (tryVault(term, vault, result)) return result;
    if (knowledge && tryDaemon(term, knowledge, result)) {
        vault.indexFromKnowledge(term, result.response, result.confidence);
        return result;
    }
    if (tryWeb(rawInput, term, webRecon, vault, result)) return result;

    {
        StreamParseResult mockStream;
        MiniIntent unclear; unclear.type=IntentType::UNCLEAR; unclear.content=rawInput;
        unclear.confidence=0.30f; unclear.subject=term; mockStream.intents.push_back(unclear);
        mockStream.isMultiIntent=false; mockStream.clarity=0.30f;
        VerificationReport mockReport;
        mockReport.satisfactionScore=0.20f; mockReport.satisfied=false;
        mockReport.missingNeeds.push_back(term);
        auto clarification = clarif.evaluate(mockReport, mockStream, frame);
        if (clarification.needed && !clarification.question.empty()) {
            result.handled=true; result.source=UnknownResolutionSource::ASKED_USER;
            result.confidence=0.25f; result.questionAsked=true; result.response=clarification.question;
            if (knowledge) knowledge->learnTopic(term, KnowledgeDaemon::LearnPriority::P0_URGENT);
            std::cout << "[InputResolution] Stage 4 — Asked user about: " << term << "\n";
            return result;
        }
    }

    if (knowledge) knowledge->learnTopic(term, KnowledgeDaemon::LearnPriority::P0_URGENT);
    if (!pipelineAnswer.empty() && pipelineAnswer.find("still learning")==std::string::npos) {
        result.handled=true; result.source=UnknownResolutionSource::QUEUED; result.confidence=0.30f;
        result.response = pipelineAnswer + "\n\n*(I've queued **"+term+"** for urgent learning — I'll know more very soon.)*";
    } else {
        result.handled=true; result.source=UnknownResolutionSource::QUEUED; result.confidence=0.20f;
        result.response = buildLearningResponse(term, false);
    }
    std::cout << "[InputResolution] Stage 5 — Queued learning for: " << term << "\n";
    return result;
}
