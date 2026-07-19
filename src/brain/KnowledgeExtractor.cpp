// KnowledgeExtractor.cpp — Yuki_1.0
// Pure C++ HTML/text extraction using regex. No scrapling dependency.

#define NOMINMAX
#include "KnowledgeExtractor.h"

#include <regex>
#include <algorithm>
#include <sstream>
#include <cctype>

KnowledgeExtractor::KnowledgeExtractor()  = default;
KnowledgeExtractor::~KnowledgeExtractor() = default;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Strip HTML tags and decode basic HTML entities
static std::string strip_html(const std::string& html) {
    // Remove <script> and <style> blocks entirely
    static const std::regex script_re(
        R"(<(script|style)[^>]*>[\s\S]*?</(script|style)>)",
        std::regex::icase);
    std::string s = std::regex_replace(html, script_re, " ");

    // Strip remaining tags
    static const std::regex tag_re("<[^>]+>");
    s = std::regex_replace(s, tag_re, " ");

    // Basic entity decode
    for (auto& [ent, ch] : std::vector<std::pair<std::string,std::string>>{
            {"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},
            {"&apos;","'"},{"&nbsp;"," "},{"&#39;","'"}})
    {
        std::string::size_type p = 0;
        while ((p = s.find(ent, p)) != std::string::npos) {
            s.replace(p, ent.size(), ch);
            p += ch.size();
        }
    }

    // Normalise whitespace
    static const std::regex ws_re(R"(\s{2,})");
    s = std::regex_replace(s, ws_re, " ");
    // Trim
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    return (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
}

// Pull the first match of a regex group(1) from html, or ""
static std::string first_match(const std::string& html, const std::regex& re) {
    std::smatch m;
    if (std::regex_search(html, m, re) && m.size() > 1)
        return strip_html(m[1].str());
    return {};
}

// Split text into sentences (period / exclamation / question followed by space)
static std::vector<std::string> split_sentences(const std::string& text) {
    std::vector<std::string> sents;
    static const std::regex sent_re(R"([^.!?]+[.!?]?)");
    for (std::sregex_iterator it(text.begin(), text.end(), sent_re), end; it != end; ++it) {
        std::string s = it->str();
        // Trim
        auto l = s.find_first_not_of(" \t\r\n");
        if (l == std::string::npos) continue;
        s = s.substr(l);
        if (s.size() >= 30) sents.push_back(s);
    }
    return sents;
}

static std::vector<std::string> extract_entities(const std::string& text) {
    std::vector<std::string> entities;
    std::regex cap(R"(\b[A-Z][a-z]{2,}\b)");
    for (std::sregex_iterator it(text.begin(), text.end(), cap), end; it != end; ++it)
        entities.push_back(it->str());
    std::sort(entities.begin(), entities.end());
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    return entities;
}

// ── extract_from_html ─────────────────────────────────────────────────────────

ExtractedKnowledge KnowledgeExtractor::extract_from_html(const std::string& html,
                                                          const std::string& url) {
    ExtractedKnowledge ek;

    // Topic: <title> tag
    static const std::regex title_re(R"(<title[^>]*>([\s\S]*?)</title>)",
                                      std::regex::icase);
    ek.topic = first_match(html, title_re);

    // Summary: meta description or og:description
    static const std::regex meta_desc_re(
        R"(<meta[^>]+name\s*=\s*["']description["'][^>]+content\s*=\s*["']([^"']+)["'])",
        std::regex::icase);
    static const std::regex og_desc_re(
        R"(<meta[^>]+property\s*=\s*["']og:description["'][^>]+content\s*=\s*["']([^"']+)["'])",
        std::regex::icase);
    ek.summary = first_match(html, meta_desc_re);
    if (ek.summary.empty()) ek.summary = first_match(html, og_desc_re);

    // JSON-LD
    static const std::regex jsonld_re(
        R"(<script[^>]+type\s*=\s*["']application/ld\+json["'][^>]*>([\s\S]*?)</script>)",
        std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), jsonld_re), end; it != end; ++it)
        ek.json_ld += (*it)[1].str();

    // Body text: strip HTML, then pull sentences as key facts
    std::string body_text = strip_html(html);
    ek.entities = extract_entities(body_text);

    auto sents = split_sentences(body_text);
    for (size_t i = 0; i < sents.size() && i < 5; ++i)
        ek.key_facts.push_back(sents[i]);

    if (ek.summary.empty() && !ek.key_facts.empty())
        ek.summary = ek.key_facts[0].substr(0, 300);

    return ek;
}

// ── extract_from_text ─────────────────────────────────────────────────────────

ExtractedKnowledge KnowledgeExtractor::extract_from_text(const std::string& text,
                                                          const std::string& topic_hint) {
    ExtractedKnowledge ek;
    ek.topic = topic_hint;

    auto sents = split_sentences(text);
    for (size_t i = 0; i < sents.size() && i < 5; ++i)
        ek.key_facts.push_back(sents[i]);

    if (!ek.key_facts.empty())
        ek.summary = ek.key_facts[0].substr(0, 300);

    ek.entities = extract_entities(text);
    return ek;
}
