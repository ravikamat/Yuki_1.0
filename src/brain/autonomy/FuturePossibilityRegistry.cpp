#include "src/brain/autonomy/FuturePossibilityRegistry.h"

namespace yuki::autonomy {

void FuturePossibilityRegistry::store(const FuturePossibilityRecord& record) {
    records_[record.recordId] = record;
}

std::vector<FuturePossibilityRecord> FuturePossibilityRegistry::dueForRevisit(std::uint64_t now) const {
    std::vector<FuturePossibilityRecord> result;
    for (const auto& kv : records_) {
        if (kv.second.revisitAt <= now && kv.second.revisitAt > 0) {
            result.push_back(kv.second);
        }
    }
    return result;
}

std::vector<FuturePossibilityRecord> FuturePossibilityRegistry::findByBlocker(const std::string& blocker) const {
    std::vector<FuturePossibilityRecord> result;
    for (const auto& kv : records_) {
        for (const auto& b : kv.second.blockers) {
            if (b == blocker) {
                result.push_back(kv.second);
                break;
            }
        }
    }
    return result;
}

const std::unordered_map<std::string, FuturePossibilityRecord>& FuturePossibilityRegistry::allRecords() const noexcept {
    return records_;
}

} // namespace yuki::autonomy
