#pragma once
#include "brain/learning/FastText.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace yuki::ethics {

struct EthicalPrinciple {
    std::string id;
    std::string name;
    std::string source;            // e.g., "BG 2.47"
    std::string teaching;          // full text
    float base_weight = 1.0f;
    std::vector<std::string> domains;
    std::vector<std::string> keywords;
};

struct ValueAlignmentReport {
    float alignment_score = 0.0f;  // [-1.0, +1.0]
    std::vector<std::pair<std::string, float>> top_principles; // id -> relevance
    std::vector<std::string> warnings; // principles that would be violated
};

class ValueConstitution {
public:
    ValueConstitution();
    explicit ValueConstitution(yuki::learning::FastText* ft);

    // Load from data/gita_constitution.jsonl
    bool load(const std::string& path);

    ValueAlignmentReport evaluate(const std::string& action_description,
                                   const std::vector<std::string>& context_tags) const;

    const EthicalPrinciple* getPrinciple(const std::string& id) const;

    void save(const std::string& path) const;
    bool loadBinary(const std::string& path);

    size_t principleCount() const { return principles_.size(); }
    void setFastText(yuki::learning::FastText* ft) { ft_ = ft; }

private:
    yuki::learning::FastText* ft_ = nullptr;
    std::unordered_map<std::string, EthicalPrinciple> principles_;
    std::unordered_map<std::string, std::vector<std::string>> keyword_index_;

    float computeRelevance(const EthicalPrinciple& p,
                           const std::string& action,
                           const std::vector<std::string>& tags) const;
    float computeAlignment(const EthicalPrinciple& p,
                          const std::string& action) const;

    std::string extractJsonString(const std::string& json, const std::string& key) const;
    float extractJsonFloat(const std::string& json, const std::string& key, float default_val = 1.0f) const;
    std::vector<std::string> extractJsonArray(const std::string& json, const std::string& key) const;
};

} // namespace yuki::ethics
