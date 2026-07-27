#pragma once
#include "brain/knowledge/ConceptNetAdapter.h"
#include "brain/language/Word2Vec.h"
#include <memory>
#include <vector>
#include <string>

namespace yuki::knowledge {

struct FilteredTriplet {
    std::string relation;
    std::string start;
    std::string end;
    float weight = 0.0f;
    float start_coverage = 0.0f;
    float end_coverage = 0.0f;
    uint64_t hash = 0;
};

class KnowledgeFilter {
public:
    explicit KnowledgeFilter(yuki::language::Word2Vec* w2v);

    // Multi-stage filter. Returns nullptr if rejected, owned pointer if accepted.
    std::unique_ptr<FilteredTriplet> filter(const ConceptNetAssertion& assertion);

    // Batch filter for high throughput.
    std::vector<FilteredTriplet> filterBatch(const std::vector<ConceptNetAssertion>& batch);

    struct CoverageStats {
        float start_avg = 0.0f;
        float end_avg = 0.0f;
        float overall = 0.0f;
    };
    CoverageStats getCoverageStats() const;

private:
    yuki::language::Word2Vec* w2v_;
    float min_coverage_ = 0.5f;
    size_t accepted_ = 0;
    size_t rejected_coverage_ = 0;
    double sum_start_cov_ = 0.0;
    double sum_end_cov_ = 0.0;

    float computeCoverage(const std::string& phrase) const;
};

} // namespace yuki::knowledge
