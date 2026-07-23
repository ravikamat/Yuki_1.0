#include "brain/testing/HistoricalDataReplay.h"
#include <cassert>

int main() {
    yuki::testing::HistoricalDataReplay replay;

    std::vector<yuki::testing::ReplayPacket> packets(10);
    replay.loadPackets(packets);

    uint64_t count = replay.replay(1000000.0);
    assert(count == 10);

    return 0;
}
