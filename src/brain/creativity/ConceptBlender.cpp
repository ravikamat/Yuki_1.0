#include "brain/creativity/ConceptBlender.h"
#include "brain/core/Logger.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace yuki { namespace creativity {

class ConceptBlender::Impl {
public:
    size_t embeddingDim_;
    std::vector<std::vector<double>> library_;

    explicit Impl(size_t dim) : embeddingDim_(dim) {}

    double computeEuclideanDistance(const std::vector<double>& u, const std::vector<double>& v) const {
        if (u.size() != v.size()) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < u.size(); ++i) {
            double d = u[i] - v[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }

    double computeNovelty(const std::vector<double>& c) const {
        if (library_.empty()) return 1.0;
        double min_dist = computeEuclideanDistance(c, library_[0]);
        for (size_t i = 1; i < library_.size(); ++i) {
            double dist = computeEuclideanDistance(c, library_[i]);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
        return min_dist;
    }

    double computeDivergence(const std::vector<double>& c) const {
        if (c.empty()) return 0.0;
        double max_val = *std::max_element(c.begin(), c.end());
        double sum_exp = 0.0;
        std::vector<double> exps(c.size());
        for (size_t i = 0; i < c.size(); ++i) {
            exps[i] = std::exp(c[i] - max_val);
            sum_exp += exps[i];
        }
        double entropy = 0.0;
        for (size_t i = 0; i < c.size(); ++i) {
            double p = exps[i] / (sum_exp + 1e-12);
            if (p > 1e-12) {
                entropy -= p * std::log(p);
            }
        }
        return entropy;
    }
};

ConceptBlender::ConceptBlender(size_t embeddingDim)
    : pImpl(std::make_unique<Impl>(embeddingDim)) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "ConceptBlender initialized with dim " + std::to_string(embeddingDim));
}

ConceptBlender::~ConceptBlender() = default;

ConceptBlender::ConceptBlender(ConceptBlender&&) noexcept = default;
ConceptBlender& ConceptBlender::operator=(ConceptBlender&&) noexcept = default;

void ConceptBlender::setConceptLibrary(const std::vector<std::vector<double>>& library) {
    pImpl->library_.clear();
    for (const auto& vec : library) {
        if (vec.size() == pImpl->embeddingDim_) {
            pImpl->library_.push_back(vec);
        }
    }
}

void ConceptBlender::clearConceptLibrary() {
    pImpl->library_.clear();
}

BlendResult ConceptBlender::blend(const std::vector<double>& a,
                                  const std::vector<double>& b,
                                  BlendMode mode,
                                  double alpha) {
    BlendResult res;
    res.mode = mode;
    res.alpha = alpha;

    if (a.size() != pImpl->embeddingDim_ || b.size() != pImpl->embeddingDim_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "ConceptBlender::blend dimension mismatch");

        res.blendVector.resize(pImpl->embeddingDim_, 0.0);
        return res;
    }

    res.blendVector.resize(pImpl->embeddingDim_);

    if (mode == BlendMode::CONVEX) {
        for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
            res.blendVector[i] = alpha * a[i] + (1.0 - alpha) * b[i];
        }
    } else { // MULTIPLICATIVE
        double norm = 0.0;
        for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
            res.blendVector[i] = a[i] * b[i];
            norm += res.blendVector[i] * res.blendVector[i];
        }
        norm = std::sqrt(norm);
        if (norm > 1e-12) {
            for (size_t i = 0; i < pImpl->embeddingDim_; ++i) {
                res.blendVector[i] /= norm;
            }
        }
    }

    res.novelty = pImpl->computeNovelty(res.blendVector);
    res.divergence = pImpl->computeDivergence(res.blendVector);
    return res;
}

std::vector<BlendResult> ConceptBlender::blendSeries(const std::vector<double>& a,
                                                     const std::vector<double>& b,
                                                     BlendMode mode,
                                                     size_t numSteps) {
    std::vector<BlendResult> results;
    if (numSteps == 0) return results;
    results.reserve(numSteps);

    if (numSteps == 1) {
        results.push_back(blend(a, b, mode, 0.5));
        return results;
    }

    for (size_t step = 0; step < numSteps; ++step) {
        double alpha = static_cast<double>(step) / static_cast<double>(numSteps - 1);
        results.push_back(blend(a, b, mode, alpha));
    }
    return results;
}

BlendResult ConceptBlender::selectMostNovel(const std::vector<BlendResult>& blends) {
    if (blends.empty()) return BlendResult{};
    auto it = std::max_element(blends.begin(), blends.end(),
                               [](const BlendResult& x, const BlendResult& y) {
                                   return x.novelty < y.novelty;
                               });
    return *it;
}

BlendResult ConceptBlender::selectMostDivergent(const std::vector<BlendResult>& blends) {
    if (blends.empty()) return BlendResult{};
    auto it = std::max_element(blends.begin(), blends.end(),
                               [](const BlendResult& x, const BlendResult& y) {
                                   return x.divergence < y.divergence;
                               });
    return *it;
}

size_t ConceptBlender::getEmbeddingDim() const {
    return pImpl->embeddingDim_;
}

size_t ConceptBlender::getLibrarySize() const {
    return pImpl->library_.size();
}

std::vector<uint8_t> ConceptBlender::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x43424C44; // 'CBLD'
    uint32_t dim = static_cast<uint32_t>(pImpl->embeddingDim_);
    uint32_t libSize = static_cast<uint32_t>(pImpl->library_.size());

    buf.resize(12);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &dim, 4);
    std::memcpy(buf.data() + 8, &libSize, 4);

    for (const auto& vec : pImpl->library_) {
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

bool ConceptBlender::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 20) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0;
    uint32_t dim = 0;
    uint32_t libSize = 0;

    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x43424C44) return false;

    std::memcpy(&dim, data.data() + 4, 4);
    std::memcpy(&libSize, data.data() + 8, 4);

    size_t required = 12 + libSize * dim * sizeof(double) + 8;
    if (data.size() != required) return false;

    pImpl->embeddingDim_ = dim;
    pImpl->library_.clear();
    pImpl->library_.reserve(libSize);

    size_t cursor = 12;
    for (size_t i = 0; i < libSize; ++i) {
        std::vector<double> vec(dim);
        std::memcpy(vec.data(), data.data() + cursor, dim * sizeof(double));
        pImpl->library_.push_back(vec);
        cursor += dim * sizeof(double);
    }

    return true;
}

}} // namespace yuki::creativity
