#pragma once
#include "brain/SmartScraper.h"
#include "brain/KnowledgeExtractor.h"
#include "brain/memory/HdcSemanticGraph.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace yuki {
namespace autonomy {

using yuki::memory::HdcSemanticGraph;

struct ResearchNode {
    std::string topic;
    float confidence = 0.0f;
    std::vector<std::string> prerequisites;
    std::vector<std::string> related_topics;
    std::map<std::string, std::string> extracted_facts;
    std::vector<std::string> source_urls;
    std::chrono::system_clock::time_point last_updated;
    int depth = 0;
};

struct ResearchReport {
    std::string query;
    std::map<std::string, ResearchNode> nodes;
    std::vector<std::string> execution_path;
    std::vector<std::string> unknown_terms;
    float overall_confidence = 0.0f;
    std::chrono::system_clock::time_point completed_at;
    int max_depth_reached = 0;
};

struct SearchResult {
    std::string url;
    std::string title;
    std::string snippet;
    float relevance = 0.0f;
};

class AutonomousResearchEngine {
public:
    using ResearchReport = yuki::autonomy::ResearchReport;
    using ResearchNode = yuki::autonomy::ResearchNode;
    using SearchResult = yuki::autonomy::SearchResult;

    AutonomousResearchEngine();
    ~AutonomousResearchEngine();

    void initialize(SmartScraper* scraper, KnowledgeExtractor* extractor, 
                    HdcSemanticGraph* semantic_graph = nullptr);

    ResearchReport research(const std::string& query, int max_depth = 5);
    
    std::vector<std::string> formulateQueries(
        const std::string& target,
        const std::map<std::string, float>& known_topics
    );

    std::vector<ResearchNode> extractKnowledge(const std::string& content, 
                                                const std::string& url,
                                                int depth);

    void resolveUnknowns(ResearchReport& report, int current_depth, int max_depth);

    // Query formulation based on ignorance gaps
    std::vector<std::string> generateQueriesFromGaps(
        const std::vector<std::string>& unknown_terms,
        const std::string& parent_topic
    );

    // Recursive topic resolution
    ResearchNode researchTopic(const std::string& topic, int depth, int max_depth);

    // Web search and extraction
    std::vector<SearchResult> searchWeb(const std::string& query, int max_results = 10);
    std::string fetchAndExtract(const std::string& url);

    // Knowledge validation
    float validateExtractedFacts(const ResearchNode& node);
    bool isConceptSufficientlyKnown(const std::string& concept_name, float threshold = 0.6f);

private:
    SmartScraper* scraper_ = nullptr;
    KnowledgeExtractor* extractor_ = nullptr;
    HdcSemanticGraph* semantic_graph_ = nullptr;

    std::map<std::string, ResearchNode> topic_cache_;
    std::vector<std::string> researched_in_session_;

    // Query templates for different topic types
    static const std::vector<std::string> TOPIC_QUERY_TEMPLATES;
    static const std::vector<std::string> TECHNICAL_QUERY_TEMPLATES;
    static const std::vector<std::string> PROCEDURAL_QUERY_TEMPLATES;

    std::string classifyTopicType(const std::string& topic);
    std::vector<std::string> selectQueryTemplates(const std::string& topic_type);
    std::string cleanExtractedText(const std::string& raw);
    std::vector<std::string> extractKeyTerms(const std::string& text);
    float computeTopicConfidence(const ResearchNode& node);
};

} // namespace autonomy
} // namespace yuki