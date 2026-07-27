#ifndef YUKI_ROLLBACK_MANAGER_H
#define YUKI_ROLLBACK_MANAGER_H

#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace action {

struct Checkpoint {
    uint64_t checkpointId = 0;
    uint64_t timestamp = 0;
    std::vector<uint8_t> stateSnapshot;
    std::string description;
    bool isValid = false;
    bool isRollbackable = true;
};

class RollbackManager {
public:
    uint64_t createCheckpoint(const std::string& description);
    bool rollbackTo(uint64_t checkpointId);
    bool validateCheckpoint(uint64_t checkpointId);
    std::vector<Checkpoint> listCheckpoints() const;
    void invalidateCheckpoint(uint64_t checkpointId);
    void clearCheckpoints();

    static constexpr uint32_t kMaxCheckpoints = 100;

private:
    std::vector<Checkpoint> checkpoints_;
    uint64_t nextCheckpointId_ = 1;

    uint64_t computeChecksum(const std::vector<uint8_t>& data);
};

} // namespace action
} // namespace yuki

#endif
