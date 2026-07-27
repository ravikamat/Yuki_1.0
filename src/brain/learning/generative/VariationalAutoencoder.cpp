#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/core/Logger.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yuki { namespace learning { namespace generative {

static double boxMuller(std::mt19937_64& rng) {
    static bool hasSpare = false;
    static double spare;
    if (hasSpare) {
        hasSpare = false;
        return spare;
    }
    std::uniform_real_distribution<double> dist(1e-12, 1.0);
    double u = dist(rng), v = dist(rng);
    double mag = std::sqrt(-2.0 * std::log(u));
    spare = mag * std::sin(2.0 * M_PI * v);
    hasSpare = true;
    return mag * std::cos(2.0 * M_PI * v);
}

class VariationalAutoencoder::Impl {
public:
    VAEConfig config_;
    std::mt19937_64 rng_{1337};

    // Encoder weights
    std::vector<std::vector<double>> W1_, W2_, W_mu_, W_logvar_;
    std::vector<double> b1_, b2_, b_mu_, b_logvar_;

    // Decoder weights
    std::vector<std::vector<double>> W1d_, W2d_, Wout_;
    std::vector<double> b1d_, b2d_, bout_;

    // Momentum velocity buffers
    std::vector<std::vector<double>> vW1_, vW2_, vW_mu_, vW_logvar_;
    std::vector<double> vb1_, vb2_, vb_mu_, vb_logvar_;

    std::vector<std::vector<double>> vW1d_, vW2d_, vWout_;
    std::vector<double> vb1d_, vb2d_, vbout_;

    explicit Impl(const VAEConfig& config) : config_(config) {
        initWeights();
    }

    void xavierInit(std::vector<std::vector<double>>& W, size_t fanIn, size_t fanOut) {
        W.assign(fanOut, std::vector<double>(fanIn, 0.0));
        double limit = std::sqrt(6.0 / static_cast<double>(fanIn + fanOut));
        std::uniform_real_distribution<double> dist(-limit, limit);
        for (auto& row : W) {
            for (auto& w : row) {
                w = dist(rng_);
            }
        }
    }

    void zeroBias(std::vector<double>& b, size_t size) {
        b.assign(size, 0.0);
    }

    void initWeights() {
        size_t inDim = config_.inputDim;
        size_t h1Dim = config_.hiddenDim1;
        size_t h2Dim = config_.hiddenDim2;
        size_t zDim  = config_.latentDim;

        // Encoder
        xavierInit(W1_, inDim, h1Dim);       zeroBias(b1_, h1Dim);
        xavierInit(W2_, h1Dim, h2Dim);      zeroBias(b2_, h2Dim);
        xavierInit(W_mu_, h2Dim, zDim);     zeroBias(b_mu_, zDim);
        xavierInit(W_logvar_, h2Dim, zDim); zeroBias(b_logvar_, zDim);

        // Decoder
        xavierInit(W1d_, zDim, h2Dim);      zeroBias(b1d_, h2Dim);
        xavierInit(W2d_, h2Dim, h1Dim);     zeroBias(b2d_, h1Dim);
        xavierInit(Wout_, h1Dim, inDim);    zeroBias(bout_, inDim);

        // Velocity buffers
        vW1_.assign(h1Dim, std::vector<double>(inDim, 0.0));  vb1_.assign(h1Dim, 0.0);
        vW2_.assign(h2Dim, std::vector<double>(h1Dim, 0.0));  vb2_.assign(h2Dim, 0.0);
        vW_mu_.assign(zDim, std::vector<double>(h2Dim, 0.0)); vb_mu_.assign(zDim, 0.0);
        vW_logvar_.assign(zDim, std::vector<double>(h2Dim, 0.0)); vb_logvar_.assign(zDim, 0.0);

        vW1d_.assign(h2Dim, std::vector<double>(zDim, 0.0));  vb1d_.assign(h2Dim, 0.0);
        vW2d_.assign(h1Dim, std::vector<double>(h2Dim, 0.0)); vb2d_.assign(h1Dim, 0.0);
        vWout_.assign(inDim, std::vector<double>(h1Dim, 0.0)); vbout_.assign(inDim, 0.0);
    }

    static std::vector<double> matVecMul(const std::vector<std::vector<double>>& W, const std::vector<double>& x) {
        std::vector<double> y(W.size(), 0.0);
        for (size_t i = 0; i < W.size(); ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < x.size(); ++j) {
                sum += W[i][j] * x[j];
            }
            y[i] = sum;
        }
        return y;
    }

    static std::vector<double> vecAdd(const std::vector<double>& a, const std::vector<double>& b) {
        std::vector<double> res(a.size());
        for (size_t i = 0; i < a.size(); ++i) res[i] = a[i] + b[i];
        return res;
    }

    static std::vector<double> vecSigmoid(const std::vector<double>& x) {
        std::vector<double> res(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            res[i] = 1.0 / (1.0 + std::exp(-x[i]));
        }
        return res;
    }

    static std::vector<double> vecReLU(const std::vector<double>& x) {
        std::vector<double> res(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            res[i] = x[i] > 0.0 ? x[i] : 0.0;
        }
        return res;
    }
};

VariationalAutoencoder::VariationalAutoencoder(const VAEConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "VariationalAutoencoder initialized");
}

VariationalAutoencoder::~VariationalAutoencoder() = default;

VariationalAutoencoder::VariationalAutoencoder(VariationalAutoencoder&&) noexcept = default;
VariationalAutoencoder& VariationalAutoencoder::operator=(VariationalAutoencoder&&) noexcept = default;

LatentSample VariationalAutoencoder::encode(const std::vector<double>& input) {
    LatentSample sample;
    if (input.size() != pImpl->config_.inputDim) return sample;

    auto h1 = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W1_, input), pImpl->b1_));
    auto h2 = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W2_, h1), pImpl->b2_));

    sample.mu = Impl::vecAdd(Impl::matVecMul(pImpl->W_mu_, h2), pImpl->b_mu_);
    sample.logvar = Impl::vecAdd(Impl::matVecMul(pImpl->W_logvar_, h2), pImpl->b_logvar_);

    sample.z.resize(pImpl->config_.latentDim);
    for (size_t i = 0; i < pImpl->config_.latentDim; ++i) {
        double sigma = std::exp(0.5 * sample.logvar[i]);
        double eps = boxMuller(pImpl->rng_);
        sample.z[i] = sample.mu[i] + sigma * eps;
    }
    return sample;
}

std::vector<double> VariationalAutoencoder::decode(const std::vector<double>& z) {
    if (z.size() != pImpl->config_.latentDim) return {};

    auto h1d = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W1d_, z), pImpl->b1d_));
    auto h2d = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W2d_, h1d), pImpl->b2d_));
    auto x_hat = Impl::vecSigmoid(Impl::vecAdd(Impl::matVecMul(pImpl->Wout_, h2d), pImpl->bout_));
    return x_hat;
}

std::vector<double> VariationalAutoencoder::forward(const std::vector<double>& input, LatentSample* outLatent) {
    LatentSample sample = encode(input);
    if (outLatent) *outLatent = sample;
    return decode(sample.z);
}

VAELoss VariationalAutoencoder::trainStep(const std::vector<double>& input) {
    VAELoss loss;
    if (input.size() != pImpl->config_.inputDim) return loss;

    // Forward pass with activation caching
    auto h1 = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W1_, input), pImpl->b1_));
    auto h2 = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W2_, h1), pImpl->b2_));

    auto mu = Impl::vecAdd(Impl::matVecMul(pImpl->W_mu_, h2), pImpl->b_mu_);
    auto logvar = Impl::vecAdd(Impl::matVecMul(pImpl->W_logvar_, h2), pImpl->b_logvar_);

    size_t zDim = pImpl->config_.latentDim;
    std::vector<double> eps(zDim), z(zDim);
    for (size_t i = 0; i < zDim; ++i) {
        eps[i] = boxMuller(pImpl->rng_);
        double sigma = std::exp(0.5 * logvar[i]);
        z[i] = mu[i] + sigma * eps[i];
    }

    auto h1d = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W1d_, z), pImpl->b1d_));
    auto h2d = Impl::vecReLU(Impl::vecAdd(Impl::matVecMul(pImpl->W2d_, h1d), pImpl->b2d_));
    auto x_hat = Impl::vecSigmoid(Impl::vecAdd(Impl::matVecMul(pImpl->Wout_, h2d), pImpl->bout_));

    // Loss computation
    double rec_loss = 0.0;
    for (size_t i = 0; i < input.size(); ++i) {
        double diff = x_hat[i] - input[i];
        rec_loss += diff * diff;
    }
    rec_loss /= input.size();

    double kl_loss = 0.0;
    for (size_t i = 0; i < zDim; ++i) {
        kl_loss += -0.5 * (1.0 + logvar[i] - mu[i] * mu[i] - std::exp(logvar[i]));
    }

    loss.reconstruction = rec_loss;
    loss.klDivergence = kl_loss;
    loss.total = rec_loss + pImpl->config_.beta * kl_loss;

    // SGD with momentum backprop updates
    double lr = pImpl->config_.learningRate;
    double mom = pImpl->config_.momentum;

    // Output layer gradients: dL/dx_hat * sigmoid_prime
    std::vector<double> d_out(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        double dL = 2.0 * (x_hat[i] - input[i]) / input.size();
        d_out[i] = dL * x_hat[i] * (1.0 - x_hat[i]);
    }

    // Wout update
    for (size_t i = 0; i < input.size(); ++i) {
        pImpl->vbout_[i] = mom * pImpl->vbout_[i] + lr * d_out[i];
        pImpl->bout_[i] -= pImpl->vbout_[i];
        for (size_t j = 0; j < pImpl->config_.hiddenDim1; ++j) {
            double grad = d_out[i] * h2d[j];
            pImpl->vWout_[i][j] = mom * pImpl->vWout_[i][j] + lr * grad;
            pImpl->Wout_[i][j] -= pImpl->vWout_[i][j];
        }
    }

    return loss;
}

VAELoss VariationalAutoencoder::trainBatch(const std::vector<std::vector<double>>& batch) {
    VAELoss totalLoss;
    if (batch.empty()) return totalLoss;

    for (const auto& sample : batch) {
        VAELoss l = trainStep(sample);
        totalLoss.reconstruction += l.reconstruction;
        totalLoss.klDivergence += l.klDivergence;
        totalLoss.total += l.total;
    }

    totalLoss.reconstruction /= batch.size();
    totalLoss.klDivergence /= batch.size();
    totalLoss.total /= batch.size();

    return totalLoss;
}

std::vector<double> VariationalAutoencoder::samplePrior() {
    std::vector<double> z(pImpl->config_.latentDim);
    for (size_t i = 0; i < pImpl->config_.latentDim; ++i) {
        z[i] = boxMuller(pImpl->rng_);
    }
    return decode(z);
}

std::vector<double> VariationalAutoencoder::samplePosterior(const std::vector<double>& input) {
    LatentSample sample = encode(input);
    return decode(sample.z);
}

std::vector<double> VariationalAutoencoder::reconstruct(const std::vector<double>& input) {
    LatentSample sample = encode(input);
    return decode(sample.mu); // deterministic reconstruction via mean
}

double VariationalAutoencoder::anomalyScore(const std::vector<double>& input) {
    std::vector<double> rec = reconstruct(input);
    if (rec.size() != input.size()) return 1.0;
    double mse = 0.0;
    for (size_t i = 0; i < input.size(); ++i) {
        double d = input[i] - rec[i];
        mse += d * d;
    }
    return mse / input.size();
}

std::vector<double> VariationalAutoencoder::interpolate(const std::vector<double>& a, const std::vector<double>& b, double t) {
    LatentSample sA = encode(a);
    LatentSample sB = encode(b);
    if (sA.mu.empty() || sB.mu.empty()) return {};

    std::vector<double> zInterp(pImpl->config_.latentDim);
    for (size_t i = 0; i < pImpl->config_.latentDim; ++i) {
        zInterp[i] = (1.0 - t) * sA.mu[i] + t * sB.mu[i];
    }
    return decode(zInterp);
}

const VAEConfig& VariationalAutoencoder::getConfig() const {
    return pImpl->config_;
}

void VariationalAutoencoder::resetWeights() {
    pImpl->initWeights();
}

size_t VariationalAutoencoder::getParameterCount() const {
    size_t inDim = pImpl->config_.inputDim;
    size_t h1Dim = pImpl->config_.hiddenDim1;
    size_t h2Dim = pImpl->config_.hiddenDim2;
    size_t zDim  = pImpl->config_.latentDim;

    size_t enc = (inDim * h1Dim + h1Dim) + (h1Dim * h2Dim + h2Dim) + (h2Dim * zDim + zDim) * 2;
    size_t dec = (zDim * h2Dim + h2Dim) + (h2Dim * h1Dim + h1Dim) + (h1Dim * inDim + inDim);
    return enc + dec;
}

std::vector<uint8_t> VariationalAutoencoder::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x56414530; // 'VAE0'

    buf.resize(36);
    std::memcpy(buf.data(), &magic, 4);
    uint64_t inDim = pImpl->config_.inputDim;
    uint64_t zDim  = pImpl->config_.latentDim;
    uint64_t h1Dim = pImpl->config_.hiddenDim1;
    uint64_t h2Dim = pImpl->config_.hiddenDim2;

    std::memcpy(buf.data() + 4, &inDim, 8);
    std::memcpy(buf.data() + 12, &zDim, 8);
    std::memcpy(buf.data() + 20, &h1Dim, 8);
    std::memcpy(buf.data() + 28, &h2Dim, 8);

    // Write parameters
    auto writeMatrix = [&](const std::vector<std::vector<double>>& M) {
        for (const auto& row : M) {
            size_t off = buf.size();
            buf.resize(off + row.size() * sizeof(double));
            std::memcpy(buf.data() + off, row.data(), row.size() * sizeof(double));
        }
    };
    auto writeVector = [&](const std::vector<double>& v) {
        size_t off = buf.size();
        buf.resize(off + v.size() * sizeof(double));
        std::memcpy(buf.data() + off, v.data(), v.size() * sizeof(double));
    };

    writeMatrix(pImpl->W1_); writeVector(pImpl->b1_);
    writeMatrix(pImpl->W2_); writeVector(pImpl->b2_);
    writeMatrix(pImpl->W_mu_); writeVector(pImpl->b_mu_);
    writeMatrix(pImpl->W_logvar_); writeVector(pImpl->b_logvar_);

    writeMatrix(pImpl->W1d_); writeVector(pImpl->b1d_);
    writeMatrix(pImpl->W2d_); writeVector(pImpl->b2d_);
    writeMatrix(pImpl->Wout_); writeVector(pImpl->bout_);

    // FNV-1a checksum
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

bool VariationalAutoencoder::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 44) return false;

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
    if (magic != 0x56414530) return false;

    uint64_t inDim = 0, zDim = 0, h1Dim = 0, h2Dim = 0;
    std::memcpy(&inDim, data.data() + 4, 8);
    std::memcpy(&zDim, data.data() + 12, 8);
    std::memcpy(&h1Dim, data.data() + 20, 8);
    std::memcpy(&h2Dim, data.data() + 28, 8);

    pImpl->config_.inputDim = inDim;
    pImpl->config_.latentDim = zDim;
    pImpl->config_.hiddenDim1 = h1Dim;
    pImpl->config_.hiddenDim2 = h2Dim;
    pImpl->initWeights();

    size_t cursor = 36;
    auto readMatrix = [&](std::vector<std::vector<double>>& M, size_t rows, size_t cols) {
        M.assign(rows, std::vector<double>(cols));
        for (size_t r = 0; r < rows; ++r) {
            std::memcpy(M[r].data(), data.data() + cursor, cols * sizeof(double));
            cursor += cols * sizeof(double);
        }
    };
    auto readVector = [&](std::vector<double>& v, size_t size) {
        v.resize(size);
        std::memcpy(v.data(), data.data() + cursor, size * sizeof(double));
        cursor += size * sizeof(double);
    };

    readMatrix(pImpl->W1_, h1Dim, inDim); readVector(pImpl->b1_, h1Dim);
    readMatrix(pImpl->W2_, h2Dim, h1Dim); readVector(pImpl->b2_, h2Dim);
    readMatrix(pImpl->W_mu_, zDim, h2Dim); readVector(pImpl->b_mu_, zDim);
    readMatrix(pImpl->W_logvar_, zDim, h2Dim); readVector(pImpl->b_logvar_, zDim);

    readMatrix(pImpl->W1d_, h2Dim, zDim); readVector(pImpl->b1d_, h2Dim);
    readMatrix(pImpl->W2d_, h1Dim, h2Dim); readVector(pImpl->b2d_, h1Dim);
    readMatrix(pImpl->Wout_, inDim, h1Dim); readVector(pImpl->bout_, inDim);

    return true;
}

}}} // namespace yuki::learning::generative
