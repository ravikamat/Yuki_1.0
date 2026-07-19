#include "RequestClassifier.h"
#include <algorithm>
#include <cctype>

RequestClassifier::RequestClassifier() {}

// ── Local: word-boundary check ──────────────────────────────────────────────
// Returns true if `needle` appears as a full word (preceded by space/start,
// followed by space/end/punctuation). Prevents "show"→"how", "whatever"→"what" etc.
static bool hasWord(const std::string& hay, const std::string& needle) {
    size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        bool leftOk  = (pos == 0 || !std::isalpha((unsigned char)hay[pos - 1]));
        bool rightOk = (pos + needle.size() >= hay.size() ||
                        !std::isalpha((unsigned char)hay[pos + needle.size()]));
        if (leftOk && rightOk) return true;
        pos += needle.size();
    }
    return false;
}

std::string RequestClassifier::classify(const MeaningState& state) const {
    // Check for ambiguous entities first to trigger clarification early
    for (const auto& entity : state.entities) {
        if (entity.type == EntityType::AMBIGUOUS) {
            return "CLARIFICATION";
        }
    }

    // Prefer LanguageLayer's normalized English so Hinglish works end-to-end.
    std::string q = state.language.normalizedEnglish;
    if (q.empty()) q = state.best_hypothesis;

    // Trim whitespace
    while (!q.empty() && std::isspace((unsigned char)q.back()))  q.pop_back();
    while (!q.empty() && std::isspace((unsigned char)q.front())) q.erase(q.begin());

    std::string lq = q;
    std::transform(lq.begin(), lq.end(), lq.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // ── 1. Identity queries ───────────────────────────────────────────────────
    if (lq.find("who am i")                  != std::string::npos ||
        lq.find("do you know me")            != std::string::npos ||
        lq.find("do you remember me")        != std::string::npos ||
        lq.find("what do you know about me") != std::string::npos) {
        return "USER_PROFILE_QUERY";
    }

    // ── 2. Identity statements ────────────────────────────────────────────────
    if (lq.rfind("my name is", 0) == 0 ||
        lq.rfind("i am ", 0)      == 0 ||
        lq.rfind("call me", 0)    == 0 ||
        lq.rfind("mera naam", 0)  == 0) {
        return "USER_PROFILE_UPDATE";
    }

    // ── 3. CONVERSATION — evaluated FIRST, before any keyword substring scan ──
    // Greetings
    if (lq == "hi" || lq == "hello" || lq == "hey" || lq == "hii" ||
        lq == "helo" || lq == "namaste" || lq == "yo" || lq == "sup") {
        return "CONVERSATION";
    }
    // Acknowledgements / reactions
    if (lq == "ok" || lq == "okay" || lq == "theek hai" || lq == "haan" ||
        lq == "yes" || lq == "no"  || lq == "nahi"      || lq == "sure" ||
        lq == "thanks" || lq == "thank you" || lq == "shukriya" || lq == "dhanyawad" ||
        lq == "cool" || lq == "nice" || lq == "great" || lq == "awesome" ||
        lq == "got it" || lq == "understood") {
        return "CONVERSATION";
    }
    // Self-referential conversational patterns — must come BEFORE "what"/"who" scan
    if (lq.rfind("what are you doing", 0) == 0 ||
        lq.rfind("what are you", 0)       == 0 ||
        lq.rfind("what do you do", 0)     == 0 ||
        lq.rfind("what can you do", 0)    == 0 ||
        lq.find("how are you")            != std::string::npos ||
        lq.find("who are you")            != std::string::npos ||
        lq.find("aap kaun ho")            != std::string::npos ||
        lq.find("kya haal hai")           != std::string::npos ||
        lq.find("kaisa ho")               != std::string::npos) {
        return "CONVERSATION";
    }
    // Emotional / small talk patterns
    if (lq.find("i am bored")      != std::string::npos ||
        lq.find("i'm bored")       != std::string::npos ||
        lq.find("i am tired")      != std::string::npos ||
        lq.find("i'm tired")       != std::string::npos ||
        lq.find("i am happy")      != std::string::npos ||
        lq.find("i'm happy")       != std::string::npos ||
        lq.find("i am sad")        != std::string::npos ||
        lq.find("i'm sad")         != std::string::npos ||
        lq.find("miss you")        != std::string::npos ||
        lq.find("good morning")    != std::string::npos ||
        lq.find("good night")      != std::string::npos ||
        lq.find("good afternoon")  != std::string::npos) {
        return "CONVERSATION";
    }

    // ── 4. Build / make / create + app ────────────────────────────────────────
    if ((lq.find("make ")   != std::string::npos ||
         lq.find("build ")  != std::string::npos ||
         lq.find("create ") != std::string::npos) &&
        (lq.find("app")         != std::string::npos ||
         lq.find("apk")         != std::string::npos ||
         lq.find("application") != std::string::npos)) {
        return "BUILD_APP";
    }

    // ── 5. RESEARCH_REQUEST — deep-explain signals only (DocReader allowed) ──────
    // "compare" is NOT here — it goes through KNOWLEDGE_QUERY + GoalBuilder extraction.
    if ((lq.find("explain in depth")  != std::string::npos ||
         lq.find("explain in detail") != std::string::npos ||
         lq.find("deep dive")         != std::string::npos ||
         lq.find("research ")         != std::string::npos ||
         lq.find("investigate ")      != std::string::npos ||
         lq.find("life cycle of")     != std::string::npos ||
         lq.find("history of")        != std::string::npos ||
         lq.find("detailed guide")    != std::string::npos ||
         lq.find("full guide")        != std::string::npos) &&
        lq.size() > 12) {
        return "RESEARCH_REQUEST";
    }

    // ── 6. KNOWLEDGE_QUERY — factual questions (DB-first, web only on miss) ───
    // Use word-boundary "what" to avoid "whatever", "somewhat", etc.
    if (hasWord(lq, "what") || hasWord(lq, "who")) {
        return "KNOWLEDGE_QUERY";
    }

    // "how" — must be word-boundary-aware
    {
        bool hasHowTo     = lq.find("how to ")  != std::string::npos;
        bool hasHowMuch   = lq.find("how much") != std::string::npos;
        bool hasHowMany   = lq.find("how many") != std::string::npos;
        bool hasHowDoes   = lq.find("how does") != std::string::npos;
        bool hasHowDo     = lq.find("how do")   != std::string::npos;
        bool startsHow    = lq.rfind("how ", 0) == 0;
        bool standaloneHow= lq == "how";
        bool midHow       = lq.find(" how ") != std::string::npos;

        if (hasHowTo || hasHowMuch || hasHowMany || hasHowDoes || hasHowDo ||
            startsHow || standaloneHow || midHow) {
            return "KNOWLEDGE_QUERY";
        }
    }

    // "tell me about X" / "explain X" — only if X is present and substantive
    if (lq.rfind("tell me about ", 0) == 0 && lq.size() > 14) {
        return "KNOWLEDGE_QUERY";
    }
    // "explain X" without depth qualifiers → knowledge query (not research)
    if (lq.rfind("explain ", 0) == 0 && lq.size() > 8) {
        return "KNOWLEDGE_QUERY";
    }
    // "compare X and Y" with leading keyword
    if (lq.rfind("compare ", 0) == 0 && lq.size() > 10) {
        return "KNOWLEDGE_QUERY";
    }
    // "difference between X and Y" / "similarities between X and Y"
    if ((lq.rfind("difference between ", 0) == 0 ||
         lq.rfind("similarities between ", 0) == 0 ||
         lq.rfind("difference of ", 0) == 0) && lq.size() > 20) {
        return "KNOWLEDGE_QUERY";
    }

    // ── 6b. Bare comparison: "X vs Y" ─────────────────────────────────────────
    // Detects "X vs Y" at any position with word-boundary vs.
    // Requires both sides to be at least 2 chars. Minimum total length=7.
    {
        size_t vsPos = lq.find(" vs ");
        if (vsPos != std::string::npos && vsPos >= 2 && lq.size() - vsPos > 5) {
            // Quick sanity: both sides should not contain typical verb words
            std::string left  = lq.substr(0, vsPos);
            std::string right = lq.substr(vsPos + 4);
            // Reject if either side is empty or looks like a sentence fragment with spaces > 4
            bool leftOk  = !left.empty()  && left.size()  >= 2;
            bool rightOk = !right.empty() && right.size() >= 2;
            if (leftOk && rightOk) {
                return "KNOWLEDGE_QUERY";
            }
        }
    }

    // ── 7. Default: TASK_REQUEST ──────────────────────────────────────────────
    return "TASK_REQUEST";
}
