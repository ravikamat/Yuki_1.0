#pragma once
#include "Hypervector.h"
#include "LocalitySensitiveHash.h"
#include "ModernHopfieldMemory.h"
#include "brain/memory/SdmOptimizer.h"
#include <functional>
#include <vector>
#include <array>
#include <chrono>
#include <mutex>
#include "yuki_shared_mutex_wrapper.h"
#include <random>
#include <cstdint>

namespace yuki::memory {

// Kanerva-style Sparse Distributed Memory (SDM)
// 10K hard locations indexed by an internal LSH — selectNearest is O(candidates)
// not O(HARD_LOCATIONS). Counter memory: 10K × 20KB int16 = 200 MB.
// 100K hard locations requires sparse counter storage (Phase 3).
// FHRR Support: Phase-based addressing with lossless binding/unbinding.
class SparseDistributedMemory {
public:
    static constexpr size_t  kDefaultHardLocations = 10000;
    static constexpr size_t  kMaxHardLocations     = 100000;
    size_t                   hard_locations_ = kDefaultHardLocations;  // runtime
    static constexpr size_t  SELECTIVITY      = 100;    // legacy alias
    static constexpr size_t  kActivationCount = 256;    // LSH pre-filtered activations
    static constexpr size_t  MAX_CONTENTS     = 16;
    static constexpr uint8_t kCounterMax      = 255;
    static constexpr uint8_t kCounterNeutral  = 128;    // baseline: equal 0s and 1s

    struct Content {
        Hypervector vector;        // Binary HDC content
        FHRRVector   fhrr_vector;  // FHRR content (new)
        float       strength     = 1.0f;
        uint64_t    access_count = 0;
        std::chrono::system_clock::time_point last_access;
    };

    SparseDistributedMemory();
    ~SparseDistributedMemory() = default;

    // Binary HDC operations (existing)
    void write(const Hypervector& address, const Content& content);
    void reinforce(const Hypervector& address, int times = 5);
    std::vector<Content> read(const Hypervector& address, size_t max_results = 10);
    void forget(float decay_rate = 0.95f);
    // Number of logical writes performed (one per write() call), not the
    // number of stored content copies (each write fans out to ~256 hard
    // locations, so counting copies made size() grow ~256× per write).
    size_t size() const;
    void clear();

    // FHRR operations (new)
    void writeFHRR(const FHRRVector& address, const Content& content);
    void reinforceFHRR(const FHRRVector& address, int times = 5);
    std::vector<Content> readFHRR(const FHRRVector& address, size_t max_results = 10);

    // Runtime scaling: grow hard locations based on write pressure
    void resize(size_t new_count);
    size_t capacity() const { return hard_locations_; }
    // Memory pressure: counter variance across locations indicates saturation
    float readNoiseEstimate() const;

    void compactCounters(); // Periodic saturation cleanup (run every 10K writes)
    void setMemoryPressureCallback(std::function<void(const std::vector<uint8_t>&)> callback);
    // Wire the EpisodicStore content-LSH into SDM (optional dual-index reads)
    void setLshIndex(LocalitySensitiveHash* lsh) { lsh_index_ = lsh; }

    // Modern Hopfield continuous energy & exponential associative access
    const ModernHopfieldMemory& getHopfield() const noexcept { return hopfield_; }
    ModernHopfieldMemory& getHopfield() noexcept { return hopfield_; }
    void setUseHopfield(bool enable) noexcept { use_hopfield_ = enable; }
    bool isUsingHopfield() const noexcept { return use_hopfield_; }

private:
    struct HardLocation {
        Hypervector           address_binary;   // Binary HDC address
        FHRRVector            address_fhrr;     // FHRR address (new)
        std::vector<uint8_t>  counters;         // DIM bytes, baseline 128 (saturating uint8)
        std::vector<Content>  contents;
        uint64_t              write_count = 0;
    };

    std::vector<HardLocation>    locations_;
    mutable yuki::concurrency::shared_mutex mtx_;
    std::mt19937                 rng_;
    mutable LocalitySensitiveHash location_lsh_;
    LocalitySensitiveHash*        lsh_index_ = nullptr;

    // Compaction tracking
    size_t total_writes_        = 0;
    size_t last_compact_write_  = 0;
    std::function<void(const std::vector<uint8_t>&)> pressure_cb_;

    ModernHopfieldMemory          hopfield_;
    bool                          use_hopfield_{true};

    // Binary HDC nearest
    std::vector<size_t> selectNearest(const Hypervector& query, size_t k) const;
    // FHRR nearest (phase-based)
    std::vector<size_t> selectNearestFHRR(const FHRRVector& query, size_t k) const;
};

} // namespace yuki::memory
