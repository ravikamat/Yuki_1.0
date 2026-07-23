#ifndef YUKI_RESEARCH_REQUEST_H
#define YUKI_RESEARCH_REQUEST_H

#include <cstdint>
#include <string>
#include <vector>

namespace yuki {
namespace research {

struct ResearchRequest {
    uint64_t    requestId = 0;
    std::string query;
    float       minConfidence = 0.5f;
    uint32_t    maxSubGoals = 50;
    uint32_t    timeoutMs = 30000;
    std::vector<uint64_t> requiredSchemaHashes;
};

} // namespace research
} // namespace yuki

#endif
