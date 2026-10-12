#pragma once
// KnowledgeExtractor.h — Yuki_1.0
// Pure C++ HTML/text extraction. No scrapling::Selector dependency.

#include <string>
#include <vector>
#include <utility>

// A subject/predicate/object fact surfaced by the OpenIE extractor.
struct SpoTriple {
    std::string subject;
    std::string predicate;
    std::string object;
    float       confidence = 0.5f;
};

struct ExtractedKnowledge {
    std::string topic;
    std::string summary;
    std::vector<std::string> key_facts;
    std::vector<std::pair<std::string, std::string>> qa_pairs; // question, answer
    std::vector<std::string> entities;
    // OpenIE SPO triples. Previously extracted and then discarded (the old
    // code noted "not stored in ExtractedKnowledge"); exposed here so callers
    // can ingest them into HdcSemanticGraph.
    std::vector<SpoTriple> spo_triples;
    std::string json_ld;
};

class KnowledgeExtractor {
public:
    KnowledgeExtractor();
    ~KnowledgeExtractor();

    // Primary API: extract from raw HTML + optional URL hint
    ExtractedKnowledge extract_from_html(const std::string& html,
                                          const std::string& url = "");

    // Convenience: extract from pre-cleaned plain text
    ExtractedKnowledge extract_from_text(const std::string& text,
                                          const std::string& topic_hint = "");
};
