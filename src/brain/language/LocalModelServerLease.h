#pragma once

#include <cstdint>
#include <string>

namespace yuki::brain::language {

struct LocalModelServerLease {
    bool attachedToExistingServer{false};
    bool ownedByYuki{false};
    uint32_t processId{0};
    std::string endpoint;
    std::string launchFingerprint;
    std::string ownershipToken;
};

} // namespace yuki::brain::language
