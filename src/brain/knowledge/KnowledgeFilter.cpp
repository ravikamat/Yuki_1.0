#include "brain/knowledge/KnowledgeFilter.h"
#include <sstream>
#include <algorithm>

namespace yuki::knowledge {

KnowledgeFilter::KnowledgeFilter(yuki::language::Word2Vec* w2v)
    : w2v_(w2v) {}

float KnowledgeFilter::computeCoverage(const std::string& phrase) const {
    if (!w2v_ || phrase.empty()) return 1.0f; // Default 1.0 if no Word2Vec dictionary passed

    std::stringstream ss(phrase);
    std::string token;
    size_t total = 0;
    size_t known = 0;

    while (ss >> token) {
        total++;
        if (w2v_->hasWord(token)) {
            known++;
        }
    }

    if (total == 0) return 0.0f;
    return static_cast<float>(known) / static_cast<float>(total);
}

std::unique_ptr<FilteredTriplet> KnowledgeFilter::filter(const ConceptNetAssertion& assertion) {
    float start_cov = computeCoverage(assertion.start_concept);
    float end_cov = computeCoverage(assertion.end_concept);

    if (start_cov < min_coverage_ || end_cov < min_coverage_) {
        rejected_coverage_++;
        return nullptr;
    }

    auto triplet = std::make_unique<FilteredTriplet>();
    triplet->relation = assertion.relation;
    triplet->start = assertion.start_concept;
    triplet->end = assertion.end_concept;
    triplet->weight = assertion.weight;
    triplet->start_coverage = start_cov;
    triplet->end_coverage = end_cov;

    std::string key = triplet->relation + "|" + triplet->start + "|" + triplet->end;
    uint64_t hash = 14695981039346656037ULL;
    for (char c : key) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    triplet->hash = hash;

    accepted_++;
    sum_start_cov_ += start_cov;
    sum_end_cov_ += end_cov;

    return triplet;
}

std::vector<FilteredTriplet> KnowledgeFilter::filterBatch(const std::vector<ConceptNetAssertion>& batch) {
    std::vector<FilteredTriplet> result;
    result.reserve(batch.size());

    for (const auto& a : batch) {
        auto ft = filter(a);
        if (ft) {
            result.push_back(std::move(*ft));
        }
    }
    return result;
}

KnowledgeFilter::CoverageStats KnowledgeFilter::getCoverageStats() const {
    CoverageStats stats;
    if (accepted_ > 0) {
        stats.start_avg = static_cast<float>(sum_start_cov_ / accepted_);
        stats.end_avg = static_cast<float>(sum_end_cov_ / accepted_);
        stats.overall = (stats.start_avg + stats.end_avg) * 0.5f;
    }
    return stats;
}

} // namespace yuki::knowledge
