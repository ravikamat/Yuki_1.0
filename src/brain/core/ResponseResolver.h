#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

namespace yuki { struct TurnResult; }

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

private:
    ResponseResolver() = default;
    mutable std::mutex cacheMutex_;

    static const std::unordered_map<std::string, std::string> kTemplates;

    static std::string injectSlots(
        const std::string& tmpl,
        const std::unordered_map<std::string, std::string>& slots);
};
