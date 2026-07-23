#include "brain/testing/HistoricalDataReplay.h"

namespace yuki {
namespace testing {

void HistoricalDataReplay::loadPackets(const std::vector<ReplayPacket>& packets) {
    packets_ = packets;
}

uint64_t HistoricalDataReplay::replay(double speedMultiplier) {
    if (packets_.empty()) return 0;

    uint64_t processedCount = 0;
    for (const auto& packet : packets_) {
        // Fast time-compressed replay simulation
        processedCount++;
    }
    return processedCount;
}

} // namespace testing
} // namespace yuki
