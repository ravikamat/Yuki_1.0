// DocReader.cpp — Real implementation
// Yuki_1.0 — Document fetch and structured content extraction
//
// Fetches documentation/article content for a GoalModel topic using
// SmartScraper → returns cleaned text for KnowledgeExtractor.

#include "DocReader.h"
#include "SmartScraper.h"
#include "brain/learning/LearningIngestor.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iostream>

static std::string toLowerTrim(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    size_t a = r.find_first_not_of(" \t\r\n");
    size_t b = r.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : r.substr(a, b - a + 1);
}

std::string DocReader::learn(const GoalModel& model) {
    std::string topic = toLowerTrim(model.goal);
    if (topic.empty()) return "";

    // ── Topic quality gate ────────────────────────────────────────────────────
    // Reject topics that are clearly not researchable:
    //   1. Too short to be meaningful.
    //   2. Chat/boilerplate strings that slipped through routing.
    //   3. The raw action name (e.g. "build app", "task request").
    if (topic.size() < 4) return "";

    static const char* const BLOCK_TOPICS[] = {
        "build app", "task request", "task_request", "unknown", "understand_input",
        "answer_query", "converse", "describe_self", "provide_support",
        "hi", "hey", "hello", "ok", "yes", "no", "cool", "thanks",
        nullptr
    };
    for (int i = 0; BLOCK_TOPICS[i]; ++i)
        if (topic == BLOCK_TOPICS[i]) return "";

    // Reject if it looks like a raw GoalModel action string (no spaces, all caps-or-underscore)
    bool hasAlpha = false, hasLower = false;
    for (char c : topic) {
        if (std::isalpha((unsigned char)c)) { hasAlpha = true; }
        if (std::islower((unsigned char)c)) { hasLower = true; }
    }
    if (hasAlpha && !hasLower) return "";   // "KNOWLEDGE_QUERY", "BUILD_APP" etc.

    // Reject very long raw sentences — means prefix stripping failed
    if (topic.size() > 60) return "";
    // ── End gate ──────────────────────────────────────────────────────────────

    SmartScraper scraper;

    // ── Source priority list ─────────────────────────────────────────────────
    // Try multiple sources in order of quality. Return first non-trivial result.
    // Wikipedia Simple → Wikipedia EN → MDN (for tech topics)
    struct Source { std::string url; std::string name; float baseConf; };

    // Encode spaces as underscores for Wikipedia URLs
    std::string wikiTopic = topic;
    std::replace(wikiTopic.begin(), wikiTopic.end(), ' ', '_');

    std::vector<Source> sources = {
        { "https://simple.wikipedia.org/wiki/" + wikiTopic, "wikipedia_simple", 0.82f },
        { "https://en.wikipedia.org/wiki/"     + wikiTopic, "wikipedia_en",     0.78f },
    };

    // For programming/tech topics try MDN as well
    if (model.domain == "tech" || model.domain == "programming" ||
        topic.find("python") != std::string::npos ||
        topic.find("javascript") != std::string::npos ||
        topic.find("api") != std::string::npos)
    {
        std::string mdnTopic = wikiTopic;
        sources.push_back({ "https://developer.mozilla.org/en-US/search?q=" + mdnTopic,
                            "mdn", 0.75f });
    }

    std::string bestResult;
    float bestConf = 0.0f;
    std::string bestSource;

    for (const auto& src : sources) {
        std::string html = scraper.fetchHtml(src.url, 6000);
        if (html.empty()) continue;

        std::string text = scraper.extractSemanticText(html);

        // Skip pages with no real content
        if (text.size() < 150) continue;

        // Truncate to a clean paragraph length for fact storage
        if (text.size() > 1200) text = text.substr(0, 1200);

        if (text.size() > bestResult.size()) {
            bestResult = text;
            bestConf   = src.baseConf;
            bestSource = src.name;
        }
    }

    if (!bestResult.empty() && !topic.empty()) {
        // Submit to background ingestor — non-blocking
        int64_t ts = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        LearningIngestor::instance().submit({
            topic,
            bestResult.substr(0, 800),   // clean summary portion
            bestSource,
            bestConf,
            ts,
            ""    // related — populated later by KnowledgeExtractor
        });
        std::cout << "[DocReader] Fetched and submitted: topic=" << topic
                  << " src=" << bestSource << " len=" << bestResult.size() << "\n";
    }

    return bestResult;
}
