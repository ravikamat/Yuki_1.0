#include "brain/research/core/ResearchPlanner.h"
#include "brain/organism/DriveSystem.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>



#include "brain/memory/HeuristicMemory.h"
#include "brain/swarm/adapters/IProceduralStore.h"

namespace yuki {
namespace research {

// ── rp_runtime global definitions (headers declare extern) ──
namespace rp_runtime {
    float g_minGoalConf        = 0.15f;
    float g_minConfThreshold   = 0.50f;
    float g_maxAggregateRisk   = 0.75f;
    float g_conjunctionPenalty = 0.15f;
    float g_structureBonus     = 0.10f;
    float g_longTokenBonus     = 0.05f;
}

static std::unique_ptr<yuki::brain::memory::ProceduralHeuristicAccess> s_rpHeuristic;

// ---- Hash & structural analysis constants ----
static constexpr uint64_t kFnvOffsetBasis     = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime           = 0x100000001b3ULL;
static float s_conjPenalty    = 0.15f;
static float s_structureBonus = 0.10f;
static float s_longTokenBonus = 0.05f;
static float s_minGoalConf    = 0.15f;
static float s_maxGoalConf    = 0.95f;
static float s_schemaWeight   = 0.50f;
static float s_reliabilityW   = 0.30f;
static float s_riskPenMedium  = 0.10f;
static float s_riskPenHigh    = 0.20f;
static float s_riskPenCrit    = 0.40f;

static void loadResearchPlannerHeuristics() {
    if (!s_rpHeuristic) return;
    s_maxGoalConf   = s_rpHeuristic->loadScalar("rp.max_goal_conf",    0.95f, 0.50f, 1.00f);
    s_schemaWeight  = s_rpHeuristic->loadScalar("rp.schema_weight",    0.50f, 0.10f, 0.90f);
    s_reliabilityW  = s_rpHeuristic->loadScalar("rp.reliability_w",    0.30f, 0.05f, 0.80f);
    s_riskPenMedium = s_rpHeuristic->loadScalar("rp.risk_pen_medium",  0.10f, 0.01f, 0.50f);
    s_riskPenHigh   = s_rpHeuristic->loadScalar("rp.risk_pen_high",    0.20f, 0.05f, 0.70f);
    s_riskPenCrit   = s_rpHeuristic->loadScalar("rp.risk_pen_crit",    0.40f, 0.10f, 0.90f);
}

void ResearchPlanner::configureHeuristics(yuki::swarm::IProceduralStore& store) {
    s_rpHeuristic = std::make_unique<yuki::brain::memory::ProceduralHeuristicAccess>(store);
    loadResearchPlannerHeuristics();
    rp_runtime::g_minGoalConf        = s_rpHeuristic->loadScalar("rp.min_goal_conf",       0.15f, 0.01f, 0.50f);
    rp_runtime::g_minConfThreshold   = s_rpHeuristic->loadScalar("rp.min_conf_threshold",  0.50f, 0.10f, 0.90f);
    rp_runtime::g_maxAggregateRisk   = s_rpHeuristic->loadScalar("rp.max_aggregate_risk",  0.75f, 0.20f, 1.00f);
    rp_runtime::g_conjunctionPenalty = s_rpHeuristic->loadScalar("rp.conjunction_penalty", 0.15f, 0.01f, 0.50f);
    rp_runtime::g_structureBonus     = s_rpHeuristic->loadScalar("rp.structure_bonus",     0.10f, 0.00f, 0.50f);
    rp_runtime::g_longTokenBonus     = s_rpHeuristic->loadScalar("rp.long_token_bonus",    0.05f, 0.00f, 0.30f);
}

ResearchPlanner::~ResearchPlanner() = default;

void ResearchPlanner::setDriveSystem(yuki::organism::DriveSystem* ptr) {
    drive_system_.reset(ptr);
}

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

// Case-normalize a character without locale dependency
static char normChar(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

// Tokenize by splitting on non-alphanumeric boundaries
static std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : input) {
        char nc = normChar(c);
        if ((nc >= 'a' && nc <= 'z') || (nc >= '0' && nc <= '9') || nc == '_') {
            current += nc;
        } else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// ---- Structural Feature Extraction ----
// Analyzes query structure WITHOUT using hardcoded word lists.
// Uses character/token statistics as semantic proxies.

struct QueryFeatures {
    float avgTokenLength = 0.0f;      // Longer tokens â†’ more specific concepts
    float tokenEntropy = 0.0f;        // Higher entropy â†’ more diverse query
    float punctuationDensity = 0.0f;  // Structured queries have more punctuation
    float numericDensity = 0.0f;      // Numeric content â†’ quantitative research
    float questionStructure = 0.0f;   // Question marks â†’ inquiry intent
    uint32_t tokenCount = 0;          // Total tokens
    uint32_t uniqueHashCount = 0;     // Unique concept hashes
};

static QueryFeatures analyzeQuery(const std::string& query, const std::vector<std::string>& tokens) {
    QueryFeatures features;
    features.tokenCount = static_cast<uint32_t>(tokens.size());

    if (tokens.empty()) return features;

    // Average token length
    float totalLen = 0.0f;
    for (const auto& t : tokens) totalLen += static_cast<float>(t.length());
    features.avgTokenLength = totalLen / static_cast<float>(tokens.size());

    // Token hash entropy: count unique hashes
    std::vector<uint64_t> hashes;
    hashes.reserve(tokens.size());
    for (const auto& t : tokens) hashes.push_back(fnv1a(t));
    std::sort(hashes.begin(), hashes.end());
    auto last = std::unique(hashes.begin(), hashes.end());
    features.uniqueHashCount = static_cast<uint32_t>(std::distance(hashes.begin(), last));

    // Shannon entropy approximation from unique ratio
    float uniqueRatio = static_cast<float>(features.uniqueHashCount) / static_cast<float>(tokens.size());
    features.tokenEntropy = -uniqueRatio * std::log2(uniqueRatio + 1e-7f);

    // Punctuation density
    uint32_t punctCount = 0;
    uint32_t numericCount = 0;
    uint32_t questionCount = 0;
    for (char c : query) {
        if (c == '.' || c == ',' || c == ';' || c == ':' || c == '-') punctCount++;
        if (c >= '0' && c <= '9') numericCount++;
        if (c == '?') questionCount++;
    }
    features.punctuationDensity = query.empty() ? 0.0f :
        static_cast<float>(punctCount) / static_cast<float>(query.length());
    features.numericDensity = query.empty() ? 0.0f :
        static_cast<float>(numericCount) / static_cast<float>(query.length());
    features.questionStructure = questionCount > 0 ? 1.0f : 0.0f;

    return features;
}

// ---- Semantic grouping: cluster tokens by hash similarity ----
// Tokens whose hashes share bit patterns (high XOR similarity) are grouped.
static std::vector<std::vector<uint32_t>> clusterTokens(
    const std::vector<std::string>& tokens) {
    
    std::vector<std::vector<uint32_t>> clusters;
    if (tokens.empty()) return clusters;

    std::vector<bool> assigned(tokens.size(), false);

    for (uint32_t i = 0; i < static_cast<uint32_t>(tokens.size()); ++i) {
        if (assigned[i]) continue;

        std::vector<uint32_t> cluster;
        cluster.push_back(i);
        assigned[i] = true;

        uint64_t baseHash = fnv1a(tokens[i]);

        for (uint32_t j = i + 1; j < static_cast<uint32_t>(tokens.size()); ++j) {
            if (assigned[j]) continue;
            uint64_t otherHash = fnv1a(tokens[j]);

            // Bit-level similarity: count shared bits via popcount of ~(XOR)
            uint64_t xorResult = baseHash ^ otherHash;
            // Low popcount = high similarity
            int diffBits = 0;
            uint64_t temp = xorResult;
            while (temp) { diffBits++; temp &= (temp - 1); }

            // If hashes share > 50% of bits (< 32 differing bits out of 64), cluster together
            if (diffBits < 32) {
                cluster.push_back(j);
                assigned[j] = true;
            }
        }
        clusters.push_back(cluster);
    }

    return clusters;
}

ResearchPlanner::ResearchPlanner(ToolRegistry* registry)
    : toolRegistry_(registry) {}

std::vector<SubGoal> ResearchPlanner::decompose(const std::string& query) {
    return decomposeInternal(query);
}

std::vector<SubGoal> ResearchPlanner::decomposeInternal(const std::string& query) {
    std::vector<SubGoal> goals;
    uint64_t queryHash = fnv1a(query);

    // Tokenize and analyze
    auto tokens = tokenize(query);
    auto features = analyzeQuery(query, tokens);

    // Root goal: always present, represents the overall query
    SubGoal rootGoal;
    rootGoal.goalId = queryHash;
    rootGoal.descriptionHash = queryHash;
    rootGoal.confidence = 0.5f;
    goals.push_back(rootGoal);

    if (tokens.empty()) return goals;

    // Cluster tokens into semantic groups
    auto clusters = clusterTokens(tokens);

    // Each cluster becomes a SubGoal
    uint32_t goalIndex = 1;
    for (const auto& cluster : clusters) {
        if (goals.size() >= kMaxSubGoals) break;

        // Compute cluster hash by XOR-folding member hashes
        uint64_t clusterHash = 0;
        float clusterAvgLen = 0.0f;
        for (uint32_t idx : cluster) {
            clusterHash ^= fnv1a(tokens[idx]);
            clusterAvgLen += static_cast<float>(tokens[idx].length());
        }
        clusterAvgLen /= static_cast<float>(cluster.size());

        SubGoal sg;
        sg.goalId = queryHash ^ (clusterHash << (goalIndex % 16));
        sg.descriptionHash = clusterHash;

        // Confidence from structural features:
        // - Longer average tokens â†’ higher specificity â†’ higher confidence
        // - More tokens in cluster â†’ more evidence â†’ higher confidence
        // - Question structure â†’ exploration, slightly lower confidence
        float lengthFactor = std::min(1.0f, clusterAvgLen / 12.0f);
        float sizeFactor = std::min(1.0f, static_cast<float>(cluster.size()) / 5.0f);
        float entropyFactor = features.tokenEntropy;

        sg.confidence = (lengthFactor * 0.4f) + (sizeFactor * 0.3f) + (entropyFactor * 0.2f)
                      + (features.questionStructure * s_structureBonus);
        sg.confidence = std::clamp(sg.confidence, s_minGoalConf, s_maxGoalConf);

        // Root dependency
        sg.dependencies.push_back(rootGoal.goalId);

        // Schema requirements derived from cluster hash
        sg.requiredSchemaHashes.push_back(clusterHash);

        goals.push_back(sg);
        goalIndex++;
    }

    // Add cross-cluster dependency edges:
    // Later clusters may depend on earlier clusters if their hashes show relatedness
    for (size_t i = 2; i < goals.size(); ++i) {
        for (size_t j = 1; j < i; ++j) {
            uint64_t xorDist = goals[i].descriptionHash ^ goals[j].descriptionHash;
            int diffBits = 0;
            while (xorDist) { diffBits++; xorDist &= (xorDist - 1); }
            // If strongly related, add dependency
            if (diffBits < 24) {
                goals[i].dependencies.push_back(goals[j].goalId);
            }
        }
    }

    return goals;
}

std::vector<SubGoal> ResearchPlanner::detectGaps(const std::vector<SubGoal>& goals) {
    std::vector<SubGoal> result = goals;
    for (auto& goal : result) {
        if (goal.confidence < SubGoal::kDefaultMinConfThreshold) {
            goal.status = GoalStatus::NEEDS_RESEARCH;
        } else if (goal.confidence < (SubGoal::kDefaultMinConfThreshold + s_structureBonus)) {
            goal.status = GoalStatus::NEEDS_VERIFICATION;
        } else {
            goal.status = GoalStatus::SATISFIED;
        }
    }
    return result;
}

std::vector<std::vector<ToolPtr>> ResearchPlanner::matchTools(
    const std::vector<SubGoal>& goals) {
    std::vector<std::vector<ToolPtr>> matches;
    if (!toolRegistry_) return matches;

    for (const auto& goal : goals) {
        std::vector<ToolPtr> candidates;
        auto allTools = toolRegistry_->getAllTools();

        // Score each tool against the goal
        std::vector<std::pair<float, ToolPtr>> scored;
        for (const auto& tool : allTools) {
            float score = computeMatchScore(goal, tool->getMetadata());
            if (score >= kMinToolMatchScore) {
                scored.push_back({score, tool});
            }
        }

        // Sort by score descending, take best matches
        std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        for (const auto& [score, tool] : scored) {
            candidates.push_back(tool);
        }

        matches.push_back(candidates);
    }
    return matches;
}

float ResearchPlanner::computeMatchScore(const SubGoal& goal, 
                                          const ToolMetadata& meta) {
    // Schema overlap: check if tool output schema matches any required schema
    float schemaOverlap = 0.0f;
    for (uint64_t reqHash : goal.requiredSchemaHashes) {
        if (meta.schema.outputSchemaHash == reqHash) {
            schemaOverlap = 1.0f;
            break;
        }
        // Partial match: check bit similarity
        uint64_t xorDist = meta.schema.outputSchemaHash ^ reqHash;
        int diffBits = 0;
        uint64_t temp = xorDist;
        while (temp) { diffBits++; temp &= (temp - 1); }
        float partialMatch = 1.0f - (static_cast<float>(diffBits) / 64.0f);
        schemaOverlap = std::max(schemaOverlap, partialMatch);
    }

    // Goal confidence weighting: higher-confidence goals need stronger matches
    float confidenceWeight = goal.confidence;

    // Tool reliability
    float reliabilityScore = meta.reliability * s_reliabilityW;

    // Risk penalty
    float riskPenalty = 0.0f;
    switch (meta.riskLevel) {
        case ToolRiskLevel::NONE:     riskPenalty = 0.0f; break;
        case ToolRiskLevel::LOW:      riskPenalty = s_longTokenBonus; break;
        case ToolRiskLevel::MEDIUM:   riskPenalty = s_riskPenMedium; break;
        case ToolRiskLevel::HIGH:     riskPenalty = s_riskPenHigh; break;
        case ToolRiskLevel::CRITICAL: riskPenalty = s_riskPenCrit; break;
    }

    // Cost efficiency: lower cost = higher score
    float costPenalty = meta.cost > 0 ? 0.01f * static_cast<float>(meta.cost) : 0.0f;

    return (schemaOverlap * s_schemaWeight * confidenceWeight)
         + reliabilityScore
         - riskPenalty
         - costPenalty;
}

ResearchPlan ResearchPlanner::buildPlan(
    const std::vector<SubGoal>& goals,
    const std::vector<std::vector<ToolPtr>>& candidates) {

    ResearchPlan plan;
    plan.planId = fnv1a(std::to_string(goals.size()));

    uint64_t nodeId = 1;
    for (size_t i = 0; i < goals.size(); ++i) {
        PlanNode node;
        node.nodeId = nodeId++;
        node.associatedGoalId = goals[i].goalId;

        if (i < candidates.size() && !candidates[i].empty()) {
            node.type = NodeType::TOOL_EXECUTION;
            node.toolId = candidates[i][0]->getMetadata().toolId;
        } else {
            node.type = NodeType::HUMAN_CLARIFICATION;
            node.toolId = "";
        }

        // Wire dependency edges from SubGoal dependencies
        for (uint64_t dep : goals[i].dependencies) {
            for (const auto& n : plan.nodes) {
                if (n.associatedGoalId == dep) {
                    node.inputNodeIds.push_back(n.nodeId);
                }
            }
        }

        plan.nodes.push_back(node);
    }

    // Synthesis node: aggregates all tool execution results
    PlanNode synth;
    synth.nodeId = nodeId;
    synth.type = NodeType::SYNTHESIS;
    for (const auto& node : plan.nodes) {
        if (node.type == NodeType::TOOL_EXECUTION) {
            synth.inputNodeIds.push_back(node.nodeId);
        }
    }
    plan.nodes.push_back(synth);

    plan.buildWaves();
    return plan;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// ResearchPlan
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void ResearchPlan::buildWaves() {
    executionWaves.clear();
    std::unordered_set<uint64_t> completed;
    std::unordered_set<uint64_t> inPlan;

    for (const auto& node : nodes) {
        inPlan.insert(node.nodeId);
    }

    while (completed.size() < nodes.size()) {
        std::vector<uint64_t> wave;
        for (const auto& node : nodes) {
            if (completed.count(node.nodeId)) continue;

            bool depsSatisfied = true;
            for (uint64_t depId : node.inputNodeIds) {
                if (inPlan.count(depId) && !completed.count(depId)) {
                    depsSatisfied = false;
                    break;
                }
            }

            if (depsSatisfied) {
                wave.push_back(node.nodeId);
            }
        }

        if (wave.empty() && completed.size() < nodes.size()) {
            break;
        }

        for (uint64_t nid : wave) {
            completed.insert(nid);
        }
        executionWaves.push_back(wave);
    }
}

std::vector<uint64_t> ResearchPlan::getReadyNodes() const {
    std::vector<uint64_t> ready;
    std::unordered_set<uint64_t> executedIds;
    for (const auto& node : nodes) {
        if (node.executed) executedIds.insert(node.nodeId);
    }

    for (const auto& node : nodes) {
        if (node.executed) continue;
        bool canRun = true;
        for (uint64_t dep : node.inputNodeIds) {
            if (!executedIds.count(dep)) {
                canRun = false;
                break;
            }
        }
        if (canRun) ready.push_back(node.nodeId);
    }
    return ready;
}

bool ResearchPlan::isComplete() const {
    for (const auto& node : nodes) {
        if (!node.executed && node.type != NodeType::HUMAN_CLARIFICATION) {
            return false;
        }
    }
    return true;
}

ResearchPlan ResearchPlanner::research(const std::string& topic) {
    auto goals = decompose(topic);
    auto gaps = detectGaps(goals);
    auto tool_matches = matchTools(gaps.empty() ? goals : gaps);
    auto plan = buildPlan(goals, tool_matches);
    plan.buildWaves();

    // Trigger tool execution for matched web search / scrape tools if registered
    if (toolRegistry_) {
        for (const auto& wave : plan.executionWaves) {
            for (uint64_t nodeId : wave) {
                for (auto& node : plan.nodes) {
                    if (node.nodeId == nodeId && !node.executed) {
                        SubGoal tempGoal{node.associatedGoalId, 0, {}, 0.5f, {}, false, GoalStatus::NEEDS_RESEARCH};
                        auto tool_match_lists = matchTools({tempGoal});
                        if (!tool_match_lists.empty()) {
                            for (auto& tool : tool_match_lists[0]) {
                                if (tool) {
                                    std::string queryStr = "{\"query\":\"" + topic + "\"}";
                                    std::vector<uint8_t> payload(queryStr.begin(), queryStr.end());
                                    tool->execute(payload);
                                    node.executed = true;
                                    node.lastStatus = ToolStatus::SUCCESS;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return plan;
}

} // namespace research
} // namespace yuki


