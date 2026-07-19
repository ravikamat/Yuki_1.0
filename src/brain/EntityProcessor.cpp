#include "EntityProcessor.h"
#include "brain/memory/UserMemory.h"
#include "database/DatabaseManager.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

EntitySpanDetector::EntitySpanDetector() {}

std::vector<std::string> EntitySpanDetector::detectSpans(const std::string& query) {
    std::vector<std::string> spans;
    
    // Lowercase version for search
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                   
    // Remove trailing question mark/period
    while (!lower.empty() && (lower.back() == '?' || lower.back() == '.' || lower.back() == '!' || lower.back() == ' '))
        lower.pop_back();
        
    // Strip typical question/command prefixes
    static const char* const PREFIXES[] = {
        "what is ", "what are ", "what does ", "what do ", "who is ", "who are ",
        "tell me about ", "explain in depth ", "explain in detail ", "explain ",
        "deep dive into ", "research ", "investigate ", "meaning of ", "define ",
        "describe ", "difference between ", "similarities between ", "difference of ",
        "compare ", "build me a ", "build a ", "build ", "make me a ", "make a ",
        "make ", "create a ", "create ", "send a ", "send ", "how to ", "how does ",
        "how do i ", "how do you ", "how do ", "my friend ", "my boss ", "my colleague ",
        "my wife ", "my husband ", "my mom ", "my dad ", "my sister ", "my brother ",
        nullptr
    };
    
    std::string candidate = query;
    // Strip trailing punctuation from candidate
    while (!candidate.empty() && (candidate.back() == '?' || candidate.back() == '.' || candidate.back() == '!' || candidate.back() == ' '))
        candidate.pop_back();
        
    std::string candidateLower = lower;
    bool stripped = true;
    while (stripped) {
        stripped = false;
        for (int i = 0; PREFIXES[i] != nullptr; ++i) {
            std::string pfx = PREFIXES[i];
            if (candidateLower.compare(0, pfx.size(), pfx) == 0) {
                candidate = candidate.substr(pfx.size());
                candidateLower = candidateLower.substr(pfx.size());
                stripped = true;
                break;
            }
        }
    }
    
    // Trim candidate
    while (!candidate.empty() && candidate.front() == ' ') {
        candidate.erase(candidate.begin());
        candidateLower.erase(candidateLower.begin());
    }
    while (!candidate.empty() && candidate.back() == ' ') {
        candidate.pop_back();
        candidateLower.pop_back();
    }
    
    if (!candidate.empty()) {
        spans.push_back(candidate);
    } else {
        // Fallback: tokenize and check if there's any substantive word
        std::stringstream ss(query);
        std::string token;
        while (ss >> token) {
            std::string clean = "";
            for (char c : token) if (std::isalnum(static_cast<unsigned char>(c))) clean += c;
            if (clean.size() >= 3) spans.push_back(clean);
        }
    }
    
    return spans;
}

EntityLinker::EntityLinker() {}

std::vector<LinkedEntity> EntityLinker::linkEntities(const std::vector<std::string>& spans, const std::string& context, const UserMemory* memory) {
    std::vector<LinkedEntity> linked;
    for (const auto& span : spans) {
        LinkedEntity le;
        le.raw_span = span;
        le.canonical_form = span;
        
        std::string linkSource = "heuristic";
        le.type = heuristicClassify(span, context, memory, linkSource);
        le.link_confidence = 0.90f;
        le.link_source = linkSource;
        linked.push_back(le);
    }
    return linked;
}

EntityType EntityLinker::heuristicClassify(const std::string& span, const std::string& context, const UserMemory* memory, std::string& linkSourceOut) {
    // 1. Lowercase working copies
    std::string lSpan = span;
    std::transform(lSpan.begin(), lSpan.end(), lSpan.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    std::string lCtx = context;
    std::transform(lCtx.begin(), lCtx.end(), lCtx.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    // ── Check 1: AMBIGUOUS / Incomplete ─────────────────────────────────────
    if (lSpan == "what" || lSpan == "who" || lSpan == "why" || lSpan == "how" || lSpan == "when" || lSpan == "where" || lSpan == "it" || lSpan == "this" || lSpan == "that" || lSpan.size() < 2) {
        linkSourceOut = "ambiguous_heuristic";
        return EntityType::AMBIGUOUS;
    }

    // ── Check 2: ACTION ──────────────────────────────────────────────────────
    bool isAction = false;
    if (lSpan.size() > 3 && lSpan.compare(lSpan.size() - 3, 3, "ing") == 0) {
        isAction = true;
    } else {
        static const char* const ACTION_VERBS[] = {
            "code", "run", "build", "make", "create", "delete", "remove", "send", "call", "play", "update", "open", "launch", "install", nullptr
        };
        for (int i = 0; ACTION_VERBS[i] != nullptr; ++i) {
            if (lSpan == ACTION_VERBS[i]) { isAction = true; break; }
        }
    }
    if (isAction) {
        linkSourceOut = "action_heuristic";
        return EntityType::ACTION;
    }

    // ── Check 3: PLACE ───────────────────────────────────────────────────────
    static const char* const PLACES[] = {
        "tokyo", "london", "paris", "new york", "home", "office", "school", "park", "city", "country", "delhi", "mumbai", nullptr
    };
    for (int i = 0; PLACES[i] != nullptr; ++i) {
        if (lSpan == PLACES[i]) {
            linkSourceOut = "place_heuristic";
            return EntityType::PLACE;
        }
    }

    // ── Check 4: PERSON (User Relation or Stated Name) ────────────────────────
    bool hasPersonalPrefix = (lCtx.find("my friend") != std::string::npos ||
                              lCtx.find("my boss") != std::string::npos ||
                              lCtx.find("my wife") != std::string::npos ||
                              lCtx.find("my husband") != std::string::npos ||
                              lCtx.find("my mom") != std::string::npos ||
                              lCtx.find("my dad") != std::string::npos ||
                              lCtx.find("my sister") != std::string::npos ||
                              lCtx.find("my brother") != std::string::npos ||
                              lCtx.find("my colleague") != std::string::npos);

    bool isKnownRelation = false;
    if (memory) {
        if (!memory->getRelationship(span).personName.empty()) {
            isKnownRelation = true;
        }
        std::string userName = memory->getUserName();
        std::string lUserName = userName;
        std::transform(lUserName.begin(), lUserName.end(), lUserName.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (!userName.empty() && lSpan == lUserName) {
            isKnownRelation = true;
        }
    }

    if (hasPersonalPrefix || isKnownRelation) {
        linkSourceOut = "user_relation";
        return EntityType::PERSON;
    }

    // ── Check 5: Famous / Public Person ─────────────────────────────────────
    static const char* const FAMOUS_PEOPLE[] = {
        "albert einstein", "einstein", "isaac newton", "newton", "elon musk", "musk", "shakespeare", "marie curie", "curie", "steve jobs", "bill gates", nullptr
    };
    for (int i = 0; FAMOUS_PEOPLE[i] != nullptr; ++i) {
        if (lSpan == FAMOUS_PEOPLE[i]) {
            linkSourceOut = "public_fact";
            return EntityType::PERSON;
        }
    }

    // ── Check 6: SPECIFIC DISAMBIGUATION FOR "apple" ──────────────────────────
    if (lSpan == "apple") {
        bool fruitCtx = (lCtx.find("eat") != std::string::npos ||
                         lCtx.find("fruit") != std::string::npos ||
                         lCtx.find("organic") != std::string::npos ||
                         lCtx.find("juice") != std::string::npos ||
                         lCtx.find("orchard") != std::string::npos ||
                         lCtx.find("tree") != std::string::npos ||
                         lCtx.find("pie") != std::string::npos ||
                         lCtx.find("delicious") != std::string::npos ||
                         lCtx.find("taste") != std::string::npos ||
                         lCtx.find("red") != std::string::npos ||
                         lCtx.find("green") != std::string::npos ||
                         lCtx.find("banana") != std::string::npos ||
                         lCtx.find("mango") != std::string::npos);

        bool companyCtx = (lCtx.find("company") != std::string::npos ||
                           lCtx.find("stock") != std::string::npos ||
                           lCtx.find("iphone") != std::string::npos ||
                           lCtx.find("macbook") != std::string::npos ||
                           lCtx.find("ipad") != std::string::npos ||
                           lCtx.find("jobs") != std::string::npos ||
                           lCtx.find("cook") != std::string::npos ||
                           lCtx.find("shares") != std::string::npos ||
                           lCtx.find("market") != std::string::npos ||
                           lCtx.find("tech") != std::string::npos ||
                           lCtx.find("nasdaq") != std::string::npos ||
                           lCtx.find("ios") != std::string::npos ||
                           lCtx.find("macos") != std::string::npos ||
                           lCtx.find("device") != std::string::npos ||
                           lCtx.find("phone") != std::string::npos ||
                           lCtx.find("computer") != std::string::npos);

        if (fruitCtx && !companyCtx) {
            linkSourceOut = "disambiguated_fruit";
            return EntityType::OBJECT;
        } else if (companyCtx && !fruitCtx) {
            linkSourceOut = "disambiguated_company";
            return EntityType::CONCEPT;
        } else {
            linkSourceOut = "ambiguous_apple";
            return EntityType::AMBIGUOUS;
        }
    }

    // ── Check 7: OBJECT (Standard Device/App or physical thing) ──────────────
    static const char* const OBJECTS[] = {
        "laptop", "phone", "book", "car", "banana", "desk", "computer", "device", "screen", "mic", "camera", "chrome", "notepad", "spotify", nullptr
    };
    for (int i = 0; OBJECTS[i] != nullptr; ++i) {
        if (lSpan == OBJECTS[i]) {
            linkSourceOut = "object_heuristic";
            return EntityType::OBJECT;
        }
    }

    // ── Check 8: CONCEPT (Topic / science term / abstract idea) ──────────────
    static const char* const CONCEPTS[] = {
        "photosynthesis", "gravity", "evolution", "democracy", "economics", "quantum physics", "science", "math", "history", "climate change", "internet", nullptr
    };
    for (int i = 0; CONCEPTS[i] != nullptr; ++i) {
        if (lSpan == CONCEPTS[i]) {
            linkSourceOut = "concept_heuristic";
            return EntityType::CONCEPT;
        }
    }

    if (DatabaseManager::instance().isOpen()) {
        std::string fact = DatabaseManager::instance().queryLearned(span, 0.40f);
        if (!fact.empty()) {
            linkSourceOut = "database_concept";
            return EntityType::CONCEPT;
        }
    }

    if (span.find(' ') != std::string::npos) {
        linkSourceOut = "multiword_fallback";
        return EntityType::CONCEPT;
    }

    linkSourceOut = "default_fallback";
    return EntityType::CONCEPT;
}
