#include "brain/sleep/DreamEngine.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/causal/CounterfactualSimulator.h"
#include "brain/organism/DriveSystem.h"
#include "brain/core/Logger.h"

#include <random>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace yuki { namespace sleep {

class DreamEngine::Impl {
public:
    yuki::learning::generative::VariationalAutoencoder* vae_ = nullptr;
    yuki::memory::MemoryFabric* fabric_ = nullptr;
    yuki::causal::CounterfactualSimulator* sim_ = nullptr;
    std::vector<yuki::organism::DriveGoal> goals_;
    DreamConfig config_;
    size_t totalDreams_ = 0;
    std::mt19937_64 rng_{2026};

    Impl() = default;
};

DreamEngine::DreamEngine() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "DreamEngine initialized");
}

DreamEngine::~DreamEngine() = default;

DreamEngine::DreamEngine(DreamEngine&&) noexcept = default;
DreamEngine& DreamEngine::operator=(DreamEngine&&) noexcept = default;

void DreamEngine::setVAE(yuki::learning::generative::VariationalAutoencoder* vae) {
    pImpl->vae_ = vae;
}

void DreamEngine::setMemoryFabric(yuki::memory::MemoryFabric* fabric) {
    pImpl->fabric_ = fabric;
}

void DreamEngine::setCounterfactualSimulator(yuki::causal::CounterfactualSimulator* sim) {
    pImpl->sim_ = sim;
}

void DreamEngine::setDriveGoals(const std::vector<yuki::organism::DriveGoal>& goals) {
    pImpl->goals_ = goals;
}

void DreamEngine::setConfig(const DreamConfig& config) {
    pImpl->config_ = config;
}

std::vector<double> DreamEngine::sampleDirichlet(size_t k, double alpha) {
    if (k == 0) return {};
    std::gamma_distribution<double> dist(alpha, 1.0);
    std::vector<double> samples(k);
    double sum = 0.0;
    for (size_t i = 0; i < k; ++i) {
        samples[i] = dist(pImpl->rng_);
        sum += samples[i];
    }
    if (sum > 1e-12) {
        for (size_t i = 0; i < k; ++i) samples[i] /= sum;
    }
    return samples;
}

std::vector<size_t> DreamEngine::sampleMemoryIndices(size_t count, size_t total) {
    std::vector<size_t> indices;
    if (total == 0 || count == 0) return indices;
    count = std::min(count, total);

    std::vector<size_t> pool(total);
    for (size_t i = 0; i < total; ++i) pool[i] = i;

    std::shuffle(pool.begin(), pool.end(), pImpl->rng_);
    indices.assign(pool.begin(), pool.begin() + count);
    return indices;
}

DreamEpisode DreamEngine::generateBlendDream() {
    DreamEpisode ep;
    ep.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (!pImpl->vae_) {
        // Fallback synthetic feature vector when VAE is unavailable
        size_t dim = 128;
        ep.features.resize(dim, 0.5);
        pImpl->totalDreams_++;
        return ep;
    }

    size_t dim = pImpl->vae_->getConfig().inputDim;
    size_t zDim = pImpl->vae_->getConfig().latentDim;

    size_t numBlend = pImpl->config_.minBlendMemories;
    auto weights = sampleDirichlet(numBlend, 1.0);

    // Latent interpolation
    std::vector<double> zBlended(zDim, 0.0);
    std::uniform_real_distribution<double> dist(-pImpl->config_.latentPerturbationScale, pImpl->config_.latentPerturbationScale);

    for (size_t i = 0; i < zDim; ++i) {
        zBlended[i] = dist(pImpl->rng_);
    }

    ep.features = pImpl->vae_->decode(zBlended);
    if (ep.features.empty()) ep.features.resize(dim, 0.0);

    ep.noveltyScore = pImpl->vae_->anomalyScore(ep.features);
    ep.isCounterfactual = false;
    pImpl->totalDreams_++;
    return ep;
}

DreamEpisode DreamEngine::generateCounterfactualDream(uint64_t memoryId, const std::vector<double>& goalDirection) {
    DreamEpisode ep;
    ep.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ep.sourceMemoryIds.push_back(memoryId);
    ep.isCounterfactual = true;

    if (pImpl->vae_) {
        size_t dim = pImpl->vae_->getConfig().inputDim;
        std::vector<double> base = pImpl->vae_->samplePrior();
        if (base.size() == goalDirection.size()) {
            for (size_t i = 0; i < base.size(); ++i) {
                base[i] = 0.7 * base[i] + 0.3 * goalDirection[i];
            }
        }
        ep.features = base;
        ep.noveltyScore = pImpl->vae_->anomalyScore(base);
    } else {
        ep.features = goalDirection;
        ep.noveltyScore = 0.5;
    }

    pImpl->totalDreams_++;
    return ep;
}

std::vector<DreamEpisode> DreamEngine::generateDreamCycle() {
    std::vector<DreamEpisode> cycle;
    cycle.reserve(pImpl->config_.dreamsPerCycle);

    for (size_t i = 0; i < pImpl->config_.dreamsPerCycle; ++i) {
        if (pImpl->config_.enableCounterfactualDreams && (i % 2 == 1)) {
            std::vector<double> goalDir = {0.1, 0.2, 0.3};
            cycle.push_back(generateCounterfactualDream(i + 1, goalDir));
        } else {
            cycle.push_back(generateBlendDream());
        }
    }
    return cycle;
}

size_t DreamEngine::trainVAEDreamBatch(size_t batchSize) {
    if (!pImpl->vae_ || batchSize == 0) return 0;

    size_t dim = pImpl->vae_->getConfig().inputDim;
    std::vector<std::vector<double>> batch;
    batch.reserve(batchSize);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (size_t b = 0; b < batchSize; ++b) {
        std::vector<double> sample(dim);
        for (size_t i = 0; i < dim; ++i) sample[i] = dist(pImpl->rng_);
        batch.push_back(sample);
    }

    pImpl->vae_->trainBatch(batch);
    return batchSize;
}

size_t DreamEngine::getTotalDreamsGenerated() const {
    return pImpl->totalDreams_;
}

std::vector<uint8_t> DreamEngine::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x4452454D; // 'DREM'
    uint64_t count = pImpl->totalDreams_;

    buf.resize(20);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &count, 8);
    std::memcpy(buf.data() + 12, &pImpl->config_.dreamsPerCycle, 8);

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool DreamEngine::deserialize(const std::vector<uint8_t>& data) {
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

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x4452454D) return false;

    uint64_t count = 0, perCycle = 0;
    std::memcpy(&count, data.data() + 4, 8);
    std::memcpy(&perCycle, data.data() + 12, 8);

    pImpl->totalDreams_ = count;
    pImpl->config_.dreamsPerCycle = perCycle;

    return true;
}

}} // namespace yuki::sleep
