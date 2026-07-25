#include "brain/creativity/CreativeSearch.h"
#include "brain/core/Logger.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>

namespace yuki { namespace creativity {

class CreativeSearch::Impl {
public:
    size_t embeddingDim_;
    std::vector<std::vector<double>> knownConcepts_;
    std::vector<double> goalVector_;
    double lambda_ = 0.1;
    double eta_ = 0.01;
    size_t maxIter_ = 100;
    double threshold_ = 1e-6;
    mutable std::mt19937_64 rng_{42};

    explicit Impl(size_t dim) : embeddingDim_(dim) {
        goalVector_.resize(dim, 1.0 / std::sqrt(static_cast<double>(dim)));
    }

    double norm(const std::vector<double>& v) const {
        double sum = 0.0;
        for (double x : v) sum += x * x;
        return std::sqrt(sum);
    }

    double dot(const std::vector<double>& a, const std::vector<double>& b) const {
        double sum = 0.0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }

    double computeNovelty(const std::vector<double>& concept) const {
        if (knownConcepts_.empty()) return 1.0;
        double min_dist = 1e9;
        for (const auto& c : knownConcepts_) {
            if (c.size() != embeddingDim_) continue;
            double dist = 0.0;
            for (size_t i = 0; i < embeddingDim_; ++i) {
                double diff = concept[i] - c[i];
                dist += diff * diff;
            }
            dist = std::sqrt(dist);
            if (dist < min_dist) min_dist = dist;
        }
        return min_dist;
    }

    double computeUtility(const std::vector<double>& concept) const {
        double n_concept = norm(concept);
        double n_goal = norm(goalVector_);
        if (n_concept < 1e-12 || n_goal < 1e-12) return 0.0;
        double sim = dot(concept, goalVector_) / (n_concept * n_goal);
        return std::max(0.0, std::min(1.0, (sim + 1.0) / 2.0));
    }

    double computeCoherence(const std::vector<double>& concept) const {
        if (concept.empty()) return 1.0;
        double mean = 0.0;
        for (double x : concept) mean += x;
        mean /= concept.size();

        double var = 0.0;
        for (double x : concept) {
            double d = x - mean;
            var += d * d;
        }
        var /= concept.size();
        double stddev = std::sqrt(var);
        return 1.0 / (1.0 + stddev);
    }

    double computeValue(const std::vector<double>& concept) const {
        double nov = computeNovelty(concept);
        double util = computeUtility(concept);
        double coh = computeCoherence(concept);
        return nov * util * coh;
    }
};

CreativeSearch::CreativeSearch(size_t embeddingDim)
    : pImpl(std::make_unique<Impl>(embeddingDim)) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "CreativeSearch initialized with dim " + std::to_string(embeddingDim));
}

CreativeSearch::~CreativeSearch() = default;

CreativeSearch::CreativeSearch(CreativeSearch&&) noexcept = default;
CreativeSearch& CreativeSearch::operator=(CreativeSearch&&) noexcept = default;

void CreativeSearch::setKnownConcepts(const std::vector<std::vector<double>>& concepts) {
    pImpl->knownConcepts_.clear();
    for (const auto& c : concepts) {
        if (c.size() == pImpl->embeddingDim_) {
            pImpl->knownConcepts_.push_back(c);
        }
    }
}

void CreativeSearch::setGoalVector(const std::vector<double>& goal) {
    if (goal.size() == pImpl->embeddingDim_) {
        pImpl->goalVector_ = goal;
    }
}

void CreativeSearch::setRepulsionStrength(double lambda) {
    pImpl->lambda_ = lambda;
}

void CreativeSearch::setStepSize(double eta) {
    pImpl->eta_ = eta;
}

void CreativeSearch::setMaxIterations(size_t maxIter) {
    pImpl->maxIter_ = maxIter;
}

void CreativeSearch::setConvergenceThreshold(double threshold) {
    pImpl->threshold_ = threshold;
}

SearchResult CreativeSearch::search(const std::vector<double>& start,
                                    SearchMode mode,
                                    size_t numCandidates) {
    SearchResult bestResult;
    bestResult.value = -1.0;

    if (start.size() != pImpl->embeddingDim_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARNING, "CreativeSearch::search start vector dimension mismatch");
        return bestResult;
    }

    std::normal_distribution<double> noise(0.0, 1.0);

    for (size_t cand = 0; cand < numCandidates; ++cand) {
        std::vector<double> current = start;
        if (cand > 0) {
            for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
                current[i] += noise(pImpl->rng_) * 0.1;
            }
        }

        size_t iter = 0;
        double prevVal = pImpl->computeValue(current);

        for (iter = 0; iter < pImpl->maxIter_; ++iter) {
            std::vector<double> nextPos = current;

            if (mode == SearchMode::DIVERGENT) {
                // Stochastic perturbation + repulsion from known concepts
                for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
                    double g = noise(pImpl->rng_);
                    double repulsion = 0.0;
                    for (const auto& kc : pImpl->knownConcepts_) {
                        double diff = current[i] - kc[i];
                        repulsion += diff / (std::abs(diff) + 1e-6);
                    }
                    nextPos[i] += pImpl->eta_ * g + pImpl->lambda_ * repulsion;
                }
            } else { // CONVERGENT (Gradient ascent on value V(x))
                double eps = 1e-4;
                double currentVal = pImpl->computeValue(current);
                std::vector<double> grad(pImpl->embeddingDim_, 0.0);

                for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
                    std::vector<double> perturbed = current;
                    perturbed[i] += eps;
                    double valPlus = pImpl->computeValue(perturbed);
                    grad[i] = (valPlus - currentVal) / eps;
                }

                for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
                    nextPos[i] += pImpl->eta_ * grad[i];
                }
            }

            double newVal = pImpl->computeValue(nextPos);
            if (std::abs(newVal - prevVal) < pImpl->threshold_) {
                current = nextPos;
                break;
            }
            current = nextPos;
            prevVal = newVal;
        }

        double finalVal = pImpl->computeValue(current);
        if (finalVal > bestResult.value) {
            bestResult.conceptVector = current;
            bestResult.novelty = pImpl->computeNovelty(current);
            bestResult.utility = pImpl->computeUtility(current);
            bestResult.coherence = pImpl->computeCoherence(current);
            bestResult.value = finalVal;
            bestResult.iterations = iter + 1;
        }
    }

    return bestResult;
}

double CreativeSearch::evaluateValue(const std::vector<double>& concept) const {
    return pImpl->computeValue(concept);
}

double CreativeSearch::evaluateNovelty(const std::vector<double>& concept) const {
    return pImpl->computeNovelty(concept);
}

double CreativeSearch::evaluateUtility(const std::vector<double>& concept) const {
    return pImpl->computeUtility(concept);
}

double CreativeSearch::evaluateCoherence(const std::vector<double>& concept) const {
    return pImpl->computeCoherence(concept);
}

size_t CreativeSearch::getEmbeddingDim() const {
    return pImpl->embeddingDim_;
}

std::vector<uint8_t> CreativeSearch::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x43525343; // 'CRSC'
    uint32_t dim = static_cast<uint32_t>(pImpl->embeddingDim_);
    uint32_t knownCount = static_cast<uint32_t>(pImpl->knownConcepts_.size());

    buf.resize(20);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &dim, 4);
    std::memcpy(buf.data() + 8, &knownCount, 4);
    std::memcpy(buf.data() + 12, &pImpl->lambda_, 8);

    for (const auto& vec : pImpl->knownConcepts_) {
        size_t offset = buf.size();
        buf.resize(offset + dim * sizeof(double));
        std::memcpy(buf.data() + offset, vec.data(), dim * sizeof(double));
    }

    // FNV-1a 64-bit checksum
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t offset = buf.size();
    buf.resize(offset + 8);
    std::memcpy(buf.data() + offset, &hash, 8);

    return buf;
}

bool CreativeSearch::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 28) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0, dim = 0, knownCount = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x43525343) return false;

    std::memcpy(&dim, data.data() + 4, 4);
    std::memcpy(&knownCount, data.data() + 8, 4);
    std::memcpy(&pImpl->lambda_, data.data() + 12, 8);

    size_t required = 20 + knownCount * dim * sizeof(double) + 8;
    if (data.size() != required) return false;

    pImpl->embeddingDim_ = dim;
    pImpl->knownConcepts_.clear();
    pImpl->knownConcepts_.reserve(knownCount);

    size_t cursor = 20;
    for (size_t i = 0; i < knownCount; ++i) {
        std::vector<double> vec(dim);
        std::memcpy(vec.data(), data.data() + cursor, dim * sizeof(double));
        pImpl->knownConcepts_.push_back(vec);
        cursor += dim * sizeof(double);
    }

    return true;
}

}} // namespace yuki::creativity
