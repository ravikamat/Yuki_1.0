// SdmOptimizer.h — Yuki_1.0 Phase D: SDM compaction utilities
#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

namespace yuki {
namespace memory {

class SdmOptimizer {
public:
    // Returns estimated memory bytes for SDM counters only (hard_locations × bits × 1 byte)
    static size_t estimateMemoryUsage(size_t hard_locations, size_t bits);

    static bool shouldCompact(size_t write_count, size_t last_compact_write_count, float noise_estimate);

    // Compact all counters for one hard location:
    //   counter < 2   → set to 0   (strong-zero lock)
    //   counter > 250 → set to 255 (strong-one lock)
    //   Leaves neutral values (2–250) unchanged.
    static void runCompaction(std::vector<uint8_t>& counters);
};

} // namespace memory
} // namespace yuki
