#include "brain/knowledge/WebKnowledgeExtractor.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace yuki::knowledge {

const std::vector<WebKnowledgeExtractor::PatternRule> WebKnowledgeExtractor::PATTERN_RULES = {
    // "X is a Y" -> (X, IsA, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+is\s+a\s+([a-z]+(?:\s+[a-z]+)*)\b)", "IsA", 0.5f, 1, 2},
    
    // "X are Y" -> (X, IsA, Y) - plural
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+are\s+([a-z]+(?:\s+[a-z]+)*)\b)", "IsA", 0.5f, 1, 2},
    
    // "X can Y" -> (X, CapableOf, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+can\s+([a-z]+(?:\s+[a-z]+)*)\b)", "CapableOf", 0.4f, 1, 2},
    
    // "X has Y" -> (X, HasProperty, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+has\s+([a-z]+(?:\s+[a-z]+)*)\b)", "HasProperty", 0.4f, 1, 2},
    
    // "X is used for Y" -> (X, UsedFor, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+is\s+used\s+for\s+([a-z]+(?:\s+[a-z]+)*)\b)", "UsedFor", 0.45f, 1, 2},
    
    // "X causes Y" -> (X, Causes, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+causes\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Causes", 0.5f, 1, 2},
    
    // "X is part of Y" -> (X, PartOf, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+is\s+part\s+of\s+([a-z]+(?:\s+[a-z]+)*)\b)", "PartOf", 0.45f, 1, 2},
    
    // "X belongs to Y" -> (X, PartOf, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+belongs\s+to\s+([a-z]+(?:\s+[a-z]+)*)\b)", "PartOf", 0.45f, 1, 2},
    
    // "X contains Y" -> (X, Contains, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+contains\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Contains", 0.4f, 1, 2},
    
    // "X makes Y" -> (X, Creates, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+makes\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Creates", 0.4f, 1, 2},
    
    // "X produces Y" -> (X, Creates, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+produces\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Creates", 0.4f, 1, 2},
    
    // "X requires Y" -> (X, Requires, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+requires\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Requires", 0.4f, 1, 2},
    
    // "X needs Y" -> (X, Requires, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+needs\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Requires", 0.4f, 1, 2},
    
    // "X enables Y" -> (X, Enables, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+enables\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Enables", 0.4f, 1, 2},
    
    // "X prevents Y" -> (X, Prevents, Y)
    {R"(\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\s+prevents\s+([a-z]+(?:\s+[a-z]+)*)\b)", "Prevents", 0.4f, 1, 2},
};

WebKnowledgeExtractor::WebKnowledgeExtractor(yuki::memory::HdcSemanticGraph* graph)
    : graph_(graph) {}

std::vector<ExtractedTriple> WebKnowledgeExtractor::extract(const std::string& text, const std::string& source_url) const {
    std::vector<ExtractedTriple> results;
    
    for (const auto& rule : PATTERN_RULES) {
        std::regex re(rule.pattern, std::regex::icase);
        std::smatch match;
        
        std::string::const_iterator search_start = text.cbegin();
        while (std::regex_search(search_start, text.cend(), match, re)) {
            if (match.size() > static_cast<size_t>(std::max(rule.subject_group, rule.object_group))) {
                ExtractedTriple triple;
                triple.subject = match[rule.subject_group].str();
                triple.object = match[rule.object_group].str();
                triple.relation = rule.relation;
                triple.confidence = rule.base_confidence;
                triple.source_tag = "web:" + source_url;
                
                // Normalize: lowercase, trim
                std::transform(triple.subject.begin(), triple.subject.end(), triple.subject.begin(), ::tolower);
                std::transform(triple.object.begin(), triple.object.end(), triple.object.begin(), ::tolower);
                std::transform(triple.relation.begin(), triple.relation.end(), triple.relation.begin(), ::tolower);
                
                // Capitalize relation for consistency
                if (!triple.relation.empty()) {
                    triple.relation[0] = std::toupper(triple.relation[0]);
                }
                
                results.push_back(std::move(triple));
            }
            search_start = match.suffix().first;
        }
    }
    
    return results;
}

size_t WebKnowledgeExtractor::extractAndIngest(const std::string& text, const std::string& source_url) {
    if (!graph_) return 0;
    
    auto triples = extract(text, source_url);
    size_t ingested = 0;
    
    for (const auto& triple : triples) {
        // Use TripleWeight with provenance
        yuki::memory::TripleWeight tw;
        tw.base_weight = triple.confidence;
        tw.sources.push_back(triple.source_tag);
        
        if (graph_->ingestProposition(triple.subject, triple.relation, triple.object, tw)) {
            ++ingested;
        }
    }
    
    return ingested;
}

} // namespace yuki::knowledge