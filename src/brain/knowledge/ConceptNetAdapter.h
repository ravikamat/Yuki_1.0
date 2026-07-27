#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <unordered_set>

namespace yuki::knowledge {

struct ConceptNetAssertion {
    std::string relation;
    std::string start_concept;      // e.g., "dog"
    std::string end_concept;        // e.g., "animal"
    float weight = 0.0f;
    std::string dataset;
    uint64_t line_number = 0;
};

class ConceptNetAdapter {
public:
    explicit ConceptNetAdapter(const std::string& config_path);

    struct ParseStats {
        uint64_t total = 0;
        uint64_t filtered_lang = 0;
        uint64_t filtered_weight = 0;
        uint64_t filtered_trivial = 0;
        uint64_t deduped = 0;
        uint64_t accepted = 0;
    };

    // Streaming parse: calls callback for each valid assertion.
    ParseStats parseStream(const std::string& csv_path,
                           std::function<bool(const ConceptNetAssertion&)> callback);

    // Pre-flight estimate sampling
    ParseStats estimate(const std::string& csv_path);

    std::string normalizeConcept(const std::string& raw) const;

private:
    float min_weight_ = 2.0f;
    std::string target_lang_ = "en";
    std::vector<std::string> blocked_relations_;
    std::unordered_set<std::string> blocked_concepts_;

    bool isValid(const ConceptNetAssertion& a) const;
    uint64_t computeTripletHash(const ConceptNetAssertion& a) const;
};

} // namespace yuki::knowledge
