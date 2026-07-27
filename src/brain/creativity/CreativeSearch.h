#pragma once
#include <vector>
#include <cstdint>
#include <memory>

namespace yuki { namespace creativity {

enum class SearchMode { DIVERGENT, CONVERGENT };

struct SearchResult {
    std::vector<double> conceptVector;
    double novelty = 0.0;
    double utility = 0.0;
    double coherence = 0.0;
    double value = 0.0;
    size_t iterations = 0;
};

class CreativeSearch {
public:
    explicit CreativeSearch(size_t embeddingDim);
    ~CreativeSearch();
    CreativeSearch(const CreativeSearch&) = delete;
    CreativeSearch& operator=(const CreativeSearch&) = delete;
    CreativeSearch(CreativeSearch&&) noexcept;
    CreativeSearch& operator=(CreativeSearch&&) noexcept;

    void setKnownConcepts(const std::vector<std::vector<double>>& concepts);
    void setGoalVector(const std::vector<double>& goal);
    void setRepulsionStrength(double lambda);      // default 0.1
    void setStepSize(double eta);                  // default 0.01
    void setMaxIterations(size_t maxIter);         // default 100
    void setConvergenceThreshold(double threshold); // default 1e-6

    SearchResult search(const std::vector<double>& start,
                        SearchMode mode,
                        size_t numCandidates = 5);

    double evaluateValue(const std::vector<double>& concept) const;
    double evaluateNovelty(const std::vector<double>& concept) const;
    double evaluateUtility(const std::vector<double>& concept) const;
    double evaluateCoherence(const std::vector<double>& concept) const;

    // Binary serialization: magic = 0x43525343 ('CRSC')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    size_t getEmbeddingDim() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::creativity
