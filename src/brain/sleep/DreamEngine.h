#pragma once
#include <vector>
#include <cstdint>
#include <memory>

namespace yuki {
namespace learning { namespace generative { class VariationalAutoencoder; } }
namespace memory { class MemoryFabric; class EpisodicStore; }
namespace causal { class CounterfactualSimulator; }
namespace organism { struct DriveGoal; }

namespace sleep {

struct DreamConfig {
    size_t minBlendMemories = 2;
    size_t maxBlendMemories = 4;
    double latentPerturbationScale = 0.1;
    size_t dreamsPerCycle = 10;
    bool enableCounterfactualDreams = true;
};

struct DreamEpisode {
    std::vector<double> features;
    std::vector<uint64_t> sourceMemoryIds;
    bool isCounterfactual = false;
    double noveltyScore = 0.0;
    uint64_t timestamp = 0;
};

class DreamEngine {
public:
    DreamEngine();
    ~DreamEngine();
    DreamEngine(const DreamEngine&) = delete;
    DreamEngine& operator=(const DreamEngine&) = delete;
    DreamEngine(DreamEngine&&) noexcept;
    DreamEngine& operator=(DreamEngine&&) noexcept;

    void setVAE(yuki::learning::generative::VariationalAutoencoder* vae);
    void setMemoryFabric(yuki::memory::MemoryFabric* fabric);
    void setCounterfactualSimulator(yuki::causal::CounterfactualSimulator* sim);
    void setDriveGoals(const std::vector<yuki::organism::DriveGoal>& goals);
    void setConfig(const DreamConfig& config);

    std::vector<DreamEpisode> generateDreamCycle();
    DreamEpisode generateBlendDream();
    DreamEpisode generateCounterfactualDream(uint64_t memoryId, const std::vector<double>& goalDirection);
    size_t trainVAEDreamBatch(size_t batchSize);

    // Binary serialization: magic = 0x4452454D ('DREM')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    size_t getTotalDreamsGenerated() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    std::vector<double> sampleDirichlet(size_t k, double alpha);
    std::vector<size_t> sampleMemoryIndices(size_t count, size_t total);
};

} // namespace sleep
} // namespace yuki
