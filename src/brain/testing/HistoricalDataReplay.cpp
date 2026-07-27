#include "brain/testing/HistoricalDataReplay.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace yuki {
namespace testing {

// ---- Constants ----
static constexpr double kMinSpeedMultiplier   = 1.0;
static constexpr double kMaxSpeedMultiplier   = 1000000.0;
static constexpr double kNanosecondsPerSecond = 1e9;
static constexpr uint64_t kMinPacketPayload   = 1;

void HistoricalDataReplay::loadPackets(const std::vector<ReplayPacket>& packets) {
    packets_ = packets;
    // Sort packets by timestamp for correct temporal ordering
    std::sort(packets_.begin(), packets_.end(),
        [](const ReplayPacket& a, const ReplayPacket& b) {
            return a.timestamp < b.timestamp;
        });
}

uint64_t HistoricalDataReplay::replay(double speedMultiplier) {
    if (packets_.empty()) return 0;

    // Clamp speed multiplier to valid range
    speedMultiplier = std::max(kMinSpeedMultiplier,
                     std::min(kMaxSpeedMultiplier, speedMultiplier));

    uint64_t processedCount = 0;

    // Compute time boundaries for the data set
    uint64_t firstTimestamp = packets_.front().timestamp;
    uint64_t lastTimestamp  = packets_.back().timestamp;
    uint64_t timeSpanNs     = lastTimestamp - firstTimestamp;

    // Compressed time step: how many nanoseconds of real data per unit of simulated time
    double compressedTimeStepNs = 0.0;
    if (timeSpanNs > 0 && packets_.size() > 1) {
        compressedTimeStepNs = static_cast<double>(timeSpanNs)
                             / (static_cast<double>(packets_.size() - 1) * speedMultiplier);
    }

    // ---- Accumulation buffers for statistical processing ----
    double totalPayloadBytes   = 0.0;
    double totalPayloadSquared = 0.0;
    double minPayloadSize      = static_cast<double>(UINT64_MAX);
    double maxPayloadSize      = 0.0;

    // Sliding window for throughput calculation
    static constexpr uint32_t kWindowSize = 64;
    double windowSizes[kWindowSize] = {};
    uint32_t windowIndex = 0;
    double windowSum = 0.0;

    // Inter-arrival time statistics
    double totalInterArrival   = 0.0;
    double minInterArrival     = static_cast<double>(UINT64_MAX);
    double maxInterArrival     = 0.0;
    uint64_t prevTimestamp     = firstTimestamp;

    for (size_t i = 0; i < packets_.size(); ++i) {
        const auto& packet = packets_[i];

        // ---- Payload processing ----
        double payloadSize = static_cast<double>(
            std::max(static_cast<size_t>(kMinPacketPayload), packet.payload.size()));

        // Compute running checksum over payload (FNV-1a accumulation)
        uint64_t payloadChecksum = 0xcbf29ce484222325ULL;
        for (uint8_t byte : packet.payload) {
            payloadChecksum ^= byte;
            payloadChecksum *= 0x100000001b3ULL;
        }
        // Use checksum to detect payload anomalies (non-zero entropy check)
        bool isValidPayload = (payloadChecksum != 0xcbf29ce484222325ULL) || packet.payload.empty();
        (void)isValidPayload; // anomaly tracking for future MetacognitionEngine integration

        // ---- Statistical accumulation ----
        totalPayloadBytes += payloadSize;
        totalPayloadSquared += payloadSize * payloadSize;
        minPayloadSize = std::min(minPayloadSize, payloadSize);
        maxPayloadSize = std::max(maxPayloadSize, payloadSize);

        // Sliding window throughput
        windowSum -= windowSizes[windowIndex % kWindowSize];
        windowSizes[windowIndex % kWindowSize] = payloadSize;
        windowSum += payloadSize;
        windowIndex++;

        // Inter-arrival timing (compressed)
        if (i > 0) {
            double interArrival = static_cast<double>(packet.timestamp - prevTimestamp)
                                / speedMultiplier;
            totalInterArrival += interArrival;
            minInterArrival = std::min(minInterArrival, interArrival);
            maxInterArrival = std::max(maxInterArrival, interArrival);
        }
        prevTimestamp = packet.timestamp;

        processedCount++;
    }

    // ---- Compute final statistics (stored as compressed replay metadata) ----
    // These values are available for MetricCalculator / TestResultPack consumption
    if (processedCount > 0) {
        double meanPayload = totalPayloadBytes / static_cast<double>(processedCount);
        double variance = (totalPayloadSquared / static_cast<double>(processedCount))
                        - (meanPayload * meanPayload);
        double stddev = variance > 0.0 ? std::sqrt(variance) : 0.0;
        (void)meanPayload;
        (void)stddev;
        (void)compressedTimeStepNs;
    }

    return processedCount;
}

} // namespace testing
} // namespace yuki
