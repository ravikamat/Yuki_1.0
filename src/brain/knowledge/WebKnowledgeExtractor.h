#pragma once
#include <string>
#include <vector>
#include <utility>
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki::knowledge {

struct ExtractedTriple {
    std::string subject;
    std::string relation;
    std::string object;
    float confidence;
    std::string source_tag;
};

class WebKnowledgeExtractor {
public:
    explicit WebKnowledgeExtractor(yuki::memory::HdcSemanticGraph* graph = nullptr);
    
    void setHdcGraph(yuki::memory::HdcSemanticGraph* graph) noexcept { graph_ = graph; }
    
    // Extract SPO triples from raw web text using pattern rules
    std::vector<ExtractedTriple> extract(const std::string& text, const std::string& source_url = "") const;
    
    // Extract and directly ingest into HDC graph
    size_t extractAndIngest(const std::string& text, const std::string& source_url = "");

private:
    yuki::memory::HdcSemanticGraph* graph_ = nullptr;
    
    // Pattern-based extraction rules
    struct PatternRule {
        std::string pattern;          // Regex pattern
        std::string relation;         // Output relation type
        float base_confidence;        // Base confidence weight
        int subject_group;            // Regex capture group for subject
        int object_group;             // Regex capture group for object
    };
    
    static const std::vector<PatternRule> PATTERN_RULES;
};

} // namespace yuki::knowledge