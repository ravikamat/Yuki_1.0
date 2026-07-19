// SdmOptimizer.cpp — SDM memory estimation + compaction logic
#include "brain/memory/SdmOptimizer.h"

namespace yuki {
namespace memory {

size_t SdmOptimizer::estimateMemoryUsage(size_t hard_locations, size_t bits) {
    return hard_locations * bits; // 1 byte per counter
}

bool SdmOptimizer::shouldCompact(size_t write_count, size_t last_compact_write_count, float noise_estimate) {
    // Adaptive: compact more frequently when noise is high (saturation)
    size_t base_threshold = 10000;
    if (noise_estimate > 64.0f) base_threshold = 5000;  // high noise = compact 2x
    return (write_count - last_compact_write_count) >= base_threshold;
}

void SdmOptimizer::runCompaction(std::vector<uint8_t>& counters) {
    for (uint8_t& c : counters) {
        if      (c < 2)   c = 0;
        else if (c > 250) c = 255;
    }
}

} // namespace memory
} // namespace yuki
