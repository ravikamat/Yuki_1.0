#include "brain/memory/SparseHDC.h"
#include "brain/core/Logger.h"
#include <numeric>
#include <sstream>

namespace yuki {
namespace memory {

SparseHypervector::SparseHypervector(size_t dim, float sparsity, std::mt19937& gen) {
    size_t n_active = static_cast<size_t>(dim * sparsity);
    std::uniform_int_distribution<size_t> dist(0, dim - 1);
    std::uniform_int_distribution<int> sign_dist(0, 1);
    
    std::unordered_set<size_t> positions;
    while (positions.size() < n_active) {
        positions.insert(dist(gen));
    }
    
    active_pos.reserve(n_active);
    active_val.reserve(n_active);
    
    for (size_t pos : positions) {
        active_pos.push_back(static_cast<uint16_t>(pos));
        active_val.push_back(sign_dist(gen) ? 1 : -1);
    }
    
    std::sort(active_pos.begin(), active_pos.end());
}

SparseHypervector SparseHypervector::bind(const SparseHypervector& other) const {
    // XOR binding for sparse: positions where exactly one is active
    SparseHypervector result;
    result.active_pos.reserve(active_pos.size() + other.active_pos.size());
    result.active_val.reserve(active_val.size() + other.active_val.size());

    size_t i = 0, j = 0;
    while (i < active_pos.size() && j < other.active_pos.size()) {
        if (active_pos[i] < other.active_pos[j]) {
            result.active_pos.push_back(active_pos[i]);
            result.active_val.push_back(active_val[i]);
            ++i;
        } else if (active_pos[i] > other.active_pos[j]) {
            result.active_pos.push_back(other.active_pos[j]);
            result.active_val.push_back(other.active_val[j]);
            ++j;
        } else {
            // Same position: XOR = multiply ternary values
            int8_t combined = active_val[i] * other.active_val[j];
            if (combined != 0) {
                result.active_pos.push_back(active_pos[i]);
                result.active_val.push_back(combined);
            }
            ++i; ++j;
        }
    }
    while (i < active_pos.size()) {
        result.active_pos.push_back(active_pos[i]);
        result.active_val.push_back(active_val[i]);
        ++i;
    }
    while (j < other.active_pos.size()) {
        result.active_pos.push_back(other.active_pos[j]);
        result.active_val.push_back(other.active_val[j]);
        ++j;
    }
    return result;
}

SparseHypervector SparseHypervector::bundle(const SparseHypervector& other) const {
    // Majority vote bundling (sparse)
    SparseHypervector result;
    
    // Combine active positions
    std::unordered_map<uint16_t, int> vote_counts;
    
    for (size_t i = 0; i < active_pos.size(); ++i) {
        vote_counts[active_pos[i]] += active_val[i];
    }
    for (size_t i = 0; i < other.active_pos.size(); ++i) {
        vote_counts[other.active_pos[i]] += other.active_val[i];
    }
    
    result.active_pos.reserve(vote_counts.size());
    result.active_val.reserve(vote_counts.size());
    
    for (const auto& [pos, vote] : vote_counts) {
        if (vote > 0) {
            result.active_pos.push_back(pos);
            result.active_val.push_back(1);
        } else if (vote < 0) {
            result.active_pos.push_back(pos);
            result.active_val.push_back(-1);
        }
        // vote == 0 -> not active (tie goes to 0)
    }
    
    std::sort(result.active_pos.begin(), result.active_pos.end());
    return result;
}

SparseHypervector SparseHypervector::permute(size_t shifts) const {
    SparseHypervector result;
    result.active_pos.reserve(active_pos.size());
    result.active_val.reserve(active_val.size());
    
    for (size_t i = 0; i < active_pos.size(); ++i) {
        size_t new_pos = (active_pos[i] + shifts) % 10000;
        result.active_pos.push_back(static_cast<uint16_t>(new_pos));
        result.active_val.push_back(active_val[i]);
    }
    std::sort(result.active_pos.begin(), result.active_pos.end());
    return result;
}

float SparseHypervector::cosineSimilarity(const SparseHypervector& other) const {
    // Cosine similarity = (A·B) / (|A| |B|)
    // For sparse ternary: dot product = sum of matching signs
    
    float dot = 0.0f;
    size_t i = 0, j = 0;
    
    while (i < active_pos.size() && j < other.active_pos.size()) {
        if (active_pos[i] < other.active_pos[j]) {
            ++i;
        } else if (active_pos[i] > other.active_pos[j]) {
            ++j;
        } else {
            // Matching position
            dot += static_cast<float>(active_val[i] * other.active_val[j]);
            ++i; ++j;
        }
    }
    
    float norm_a = std::sqrt(static_cast<float>(active_pos.size()));
    float norm_b = std::sqrt(static_cast<float>(other.active_pos.size()));
    
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (norm_a * norm_b);
}

size_t SparseHypervector::hammingDistance(const SparseHypervector& other) const {
    // Hamming distance for ternary sparse vectors
    size_t dist = 0;
    size_t i = 0, j = 0;
    
    while (i < active_pos.size() && j < other.active_pos.size()) {
        if (active_pos[i] < other.active_pos[j]) {
            ++dist; ++i;
        } else if (active_pos[i] > other.active_pos[j]) {
            ++dist; ++j;
        } else {
            if (active_val[i] != other.active_val[j]) ++dist;
            ++i; ++j;
        }
    }
    dist += (active_pos.size() - i) + (other.active_pos.size() - j);
    return dist;
}

SparseHypervector SparseHypervector::fromDense(const class Hypervector& dense) {
    SparseHypervector sparse;
    // This would require access to Hypervector's internals
    // For now, return empty - would be implemented when integrating
    return sparse;
}

std::string SparseHypervector::serialize() const {
    std::ostringstream oss;
    oss << active_pos.size() << "|";
    for (size_t i = 0; i < active_pos.size(); ++i) {
        oss << active_pos[i] << ":" << (active_val[i] > 0 ? "+" : "-");
        if (i + 1 < active_pos.size()) oss << ",";
    }
    return oss.str();
}

SparseHypervector SparseHypervector::deserialize(const std::string& data, size_t dim) {
    SparseHypervector hv;
    size_t pipe = data.find('|');
    if (pipe == std::string::npos) return hv;
    
    size_t count = std::stoul(data.substr(0, pipe));
    std::string pairs = data.substr(pipe + 1);
    
    hv.active_pos.reserve(count);
    hv.active_val.reserve(count);
    
    std::istringstream iss(pairs);
    std::string pair;
    while (std::getline(iss, pair, ',')) {
        size_t colon = pair.find(':');
        if (colon == std::string::npos) continue;
        
        uint16_t pos = static_cast<uint16_t>(std::stoul(pair.substr(0, colon)));
        int8_t val = (pair[colon + 1] == '+') ? 1 : -1;
        
        hv.active_pos.push_back(pos);
        hv.active_val.push_back(val);
    }
    return hv;
}

void SparseHypervector::enforceSparsity(size_t max_active) {
    if (active_pos.size() <= max_active) return;
    
    // Keep strongest activations (by absolute value, but all are ±1)
    // For now, randomly subsample
    std::mt19937 gen(42);
    std::shuffle(active_pos.begin(), active_pos.end(), gen);
    std::shuffle(active_val.begin(), active_val.end(), gen);
    
    active_pos.resize(max_active);
    active_val.resize(max_active);
    std::sort(active_pos.begin(), active_pos.end());
}

// SparseHDC implementation
SparseHDC::SparseHDC(const SparseHDCConfig& cfg) : config_(cfg), rng_(std::random_device{}()) {}

SparseHypervector SparseHDC::random(std::mt19937& gen) const {
    return SparseHypervector(config_.dim, config_.sparsity, gen);
}

SparseHypervector SparseHDC::zero() const {
    return SparseHypervector();
}

SparseHypervector SparseHDC::one() const {
    SparseHypervector hv;
    hv.active_pos.resize(config_.active_bits);
    hv.active_val.resize(config_.active_bits, 1);
    for (size_t i = 0; i < config_.active_bits; ++i) {
        hv.active_pos[i] = static_cast<uint16_t>(i);
    }
    return hv;
}

SparseHypervector SparseHDC::fromText(const std::string& text) const {
    // Use text hash for deterministic sparse vector
    std::hash<std::string> hasher;
    uint64_t seed = hasher(text);
    return fromSeed(seed);
}

SparseHypervector SparseHDC::fromSeed(uint64_t seed) const {
    std::mt19937 gen(seed);
    return random(gen);
}

SparseHypervector SparseHDC::bind(const SparseHypervector& a, const SparseHypervector& b) const {
    return a.bind(b);
}

SparseHypervector SparseHDC::bundle(const SparseHypervector& a, const SparseHypervector& b) const {
    return a.bundle(b);
}

SparseHypervector SparseHDC::permute(const SparseHypervector& a, size_t shifts) const {
    return a.permute(shifts);
}

float SparseHDC::cosineSimilarity(const SparseHypervector& a, const SparseHypervector& b) const {
    return a.cosineSimilarity(b);
}

size_t SparseHDC::hammingDistance(const SparseHypervector& a, const SparseHypervector& b) const {
    return a.hammingDistance(b);
}

void SparseHDC::cleanup(SparseHypervector& hv) const {
    hv.enforceSparsity(config_.active_bits);
}

std::vector<uint16_t> SparseHDC::samplePositions(size_t count, std::mt19937& gen) const {
    std::vector<uint16_t> positions;
    positions.reserve(count);
    
    std::uniform_int_distribution<uint16_t> dist(0, static_cast<uint16_t>(config_.dim - 1));
    std::unordered_set<uint16_t> seen;
    
    while (positions.size() < count) {
        uint16_t pos = dist(gen);
        if (seen.insert(pos).second) {
            positions.push_back(pos);
        }
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

int8_t SparseHDC::randomTernary(std::mt19937& gen) const {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(gen) ? 1 : -1;
}

std::string SparseHDC::serialize() const {
    std::ostringstream oss;
    oss << "{\"dim\":" << config_.dim
        << ",\"sparsity\":" << config_.sparsity
        << ",\"active_bits\":" << config_.active_bits
        << ",\"total_vectors\":" << stats_.total_vectors
        << ",\"avg_sparsity\":" << stats_.avg_sparsity << "}";
    return oss.str();
}

bool SparseHDC::deserialize(const std::string& data) {
    // Simplified
    return true;
}

} // namespace memory
} // namespace yuki