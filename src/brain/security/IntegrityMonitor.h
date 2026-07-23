#ifndef YUKI_INTEGRITY_MONITOR_H
#define YUKI_INTEGRITY_MONITOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace yuki {
namespace security {

enum class CorruptionType : uint8_t {
    NONE = 0,
    HASH_MISMATCH,
    MEMORY_TAMPERING,
    UNAUTHORIZED_WRITE
};

struct CorruptionReport {
    CorruptionType type = CorruptionType::NONE;
    std::string    moduleName;
    uint64_t       expectedHash = 0;
    uint64_t       actualHash = 0;
};

class IntegrityMonitor {
public:
    bool verifyModule(const std::string& moduleName, uint64_t expectedHash);
    std::vector<CorruptionReport> verifyAllModules();
    bool rollbackToCheckpoint(const std::string& checkpointId);
    void quarantineModule(const std::string& moduleName);
};

} // namespace security
} // namespace yuki

#endif
