#pragma once
#include <vector>
#include <cstdint>
#include <memory>

namespace yuki { namespace learning { namespace generative {

struct VAEConfig {
    size_t inputDim = 128;
    size_t latentDim = 32;
    size_t hiddenDim1 = 256;
    size_t hiddenDim2 = 128;
    double learningRate = 1e-3;
    double beta = 1.0;
    double momentum = 0.9;
};

struct VAELoss {
    double reconstruction = 0.0;
    double klDivergence = 0.0;
    double total = 0.0;
};

struct LatentSample {
    std::vector<double> z;
    std::vector<double> mu;
    std::vector<double> logvar;
};

class VariationalAutoencoder {
public:
    explicit VariationalAutoencoder(const VAEConfig& config = VAEConfig{});
    ~VariationalAutoencoder();
    VariationalAutoencoder(const VariationalAutoencoder&) = delete;
    VariationalAutoencoder& operator=(const VariationalAutoencoder&) = delete;
    VariationalAutoencoder(VariationalAutoencoder&&) noexcept;
    VariationalAutoencoder& operator=(VariationalAutoencoder&&) noexcept;

    std::vector<double> forward(const std::vector<double>& input, LatentSample* outLatent = nullptr);
    LatentSample encode(const std::vector<double>& input);
    std::vector<double> decode(const std::vector<double>& z);
    VAELoss trainStep(const std::vector<double>& input);
    VAELoss trainBatch(const std::vector<std::vector<double>>& batch);
    std::vector<double> samplePrior();
    std::vector<double> samplePosterior(const std::vector<double>& input);
    std::vector<double> reconstruct(const std::vector<double>& input);
    double anomalyScore(const std::vector<double>& input);
    std::vector<double> interpolate(const std::vector<double>& a, const std::vector<double>& b, double t);

    const VAEConfig& getConfig() const;

    // Binary serialization: magic = 0x56414530 ('VAE0')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    void resetWeights();
    size_t getParameterCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}}} // namespace yuki::learning::generative
