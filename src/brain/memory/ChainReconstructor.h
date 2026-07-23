#ifndef YUKI_CHAIN_RECONSTRUCTOR_H
#define YUKI_CHAIN_RECONSTRUCTOR_H

#include "brain/memory/KnowledgeTag.h"
#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace memory {

enum class ChainType : uint8_t {
    FUZZY = 0,
    PREREQUISITE,
    CAUSAL,
    RESEARCH_AND_DEVELOPMENT,
    CONTRADICTION
};

struct ChainNode {
    uint64_t    nodeId = 0;
    std::string conceptName;
    float       confidence = 0.0f;
};

struct KnowledgeChain {
    uint64_t               chainId = 0;
    ChainType              type = ChainType::FUZZY;
    std::vector<ChainNode> nodes;
    float                  overallCoherence = 0.0f;
    std::vector<KnowledgeTag> tags;
};

class ChainReconstructor {
public:
    KnowledgeChain reconstruct(const std::string& query, ChainType type = ChainType::FUZZY);
    KnowledgeChain buildPrerequisiteChain(const std::string& targetGoal);
    KnowledgeChain buildCausalChain(const std::string& symptom);
    KnowledgeChain buildRDChain(const std::string& projectGoal);
    KnowledgeChain detectContradictions(const std::string& concept);

    static constexpr float kMinCoherenceThreshold = 0.4f;
};

} // namespace memory
} // namespace yuki

#endif
