#pragma once
// SymbolicLearningEpisode — Episode structure for COGNI-1 symbolic reasoning/learning
// Distinct from LearningEpisode.h (which is for self-improvement loop)

#include "../reasoning/ClingoStore.h"
#include <vector>
#include <string>

namespace yuki {
namespace brain {
namespace learning {

struct SymbolicLearningEpisode {
    uint64_t episode_id = 0;
    std::vector<reasoning::Atom> initial_facts;
    std::vector<int> actions;  // Action sequence
    std::vector<reasoning::Atom> resulting_facts;
    float reward = 0.0f;
    bool success = false;
    std::string task_type;  // "retrieval", "planning", etc.
};

} // namespace learning
} // namespace brain
} // namespace yuki