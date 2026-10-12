#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

namespace yuki { struct TurnResult; }
namespace yuki::memory { class HdcSemanticGraph; }

class ResponseResolver {
public:
    static ResponseResolver& instance();

    // Resolve a static response template key from the DB cache.
    // Language fallback: requestedLang -> DEFAULT_LANGUAGE -> ENGLISH.
    std::string resolve(
        const std::string& responseKey,
        const std::unordered_map<std::string, std::string>& slots = {},
        const std::string& requestedLang = "ENGLISH") const;

    // Resolve templates from TurnResult
    std::string resolve(const yuki::TurnResult& turn_result) const;

    // Resolve a factual/knowledge query from the learned_knowledge table.
    // Checks DB for topic first (min confidence = 0.5).
    // Returns "" if no learned fact exists — caller should fall back to
    // KnowledgeRouter / web search.
    std::string resolveKnowledge(const std::string& topic,
                                  float minConfidence = 0.5f) const;

    // WeightedContextAssembler dependency: when the learned_knowledge table
    // has no answer, triples are assembled from the HDC semantic graph and
    // ranked by their provenance-aware effective weight.
    void setHdcGraph(yuki::memory::HdcSemanticGraph* graph) noexcept { hdc_graph_ = graph; }

private:
    ResponseResolver() = default;
    mutable std::mutex cacheMutex_;

    // Build a reply from the top-weighted triples around `topic`; "" if none
    // clear minConfidence. Also records them for the per-turn feedback loop.
    std::string assembleFromGraph(const std::string& topic, float minConfidence) const;

    yuki::memory::HdcSemanticGraph* hdc_graph_ = nullptr;

    static const std::unordered_map<std::string, std::string> kTemplates;

    static std::string injectSlots(
        const std::string& tmpl,
        const std::unordered_map<std::string, std::string>& slots);
};
