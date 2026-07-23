#include "CompetenceRecord.h"

namespace yuki::metacognition {

void CompetenceRecord::update(bool success) {
    float outcome = success ? 1.0f : 0.0f;
    if (sample_count == 0) {
        success_rate_ema = outcome;
    } else {
        success_rate_ema = alpha * outcome + (1.0f - alpha) * success_rate_ema;
    }
    sample_count++;
    if (success) success_count++;
    else failure_count++;
}

} // namespace yuki::metacognition
