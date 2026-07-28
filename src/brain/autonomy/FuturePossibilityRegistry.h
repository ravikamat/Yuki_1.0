#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace yuki::autonomy {

class FuturePossibilityRegistry {
public:
    void store(const FuturePossibilityRecord& record);
    std::vector<FuturePossibilityRecord> dueForRevisit(std::uint64_t now) const;
    std::vector<FuturePossibilityRecord> findByBlocker(const std::string& blocker) const;
    const std::unordered_map<std::string, FuturePossibilityRecord>& allRecords() const noexcept;

private:
    std::unordered_map<std::string, FuturePossibilityRecord> records_;
};

} // namespace yuki::autonomy
