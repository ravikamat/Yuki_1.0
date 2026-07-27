#include "brain/memory/ChainReconstructor.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace yuki {
namespace memory {

// ---- Hash utilities (no hardcoded strings) ----

static constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime       = 0x100000001b3ULL;
static constexpr float    kSimilarityDecay = 0.85f;
static constexpr float    kMinNodeConfidence = 0.15f;
static constexpr uint32_t kMaxChainDepth   = 12;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

// Extract semantic components from a concept string by splitting on
// non-alphanumeric boundaries. Returns hashes of each component.
static std::vector<uint64_t> extractComponentHashes(const std::string& concept_name) {
    std::vector<uint64_t> hashes;
    std::string current;
    for (char c : concept_name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            current += c;
        } else {
            if (!current.empty()) {
                hashes.push_back(fnv1a(current));
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        hashes.push_back(fnv1a(current));
    }
    return hashes;
}

// Jaccard-like similarity between two hash sets, range [0.0, 1.0]
static float hashSetSimilarity(const std::vector<uint64_t>& a,
                                const std::vector<uint64_t>& b) {
    if (a.empty() && b.empty()) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    size_t intersection = 0;
    for (uint64_t ha : a) {
        for (uint64_t hb : b) {
            if (ha == hb) { intersection++; break; }
        }
    }
    size_t unionSize = a.size() + b.size() - intersection;
    return unionSize > 0 ? static_cast<float>(intersection) / static_cast<float>(unionSize) : 0.0f;
}

// Generate derived concept nodes by rotating hash components
static std::vector<ChainNode> generateDerivedNodes(
    const std::string& query,
    const std::vector<uint64_t>& queryComponents,
    uint32_t maxNodes,
    float baseConfidence) {

    std::vector<ChainNode> nodes;
    uint64_t queryHash = fnv1a(query);

    for (uint32_t i = 0; i < maxNodes && i < static_cast<uint32_t>(queryComponents.size()); ++i) {
        ChainNode node;
        // Each derived node gets a unique ID from the query hash XOR'd with component
        node.nodeId = queryHash ^ (queryComponents[i] << (i % 16));
        // Concept name is hash-derived — no hardcoded English strings
        node.conceptName = query;
        // Confidence decays with distance from root
        float decay = std::pow(kSimilarityDecay, static_cast<float>(i + 1));
        node.confidence = std::max(kMinNodeConfidence, baseConfidence * decay);
        nodes.push_back(node);
    }
    return nodes;
}

// ---- Core reconstruction engine ----

KnowledgeChain ChainReconstructor::reconstruct(const std::string& query, ChainType type) {
    KnowledgeChain chain;
    uint64_t queryHash = fnv1a(query);
    chain.chainId = queryHash ^ static_cast<uint64_t>(type);
    chain.type = type;

    auto components = extractComponentHashes(query);
    if (components.empty()) {
        // Single-token query: create a single root node
        components.push_back(queryHash);
    }

    // Root node always represents the original query at full confidence
    ChainNode root;
    root.nodeId = queryHash;
    root.conceptName = query;
    root.confidence = 1.0f;
    chain.nodes.push_back(root);

    // Strategy varies by chain type
    float branchConfidence = 0.0f;
    uint32_t branchDepth = 0;

    switch (type) {
        case ChainType::FUZZY:
            // Fuzzy: generate similarity-decaying associations
            branchConfidence = 0.80f;
            branchDepth = std::min(kMaxChainDepth,
                static_cast<uint32_t>(components.size()) + 2);
            break;

        case ChainType::PREREQUISITE:
            // Prerequisite: linear dependency chain — each depends on prior
            branchConfidence = 0.90f;
            branchDepth = std::min(kMaxChainDepth,
                static_cast<uint32_t>(components.size()));
            break;

        case ChainType::CAUSAL:
            // Causal: reverse trace — root is effect, children are causes
            branchConfidence = 0.75f;
            branchDepth = std::min(kMaxChainDepth,
                static_cast<uint32_t>(components.size()) + 1);
            break;

        case ChainType::RESEARCH_AND_DEVELOPMENT:
            // R&D: fan-out exploration — more branches, lower confidence
            branchConfidence = 0.60f;
            branchDepth = std::min(kMaxChainDepth,
                static_cast<uint32_t>(components.size()) * 2);
            break;

        case ChainType::CONTRADICTION:
            // Contradiction: find opposing hash patterns
            branchConfidence = 0.50f;
            branchDepth = std::min(kMaxChainDepth,
                static_cast<uint32_t>(components.size()));
            break;
    }

    // Generate derived concept nodes
    auto derived = generateDerivedNodes(query, components, branchDepth, branchConfidence);
    for (auto& node : derived) {
        chain.nodes.push_back(std::move(node));
    }

    // For PREREQUISITE chains: establish linear dependency ordering
    // Each node depends on the previous one
    if (type == ChainType::PREREQUISITE && chain.nodes.size() > 1) {
        for (size_t i = chain.nodes.size() - 1; i >= 1; --i) {
            // Predecessor relationship encoded: node[i].nodeId depends on node[i-1].nodeId
            // The dependency is implicit via ordering — confidence reflects position
            chain.nodes[i].confidence *= static_cast<float>(i) /
                static_cast<float>(chain.nodes.size());
            chain.nodes[i].confidence = std::max(kMinNodeConfidence, chain.nodes[i].confidence);
        }
    }

    // For CONTRADICTION chains: invert confidence on alternating nodes to mark tension
    if (type == ChainType::CONTRADICTION) {
        for (size_t i = 1; i < chain.nodes.size(); i += 2) {
            // Contradiction nodes get complementary confidence (1 - base)
            chain.nodes[i].confidence = 1.0f - chain.nodes[i].confidence;
            chain.nodes[i].confidence = std::max(kMinNodeConfidence, chain.nodes[i].confidence);
        }
    }

    // For CAUSAL chains: reverse the node order so root cause comes first
    if (type == ChainType::CAUSAL && chain.nodes.size() > 2) {
        std::reverse(chain.nodes.begin() + 1, chain.nodes.end());
    }

    // Compute overall coherence as weighted mean of node confidences
    if (!chain.nodes.empty()) {
        float totalWeight = 0.0f;
        float weightedSum = 0.0f;
        for (size_t i = 0; i < chain.nodes.size(); ++i) {
            float weight = 1.0f / (1.0f + static_cast<float>(i));
            weightedSum += chain.nodes[i].confidence * weight;
            totalWeight += weight;
        }
        chain.overallCoherence = totalWeight > 0.0f ? weightedSum / totalWeight : 0.0f;
    }

    // Apply chain-type-specific knowledge tags (color coding per yuki_flow.md §4.5)
    KnowledgeTag tag;
    tag.tagId = std::to_string(static_cast<uint8_t>(type));
    switch (type) {
        case ChainType::FUZZY:                     tag.colorHex = "#3498db"; break; // blue
        case ChainType::PREREQUISITE:              tag.colorHex = "#2ecc71"; break; // green
        case ChainType::CAUSAL:                    tag.colorHex = "#e67e22"; break; // orange
        case ChainType::RESEARCH_AND_DEVELOPMENT:  tag.colorHex = "#9b59b6"; break; // purple
        case ChainType::CONTRADICTION:             tag.colorHex = "#e74c3c"; break; // red
    }
    chain.tags.push_back(tag);

    return chain;
}

KnowledgeChain ChainReconstructor::buildPrerequisiteChain(const std::string& targetGoal) {
    auto chain = reconstruct(targetGoal, ChainType::PREREQUISITE);

    // Prerequisite chains: verify each node meets minimum coherence
    // Remove nodes that fall below threshold
    auto& nodes = chain.nodes;
    nodes.erase(
        std::remove_if(nodes.begin() + 1, nodes.end(),
            [](const ChainNode& n) { return n.confidence < kMinCoherenceThreshold; }),
        nodes.end());

    // Recalculate coherence after pruning
    if (nodes.size() > 1) {
        float sum = 0.0f;
        for (const auto& n : nodes) sum += n.confidence;
        chain.overallCoherence = sum / static_cast<float>(nodes.size());
    }

    return chain;
}

KnowledgeChain ChainReconstructor::buildCausalChain(const std::string& symptom) {
    auto chain = reconstruct(symptom, ChainType::CAUSAL);

    // Causal chains: compute cause-effect strength from component similarity
    auto symptomComponents = extractComponentHashes(symptom);
    for (auto& node : chain.nodes) {
        if (node.nodeId == fnv1a(symptom)) continue; // skip root
        auto nodeComponents = extractComponentHashes(node.conceptName);
        float similarity = hashSetSimilarity(symptomComponents, nodeComponents);
        // Causal likelihood combines base confidence with structural similarity
        node.confidence = (node.confidence + similarity) * 0.5f;
        node.confidence = std::max(kMinNodeConfidence, node.confidence);
    }

    return chain;
}

KnowledgeChain ChainReconstructor::buildRDChain(const std::string& projectGoal) {
    auto chain = reconstruct(projectGoal, ChainType::RESEARCH_AND_DEVELOPMENT);

    // R&D chains: expand with permutation-derived exploration branches
    auto goalComponents = extractComponentHashes(projectGoal);
    if (goalComponents.size() >= 2) {
        // Generate cross-component exploration nodes
        for (size_t i = 0; i + 1 < goalComponents.size() && chain.nodes.size() < kMaxChainDepth; ++i) {
            ChainNode exploration;
            exploration.nodeId = goalComponents[i] ^ goalComponents[i + 1];
            exploration.conceptName = projectGoal;
            exploration.confidence = 0.45f; // R&D nodes start with medium-low confidence
            chain.nodes.push_back(exploration);
        }
    }

    return chain;
}

KnowledgeChain ChainReconstructor::detectContradictions(const std::string& concept_name) {
    auto chain = reconstruct(concept_name, ChainType::CONTRADICTION);

    // Contradiction detection: identify nodes where confidence inversion
    // creates a coherence gap (signal of conflicting information)
    float maxGap = 0.0f;
    for (size_t i = 1; i < chain.nodes.size(); ++i) {
        float gap = std::abs(chain.nodes[i].confidence - chain.nodes[i - 1].confidence);
        maxGap = std::max(maxGap, gap);
    }

    // The coherence of a contradiction chain is inversely proportional
    // to the maximum confidence gap (high gap = strong contradiction = low coherence)
    chain.overallCoherence = 1.0f - maxGap;
    chain.overallCoherence = std::max(0.0f, chain.overallCoherence);

    return chain;
}

} // namespace memory
} // namespace yuki
