#ifndef YUKI_HISTORICAL_DATA_REPLAY_H
#define YUKI_HISTORICAL_DATA_REPLAY_H

#include <cstdint>
#include <vector>

namespace yuki {
namespace testing {

struct ReplayPacket {
    uint64_t timestamp = 0;
    std::vector<uint8_t> payload;
};

class HistoricalDataReplay {
public:
    void loadPackets(const std::vector<ReplayPacket>& packets);
    uint64_t replay(double speedMultiplier = 1000000.0);

    size_t getPacketCount() const { return packets_.size(); }
    void clear() { packets_.clear(); }

private:
    std::vector<ReplayPacket> packets_;
};

} // namespace testing
} // namespace yuki

#endif
