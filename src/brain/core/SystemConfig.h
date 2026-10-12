#pragma once
#include <string>
#include <cstddef>
#include "brain/core/CognitiveConstants.h"

namespace yuki {

struct SystemConfig {
    // SWARM COGNITIVE FABRIC (Phase 2)
    static inline bool use_swarm_fabric = false;          // DEFAULT FALSE
    static inline std::string brain_root = "YUKI_BRAIN";
    static inline size_t max_concurrent_agents = 64;
    static inline bool enable_background_merge = true;
    static inline size_t merge_interval_ms = 5000;
};

} // namespace yuki
