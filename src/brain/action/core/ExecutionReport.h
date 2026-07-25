#ifndef YUKI_EXECUTION_REPORT_H
#define YUKI_EXECUTION_REPORT_H

#include "brain/action/core/ActionGoal.h"
#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace action {

struct ActionResult {
    uint64_t nodeId = 0;
    ActionStatus status = ActionStatus::PENDING;
    float confidence = 0.0f;
    std::vector<uint8_t> payload;
    uint64_t timestamp = 0;
    uint32_t retryCount = 0;
    bool rollbackAvailable = false;
    std::string rollbackLog;
};

class ExecutionReport {
public:
    uint64_t reportId = 0;
    std::vector<ActionResult> results;
    float overallSuccess = 0.0f;
    std::vector<uint64_t> failedNodes;
    std::vector<uint64_t> rolledBackNodes;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    float totalDurationMs = 0.0f;

    void computeOverallSuccess();
    std::vector<uint8_t> serialize() const;
};

} // namespace action
} // namespace yuki

#endif
