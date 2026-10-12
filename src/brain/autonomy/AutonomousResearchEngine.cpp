#include "AutonomousResearchEngine.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <iostream>

namespace yuki {
namespace autonomy {

const std::vector<std::string> AutonomousResearchEngine::TOPIC_QUERY_TEMPLATES = {
    "what is {topic}",
    "{topic} explained simply",
    "{topic} definition and examples",
    "how does {topic} work"
};

const std::vector<std::string> AutonomousResearchEngine::TECHNICAL_QUERY_TEMPLATES = {
    "{topic} tutorial",
    "{topic} getting started guide",
    "{topic} API documentation",
    "{topic} python example",
    "{topic} implementation guide"
};

const std::vector<std::string> AutonomousResearchEngine::PROCEDURAL_QUERY_TEMPLATES = {
    "how to {topic}",
    "{topic} step by step",
    "{topic} best practices",
    "{topic} common mistakes"
};

AutonomousResearchEngine::AutonomousResearchEngine() = default;

AutonomousResearchEngine::~AutonomousResearchEngine() = default;

void AutonomousResearchEngine::initialize(SmartScraper* scraper, KnowledgeExtractor* extractor,
                                          HdcSemanticGraph* semantic_graph) {
    scraper_ = scraper;
    extractor_ = extractor;
    semantic_graph_ = semantic_graph;
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutonomousResearchEngine",
        "Initialized with scraper, extractor, and semantic graph");
}

ResearchReport AutonomousResearchEngine::research(const std::string& query, int max_depth) {
    ResearchReport report;
    report.query = query;
    report.completed_at = std::chrono::system_clock::now();

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutonomousResearchEngine",
        "Starting research for: " + query + " (max_depth=" + std::to_string(max_depth) + ")");

    // Start with the main topic
    ResearchNode main_node = researchTopic(query, 0, max_depth);
    report.nodes[query] = main_node;
    report.max_depth_reached = main_node.depth;

    // Resolve unknowns recursively
    resolveUnknowns(report, 0, max_depth);

    // Compute overall confidence
    float total_conf = 0.0f;
    for (const auto& [_, node] : report.nodes) {
        total_conf += node.confidence;
    }
    report.overall_confidence = report.nodes.empty() ? 0.0f : total_conf / report.nodes.size();

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutonomousResearchEngine",
        "Research complete. Nodes: " + std::to_string(report.nodes.size()) +
        ", confidence: " + std::to_string(report.overall_confidence));

    return report;
}

ResearchNode AutonomousResearchEngine::researchTopic(const std::string& topic, int depth, int max_depth) {
    if (depth > max_depth) {
        ResearchNode empty;
        empty.topic = topic;
        empty.depth = depth;
        empty.confidence = 0.0f;
        return empty;
    }

    // Check cache
    auto cache_it = topic_cache_.find(topic);
    if (cache_it != topic_cache_.end()) {
        return cache_it->second;
    }

    // Check if already in semantic graph with high confidence
    if (semantic_graph_) {
        auto stats = semantic_graph_->getAllConceptStats();
        for (const auto& s : stats) {
            if (s.id == topic && s.heuristicStrength > 0.5f) {
                ResearchNode cached;
                cached.topic = topic;
                cached.confidence = std::clamp(s.heuristicStrength, 0.0f, 1.0f);
                cached.depth = depth;
                topic_cache_[topic] = cached;
                return cached;
            }
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "AutonomousResearchEngine",
        "Researching topic: " + topic + " at depth " + std::to_string(depth));

    ResearchNode node;
    node.topic = topic;
    node.depth = depth;
    node.last_updated = std::chrono::system_clock::now();

    // Classify topic type and select query templates
    std::string topic_type = classifyTopicType(topic);
    auto templates = selectQueryTemplates(topic_type);

    // Search web for each template
    std::vector<SearchResult> all_results;
    for (const auto& tmpl : templates) {
        std::string query = tmpl;
        size_t pos = 0;
        while ((pos = query.find("{topic}", pos)) != std::string::npos) {
            query.replace(pos, 7, topic);
            pos += topic.length();
        }
        auto results = searchWeb(query, 5);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    // Deduplicate by URL
    std::sort(all_results.begin(), all_results.end(),
        [](const SearchResult& a, const SearchResult& b) { return a.url < b.url; });
    all_results.erase(std::unique(all_results.begin(), all_results.end(),
        [](const SearchResult& a, const SearchResult& b) { return a.url == b.url; }),
        all_results.end());

    // Fetch and extract from top results
    std::string combined_content;
    for (const auto& result : all_results) {
        if (result.relevance > 0.3f) {
            std::string content = fetchAndExtract(result.url);
            if (!content.empty()) {
                combined_content += "\n--- SOURCE: " + result.url + " ---\n" + content;
                node.source_urls.push_back(result.url);
            }
        }
    }

    // Extract structured knowledge
    auto extracted = extractKnowledge(combined_content, topic, depth);
    for (const auto& ext : extracted) {
        if (ext.topic == topic) {
            node = ext;
            break;
        }
        // Add related topics
        node.related_topics.push_back(ext.topic);
        for (const auto& [k, v] : ext.extracted_facts) {
            node.extracted_facts[k] = v;
        }
    }

    // Extract prerequisites from facts
    if (node.extracted_facts.count("requires")) {
        std::string reqs = node.extracted_facts["requires"];
        std::stringstream ss(reqs);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            if (!item.empty()) node.prerequisites.push_back(item);
        }
    }

    node.confidence = computeTopicConfidence(node);
    topic_cache_[topic] = node;

    return node;
}

void AutonomousResearchEngine::resolveUnknowns(ResearchReport& report, int current_depth, int max_depth) {
    bool found_unknown = true;
    int iteration = 0;

    while (found_unknown && current_depth < max_depth && iteration < 10) {
        found_unknown = false;
        iteration++;
        current_depth++;
        report.max_depth_reached = std::max(report.max_depth_reached, current_depth);

        std::vector<std::string> unknown_terms;

        // Collect all unknown terms from current nodes
        for (auto& [node_topic, node] : report.nodes) {
            for (const auto& prereq : node.prerequisites) {
                if (report.nodes.find(prereq) == report.nodes.end() &&
                    topic_cache_.find(prereq) == topic_cache_.end()) {
                    unknown_terms.push_back(prereq);
                }
            }
            // Also check extracted facts for unknown terms
            for (const auto& [key, value] : node.extracted_facts) {
                auto terms = extractKeyTerms(value);
                for (const auto& term : terms) {
                    if (term.length() > 3 && report.nodes.find(term) == report.nodes.end() &&
                        topic_cache_.find(term) == topic_cache_.end()) {
                        unknown_terms.push_back(term);
                    }
                }
            }
        }

        // Deduplicate
        std::sort(unknown_terms.begin(), unknown_terms.end());
        unknown_terms.erase(std::unique(unknown_terms.begin(), unknown_terms.end()), unknown_terms.end());

        if (!unknown_terms.empty()) {
            found_unknown = true;
            report.unknown_terms.insert(report.unknown_terms.end(), unknown_terms.begin(), unknown_terms.end());

            // Research each unknown term
            for (const auto& term : unknown_terms) {
                if (report.nodes.size() > 100) break; // Safety limit
                ResearchNode new_node = researchTopic(term, current_depth, max_depth);
                report.nodes[term] = new_node;
            }
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutonomousResearchEngine",
        "Resolved unknowns. Total nodes: " + std::to_string(report.nodes.size()) +
        ", max depth: " + std::to_string(report.max_depth_reached));
}

std::vector<std::string> AutonomousResearchEngine::formulateQueries(
    const std::string& target,
    const std::map<std::string, float>& known_topics
) {
    std::vector<std::string> queries;
    std::string topic_type = classifyTopicType(target);
    auto templates = selectQueryTemplates(topic_type);

    for (const auto& tmpl : templates) {
        std::string query = tmpl;
        size_t pos = 0;
        while ((pos = query.find("{topic}", pos)) != std::string::npos) {
            query.replace(pos, 7, target);
            pos += target.length();
        }
        queries.push_back(query);
    }

    // Add gap-based queries
    auto gap_queries = generateQueriesFromGaps(
        [&known_topics]() {
            std::vector<std::string> gaps;
            for (const auto& [topic_name, conf] : known_topics) {
                if (conf < 0.5f) gaps.push_back(topic_name);
            }
            return gaps;
        }(),
        target
    );
    queries.insert(queries.end(), gap_queries.begin(), gap_queries.end());

    return queries;
}

std::vector<std::string> AutonomousResearchEngine::generateQueriesFromGaps(
    const std::vector<std::string>& unknown_terms,
    const std::string& parent_topic
) {
    std::vector<std::string> queries;
    for (const auto& term : unknown_terms) {
        queries.push_back("what is " + term + " in context of " + parent_topic);
        queries.push_back(term + " explained for " + parent_topic);
    }
    return queries;
}

std::vector<ResearchNode> AutonomousResearchEngine::extractKnowledge(const std::string& content, 
                                                                      const std::string& url,
                                                                      int depth) {
    std::vector<ResearchNode> nodes;

    if (!extractor_ || content.empty()) {
        return nodes;
    }

    try {
        ExtractedKnowledge extracted = extractor_->extract_from_text(content, url);
        
        ResearchNode node;
        node.topic = extracted.topic.empty() ? url : extracted.topic;
        node.depth = depth;
        node.source_urls.push_back(url);
        node.last_updated = std::chrono::system_clock::now();

        if (!extracted.summary.empty()) {
            node.extracted_facts["summary"] = extracted.summary;
        }
        for (size_t i = 0; i < extracted.key_facts.size(); ++i) {
            node.extracted_facts["fact_" + std::to_string(i)] = extracted.key_facts[i];
        }
        for (const auto& [q, a] : extracted.qa_pairs) {
            node.extracted_facts[q] = a;
        }
        node.related_topics = extracted.entities;
        node.confidence = computeTopicConfidence(node);
        nodes.push_back(node);
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "AutonomousResearchEngine",
            "Knowledge extraction failed: " + std::string(e.what()));
    }

    return nodes;
}

std::vector<AutonomousResearchEngine::SearchResult> AutonomousResearchEngine::searchWeb(
    const std::string& query, int max_results) {
    std::vector<SearchResult> results;

    if (!scraper_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "AutonomousResearchEngine",
            "No scraper available for web search");
        return results;
    }

    try {
        // Use duckduckgo html or google custom search
        std::string search_url = "https://html.duckduckgo.com/html/?q=" + query;
        std::replace(search_url.begin(), search_url.end(), ' ', '+');

        auto page = scraper_->scrape(search_url, 10000);
        if (page.clean_text.empty()) return results;

        // Parse text for snippets/results
        std::string content = page.clean_text;
        std::stringstream ss(content);
        std::string line;
        int count = 0;
        while (std::getline(ss, line) && count < max_results) {
            if (line.length() > 20) {
                SearchResult sr;
                sr.title = line.substr(0, 50);
                sr.snippet = line;
                sr.relevance = 1.0f - (count * 0.1f);
                sr.url = "https://duckduckgo.com/html/?q=" + query;
                results.push_back(sr);
                count++;
            }
        }
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "AutonomousResearchEngine",
            "Web search failed: " + std::string(e.what()));
    }

    return results;
}

std::string AutonomousResearchEngine::fetchAndExtract(const std::string& url) {
    if (!scraper_) return "";

    try {
        auto page = scraper_->scrape(url, 15000);
        if (page.clean_text.empty()) return "";

        return page.clean_text.substr(0, 50000); // Limit size
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "AutonomousResearchEngine",
            "Fetch failed for " + url + ": " + e.what());
        return "";
    }
}

std::string AutonomousResearchEngine::classifyTopicType(const std::string& topic) {
    std::string lower = topic;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static const std::vector<std::string> technical_keywords = {
        "api", "sdk", "library", "framework", "protocol", "algorithm",
        "database", "encryption", "authentication", "compiler", "runtime"
    };

    static const std::vector<std::string> procedural_keywords = {
        "how to", "setup", "install", "configure", "deploy", "build",
        "create", "implement", "integrate", "automate"
    };

    for (const auto& kw : technical_keywords) {
        if (lower.find(kw) != std::string::npos) return "technical";
    }
    for (const auto& kw : procedural_keywords) {
        if (lower.find(kw) != std::string::npos) return "procedural";
    }
    return "conceptual";
}

std::vector<std::string> AutonomousResearchEngine::selectQueryTemplates(const std::string& topic_type) {
    if (topic_type == "technical") return TECHNICAL_QUERY_TEMPLATES;
    if (topic_type == "procedural") return PROCEDURAL_QUERY_TEMPLATES;
    return TOPIC_QUERY_TEMPLATES;
}

std::string AutonomousResearchEngine::cleanExtractedText(const std::string& raw) {
    std::string text = raw;
    // Remove scripts, styles, HTML tags
    text = std::regex_replace(text, std::regex("<script[^>]*>.*?</script>", std::regex_constants::icase), std::string(""));
    text = std::regex_replace(text, std::regex("<style[^>]*>.*?</style>", std::regex_constants::icase), std::string(""));
    text = std::regex_replace(text, std::regex("<[^>]+>"), std::string(" "));
    text = std::regex_replace(text, std::regex("\\s+"), std::string(" "));
    text = std::regex_replace(text, std::regex("&[a-z]+;"), std::string(" "));
    return text;
}

std::vector<std::string> AutonomousResearchEngine::extractKeyTerms(const std::string& text) {
    std::vector<std::string> terms;
    std::regex word_regex(R"(\b[A-Z][a-z]+(?:[A-Z][a-z]+)*\b)"); // CamelCase
    std::regex tech_regex(R"(\b(?:API|SDK|HTTP|REST|JSON|SQL|AI|ML|NLP|UI|UX|CLI|GUI)\b)");
    
    std::smatch match;
    std::string::const_iterator start(text.cbegin());
    
    while (std::regex_search(start, text.cend(), match, word_regex)) {
        terms.push_back(match[0]);
        start = match.suffix().first;
    }
    
    start = text.cbegin();
    while (std::regex_search(start, text.cend(), match, tech_regex)) {
        terms.push_back(match[0]);
        start = match.suffix().first;
    }

    // Deduplicate
    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    return terms;
}

float AutonomousResearchEngine::computeTopicConfidence(const ResearchNode& node) {
    float confidence = 0.0f;
    
    // Source diversity
    confidence += std::min(node.source_urls.size() * 0.15f, 0.4f);
    
    // Fact richness
    confidence += std::min(node.extracted_facts.size() * 0.05f, 0.3f);
    
    // Prerequisite resolution
    confidence += std::min(node.prerequisites.size() * 0.05f, 0.15f);
    
    // Related topics
    confidence += std::min(node.related_topics.size() * 0.03f, 0.15f);
    
    return std::clamp(confidence, 0.0f, 1.0f);
}

float AutonomousResearchEngine::validateExtractedFacts(const ResearchNode& node) {
    return computeTopicConfidence(node);
}

bool AutonomousResearchEngine::isConceptSufficientlyKnown(const std::string& concept_name, float threshold) {
    auto it = topic_cache_.find(concept_name);
    if (it != topic_cache_.end()) {
        return it->second.confidence >= threshold;
    }
    if (semantic_graph_) {
        auto stats = semantic_graph_->getAllConceptStats();
        for (const auto& s : stats) {
            if (s.id == concept_name) {
                return (s.accessCount > 0 || s.reinforcementCount > 0);
            }
        }
    }
    return false;
}

} // namespace autonomy
} // namespace yuki