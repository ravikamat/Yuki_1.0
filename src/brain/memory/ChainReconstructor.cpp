#include "brain/memory/ChainReconstructor.h"

namespace yuki {
namespace memory {

KnowledgeChain ChainReconstructor::reconstruct(const std::string& query, ChainType type) {
    KnowledgeChain chain;
    chain.chainId = 0x8801;
    chain.type = type;

    ChainNode root;
    root.nodeId = 1;
    root.conceptName = query;
    root.confidence = 0.9f;
    chain.nodes.push_back(root);

    ChainNode child;
    child.nodeId = 2;
    child.conceptName = query + "_derived_concept";
    child.confidence = 0.75f;
    chain.nodes.push_back(child);

    chain.overallCoherence = 0.825f;

    KnowledgeTag tag;
    tag.tagId = "auto_reconstructed";
    tag.colorHex = "#2ecc71";
    chain.tags.push_back(tag);

    return chain;
}

KnowledgeChain ChainReconstructor::buildPrerequisiteChain(const std::string& targetGoal) {
    return reconstruct(targetGoal, ChainType::PREREQUISITE);
}

KnowledgeChain ChainReconstructor::buildCausalChain(const std::string& symptom) {
    return reconstruct(symptom, ChainType::CAUSAL);
}

KnowledgeChain ChainReconstructor::buildRDChain(const std::string& projectGoal) {
    return reconstruct(projectGoal, ChainType::RESEARCH_AND_DEVELOPMENT);
}

KnowledgeChain ChainReconstructor::detectContradictions(const std::string& concept) {
    return reconstruct(concept, ChainType::CONTRADICTION);
}

} // namespace memory
} // namespace yuki
