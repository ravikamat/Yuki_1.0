#include "PolicyDivergenceLogger.h"
#include <chrono>

namespace yuki::metacognition {

void PolicyDivergenceLogger::log(uint8_t legacy_mode,
                                  uint8_t ensemble_mode,
                                  float   confidence) noexcept {
    DivergenceRecord rec;
    rec.timestamp_ms  = nowMs();
    rec.legacy_mode   = legacy_mode;
    rec.ensemble_mode = ensemble_mode;
    rec.confidence    = confidence;

    std::lock_guard<std::mutex> lk(log_mutex_);
    records_.push_back(rec);
}

std::vector<DivergenceRecord> PolicyDivergenceLogger::snapshot() const {
    std::lock_guard<std::mutex> lk(log_mutex_);
    return records_;
}

size_t PolicyDivergenceLogger::size() const noexcept {
    std::lock_guard<std::mutex> lk(log_mutex_);
    return records_.size();
}

void PolicyDivergenceLogger::clear() noexcept {
    std::lock_guard<std::mutex> lk(log_mutex_);
    records_.clear();
}

uint64_t PolicyDivergenceLogger::nowMs() noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace yuki::metacognition
