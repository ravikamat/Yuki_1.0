#ifndef YUKI_SUBGOAL_H
#define YUKI_SUBGOAL_H

#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace research {

enum class GoalStatus : uint8_t {
    UNSPECIFIED = 0,
    SATISFIED,
    NEEDS_VERIFICATION,
    NEEDS_RESEARCH,
    FAILED
};

struct SubGoal {
    uint64_t              goalId = 0;
    uint64_t              descriptionHash = 0;
    std::vector<uint64_t> requiredSchemaHashes;
    float                 confidence = 0.0f;
    std::vector<uint64_t> dependencies;
    bool                  satisfied = false;
    GoalStatus            status = GoalStatus::UNSPECIFIED;

    static constexpr float kDefaultConfidence = 0.0f;
    static constexpr float kMinConfidenceThreshold = 0.5f;
};

} // namespace research
} // namespace yuki

#endif
