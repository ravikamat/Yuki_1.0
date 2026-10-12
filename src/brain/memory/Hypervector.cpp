// Hypervector.cpp G– Phase D: 64-bit word implementation + FHRR
#include "brain/memory/Hypervector.h"
#include <intrin.h>      // __popcnt64 (clang-cl / MSVC ABI intrinsic)
#include <sstream>
#include <iomanip>
#include <cstring>
#include <random>
#include <cmath>

namespace yuki::memory {

// G– popcount (clang-cl/MSVC ABI intrinsic) G–
size_t Hypervector::popcount64_(uint64_t w) {
    return static_cast<size_t>(__popcnt64(w));
}

// G– Binary HDC Constructors G–
Hypervector::Hypervector() { words_.fill(0); }

Hypervector::Hypervector(uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<uint64_t> dist;
    for (auto& w : words_) w = dist(gen);
    fixLastWord_();
}

Hypervector::Hypervector(const std::string& text) {
    words_.fill(0);
    std::hash<std::string> hasher;
    std::mt19937 gen(static_cast<uint32_t>(hasher(text)));
    std::uniform_int_distribution<size_t> dist(0, DIM - 1);
    for (int i = 0; i < 50; ++i) {
        size_t bit = dist(gen);
        words_[bit >> 6] |= (uint64_t(1) << (bit & 63));
    }
    for (unsigned char c : text) {
        size_t bit = (static_cast<size_t>(
                          hasher(std::string(1, static_cast<char>(c)))) * 31ULL) % DIM;
        words_[bit >> 6] |= (uint64_t(1) << (bit & 63));
    }
    // no need to fixLastWord_: only set bits in [0,DIM)
}

// G– Binary HDC algebra G–
Hypervector Hypervector::bind(const Hypervector& other) const {
    Hypervector result;
    for (size_t w = 0; w < kWords; ++w)
        result.words_[w] = words_[w] ^ other.words_[w];
    // No fixLastWord_ needed: XOR preserves zero extra bits.
    return result;
}

Hypervector Hypervector::bundle(const Hypervector& other) const {
    // Two-vector majority-vote: tie (0,0)G0, (1,1)G1, (0,1)G0 G AND
    Hypervector result;
    for (size_t w = 0; w < kWords; ++w)
        result.words_[w] = words_[w] & other.words_[w];
    return result;
}

Hypervector Hypervector::permute(size_t shifts) const {
    if (shifts == 0) return *this;
    const size_t S = shifts % DIM;
    
    Hypervector result;
    for (size_t i = 0; i < DIM; ++i) {
        if (this->get(i)) {
            result.set((i + S) % DIM, true);
        }
    }
    return result;
}

// G– Similarity G–
size_t Hypervector::hammingDistance(const Hypervector& other) const {
    size_t dist = 0;
    for (size_t w = 0; w < kWords; ++w)
        dist += popcount64_(words_[w] ^ other.words_[w]);
    return dist;
}

float Hypervector::cosineSimilarity(const Hypervector& other) const {
    size_t match = DIM - hammingDistance(other);
    return (2.0f * static_cast<float>(match) / static_cast<float>(DIM)) - 1.0f;
}

// G– Serialisation G–
std::string Hypervector::toHex() const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(DIM / 4);  // 2500 chars
    for (size_t i = 0; i < DIM; i += 4) {
        size_t  w      = i >> 6;        // i / 64
        size_t  b      = i & 63;        // i % 64 (always multiple of 4 G safe in one word)
        uint8_t nibble = static_cast<uint8_t>((words_[w] >> b) & 0xFULL);
        result += hex_chars[nibble];
    }
    return result;
}

Hypervector Hypervector::fromHex(const std::string& hex) {
    Hypervector result;
    result.words_.fill(0);
    size_t idx = 0;
    for (char c : hex) {
        if (idx >= DIM) break;
        int val = (c >= '0' && c <= '9') ? (c - '0')
                : (c >= 'a' && c <= 'f') ? (c - 'a' + 10)
                : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0;
        size_t w = idx >> 6;
        size_t b = idx & 63;
        result.words_[w] |= (static_cast<uint64_t>(val) << b);
        idx += 4;
    }
    return result;
}

// G– Generators G–
Hypervector Hypervector::random(std::mt19937& gen) {
    Hypervector result;
    std::uniform_int_distribution<uint64_t> dist;
    for (auto& w : result.words_) w = dist(gen);
    result.fixLastWord_();
    return result;
}

Hypervector Hypervector::zero() {
    Hypervector result;
    result.words_.fill(0);
    return result;
}

Hypervector Hypervector::one() {
    Hypervector result;
    result.words_.fill(~uint64_t(0));
    result.fixLastWord_();
    return result;
}

// G– FHRRVector Implementation G–
constexpr float TWO_PI = 2.0f * M_PI;

void FHRRVector::normalizePhase(Phase& p) {
    p = std::fmod(p, 2 * M_PI);
    if (p < 0) p += 2 * M_PI;
}

FHRRVector::Phase FHRRVector::randomPhase(std::mt19937& gen) {
    std::uniform_real_distribution<float> dist(0.0f, 2.0f * M_PI);
    return dist(gen);
}

FHRRVector::FHRRVector() : phases_(DIM, 0.0f) {}

FHRRVector::FHRRVector(uint64_t seed) : phases_(DIM, 0.0f) {
    std::mt19937 gen(static_cast<uint32_t>(seed));
    for (auto& p : phases_) p = static_cast<float>(gen()) * (2.0f * M_PI / 4294967296.0f);
}

FHRRVector::FHRRVector(const std::string& text) : phases_(DIM, 0.0f) {
    std::hash<std::string> hasher;
    std::mt19937 gen(static_cast<uint32_t>(hasher(text)));
    std::uniform_real_distribution<float> dist(0.0f, 2.0f * M_PI);
    for (auto& p : phases_) p = dist(gen);
    // Add semantic seeding from text
    std::hash<std::string> hasher2;
    std::mt19937 gen2(static_cast<uint32_t>(hasher2(text)));
    for (unsigned char c : text) {
        size_t idx = (static_cast<size_t>(
                          hasher2(std::string(1, static_cast<char>(c)))) * 31ULL) % DIM;
        phases_[idx] = randomPhase(gen2);
    }
}

// G– FHRR algebra (lossless, invertible) G–
FHRRVector FHRRVector::bind(const FHRRVector& other) const {
    FHRRVector result;
    result.phases_.resize(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        float sum = phases_[i] + other.phases_[i];
        if (sum >= 2 * M_PI) sum -= 2 * M_PI;
        result.phases_[i] = sum;
    }
    return result;
}

FHRRVector FHRRVector::unbind(const FHRRVector& other) const {
    FHRRVector result;
    result.phases_.resize(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        float diff = phases_[i] - other.phases_[i];
        if (diff < 0) diff += 2 * M_PI;
        result.phases_[i] = diff;
    }
    return result;
}

FHRRVector FHRRVector::permute(size_t shifts) const {
    if (shifts == 0) return *this;
    const size_t S = shifts % DIM;
    FHRRVector result;
    result.phases_.resize(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        result.phases_[i] = phases_[(i + DIM - S) % DIM];
    }
    return result;
}

// G– Similarity G–
float FHRRVector::cosineSimilarity(const FHRRVector& other) const {
    float dot = 0.0f;
    for (size_t i = 0; i < DIM; ++i) {
        float diff = phases_[i] - other.phases_[i];
        dot += std::cos(diff);
    }
    return dot / static_cast<float>(DIM);
}

float FHRRVector::phaseDifference(const FHRRVector& other) const {
    float total = 0.0f;
    for (size_t i = 0; i < DIM; ++i) {
        float diff = phases_[i] - other.phases_[i];
        if (diff < 0) diff += 2 * M_PI;
        total += diff;
    }
    return total / DIM;
}

// Exact inverse (complex conjugate = negative phase)
FHRRVector FHRRVector::inverse() const {
    FHRRVector result;
    result.phases_.resize(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        float p = phases_[i];
        if (p > 0) result.phases_[i] = 2 * M_PI - p;
        else result.phases_[i] = 0.0f;
    }
    return result;
}

// G– Serialisation G–
std::string FHRRVector::toHex() const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(DIM / 4);  // 512 chars for 2048 dims
    for (size_t i = 0; i < DIM; ++i) {
        // Encode phase as 4 hex nibbles (16-bit precision)
        uint16_t val = static_cast<uint16_t>(phases_[i] / (2 * M_PI) * 65535.0f);
        result += hex_chars[(val >> 12) & 0xF];
        result += hex_chars[(val >> 8)  & 0xF];
        result += hex_chars[(val >> 4)  & 0xF];
        result += hex_chars[val & 0xF];
    }
    return result;
}

FHRRVector FHRRVector::fromHex(const std::string& hex) {
    FHRRVector result;
    result.phases_.resize(DIM);
    for (size_t i = 0; i < DIM && i * 4 < hex.size(); ++i) {
        if (i * 4 + 4 > hex.size()) break;
        uint16_t val = 0;
        for (int j = 0; j < 4; ++j) {
            char c = hex[i * 4 + j];
            int val = (c >= '0' && c <= '9') ? (c - '0')
                    : (c >= 'a' && c <= 'f') ? (c - 'a' + 10)
                    : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0;
            val = (val << 4) | val;
        }
        result.phases_[i] = (val / 65535.0f) * 2 * M_PI;
    }
    return result;
}





// G Generators G
FHRRVector FHRRVector::random(std::mt19937& gen) {
    FHRRVector result;
    result.phases_.resize(DIM);
    for (auto& p : result.phases_) {
        p = static_cast<float>(gen()) * (2.0f * M_PI / 4294967296.0f);
    }
    return result;
}



FHRRVector FHRRVector::zero() {
    FHRRVector result;
    result.phases_.assign(DIM, 0.0f);
    return result;
}

FHRRVector FHRRVector::one() {
    FHRRVector result;
    result.phases_.assign(DIM, M_PI);
    return result;
}

} // namespace yuki::memory
