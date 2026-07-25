#include "brain/action/core/ExecutionReport.h"
#include <cstring>

namespace yuki {
namespace action {

void ExecutionReport::computeOverallSuccess() {
    if (results.empty()) {
        overallSuccess = 0.0f;
        return;
    }

    uint32_t successCount = 0;
    for (const auto& result : results) {
        if (result.status == ActionStatus::SUCCESS) {
            successCount++;
        }
    }

    overallSuccess = static_cast<float>(successCount) / static_cast<float>(results.size());
}

std::vector<uint8_t> ExecutionReport::serialize() const {
    std::vector<uint8_t> data;
    data.resize(sizeof(uint64_t) * 3 + sizeof(float) * 2 + sizeof(uint32_t));
    size_t offset = 0;

    std::memcpy(data.data() + offset, &reportId, sizeof(reportId));
    offset += sizeof(reportId);
    std::memcpy(data.data() + offset, &startTime, sizeof(startTime));
    offset += sizeof(startTime);
    std::memcpy(data.data() + offset, &endTime, sizeof(endTime));
    offset += sizeof(endTime);
    std::memcpy(data.data() + offset, &overallSuccess, sizeof(overallSuccess));
    offset += sizeof(overallSuccess);
    std::memcpy(data.data() + offset, &totalDurationMs, sizeof(totalDurationMs));
    offset += sizeof(totalDurationMs);

    return data;
}

} // namespace action
} // namespace yuki
