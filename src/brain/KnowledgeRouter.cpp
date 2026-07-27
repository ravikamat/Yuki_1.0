#include "KnowledgeRouter.h"
#include "database/DatabaseManager.h"
#include "brain/core/ConfigManager.h"
#include <sstream>

KnowledgeRouter::KnowledgeRouter() : lkb_("knowledge.db") {
    lkb_.initialize();
    bootstrapConcepts();
}

// bootstrapConcepts() seeds knowledge.db (legacy path).
// Identity facts now also live in learned_knowledge via v5 DB seed.
// The two paths coexist: lkb_ is checked first, then DatabaseManager.
void KnowledgeRouter::bootstrapConcepts() {
    std::unordered_map<std::string, std::string> templates;
    yuki::ConfigManager::instance().loadTemplates("data/identity_templates.txt", templates);

    if (lkb_.queryKey("yuki").empty()) {
        KnowledgeRecord r1;
        r1.id = "id_1"; r1.domain = "identity"; r1.key = "yuki";
        std::string val = templates.count("yuki_identity") ? templates["yuki_identity"] : templates["SELF_INTRO"];
        r1.value = !val.empty() ? val : "Yuki Agentic System";
        r1.source = "bootstrap"; r1.confidence = 1.0f; r1.timestamp = 0;
        lkb_.storeFact(r1);
    }

    if (lkb_.queryKey("creator").empty()) {
        KnowledgeRecord r2;
        r2.id = "id_2"; r2.domain = "identity"; r2.key = "creator";
        std::string val = yuki::ConfigManager::instance().getTemplate("CREATOR_INFO");
        r2.value = !val.empty() ? val : "I was created by RahulRavi.";
        r2.source = "bootstrap"; r2.confidence = 1.0f; r2.timestamp = 0;
        lkb_.storeFact(r2);
    }

    if (lkb_.queryKey("who are you").empty()) {
        KnowledgeRecord r3;
        r3.id = "id_3"; r3.domain = "identity"; r3.key = "who are you";
        std::string val = templates.count("yuki_identity") ? templates["yuki_identity"] : templates["SELF_SHORT"];
        r3.value = !val.empty() ? val : "Yuki Agentic System";
        r3.source = "bootstrap"; r3.confidence = 1.0f; r3.timestamp = 0;
        lkb_.storeFact(r3);
    }

    if (lkb_.queryKey("quantum physics").empty()) {
        KnowledgeRecord r4;
        r4.id = "id_4"; r4.domain = "physics"; r4.key = "quantum physics";
        r4.value = "Quantum physics is the study of matter and energy at the most fundamental level, uncovering the properties and behaviors of the very small.";
        r4.source = "bootstrap"; r4.confidence = 0.9f; r4.timestamp = 0;
        lkb_.storeFact(r4);
    }
}

static bool isWeakSummaryLocal(const std::string& s) {
    if (s.empty()) return true;
    if (s.size() < 15) return true;
    if (s.find("No local knowledge or web results") != std::string::npos) return true;
    if (s.find("Insufficient parameters")           != std::string::npos) return true;
    if (s.find("Found online for")                  != std::string::npos && s.size() < 60) return true;
    if (s.find("Jump to content")                   != std::string::npos) return true;
    if (s.find("Main menu")                         != std::string::npos) return true;
    if (s.find("Retrieved from")                    != std::string::npos) return true;
    if (s.find("still learning")                    != std::string::npos) return true;
    return false;
}

FactBundle KnowledgeRouter::route(const MeaningState& state) {
    FactBundle bundle;

    std::string searchKey = state.goal.target;
    if (searchKey.empty() && !state.goal.parameters.empty()) {
        searchKey = state.goal.parameters.begin()->second;
    }

    if (!searchKey.empty()) {
        // ── Step 1: legacy knowledge.db (lkb_) ───────────────────────────────
        auto records = lkb_.queryKey(searchKey);
        if (records.empty()) {
            records = lkb_.queryDomain(searchKey);
        }

        if (!records.empty()) {
            std::ostringstream ss;
            for (const auto& r : records) {
                ss << r.value << "\n";
                bundle.sources.push_back(r.source);
            }
            bundle.summary = ss.str();
            while (!bundle.summary.empty() && bundle.summary.back() == '\n')
                bundle.summary.pop_back();
            return bundle;
        }

        // ── Step 2: unified DB path (learned_knowledge in yuki_global.db) ────
        // This is where v5 bootstrap seed facts, daemon-learned facts, and
        // web_fallback facts all live. Checked before web fetch so bootstrap
        // knowledge is always available on first run.
        if (DatabaseManager::instance().isOpen()) {
            std::string dbFact = DatabaseManager::instance().queryLearned(searchKey, 0.40f);
            if (!dbFact.empty()) {
                bundle.summary = dbFact;
                bundle.sources.push_back("learned_db");
                return bundle;
            }
        }

        // ── Step 3: web fallback ──────────────────────────────────────────────
        if (state.goal.action == "CONVERSATION" || state.goal.target.empty()) {
            return bundle;  // empty — caller handles it
        }
        std::string url = "https://en.wikipedia.org/wiki/" + searchKey;
        std::string html = scraper_.fetchHtml(url);
        if (!html.empty()) {
            std::string text = scraper_.extractSemanticText(html);
            // Truncate cleanly at sentence boundary when possible
            if (text.size() > 400) {
                size_t cutoff = text.rfind('.', 400);
                if (cutoff != std::string::npos && cutoff > 100)
                    text = text.substr(0, cutoff + 1);
                else
                    text = text.substr(0, 400) + "...";
            }
            if (text.size() >= 30 && !isWeakSummaryLocal(text)) {
                bundle.summary = text;
                bundle.sources.push_back("web_fallback");
                // Store in legacy lkb_ for fast repeat lookup this session
                KnowledgeRecord r;
                r.id        = "web_" + searchKey;
                r.domain    = "web";
                r.key       = searchKey;
                r.value     = text;
                r.source    = url;
                r.confidence = 0.7f;
                r.timestamp = 0;
                lkb_.storeFact(r);
            }
        }
        // If html empty or too short: return empty bundle — caller shows smart fallback

    } else {
        // No search key — return empty, not error string
        bundle.summary.clear();
    }

    return bundle;
}
