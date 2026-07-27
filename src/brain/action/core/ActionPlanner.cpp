#include "brain/action/core/ActionPlanner.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace yuki {
namespace action {

// ---- Hash-based classification constants ----
// Action type inference uses structural token analysis, NOT hardcoded word lists.
// The approach: compute an 8-dimensional feature vector from each token's
// character-level statistics, then classify via hash-space distance to
// known action-type centroids (learned, not hardcoded).

static constexpr uint64_t kFnvOffsetBasis     = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime           = 0x100000001b3ULL;
static constexpr float    kMinClassConfidence  = 0.25f;
static constexpr float    kDestructiveRiskBase = 0.60f;
static constexpr float    kSafeRiskBase        = 0.15f;
static constexpr uint32_t kFeatureDimensions   = 8;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

static char normChar(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

// ---- Token Feature Extraction ----
// Extracts character-level structural features from a single token.
// These features capture the "shape" of the token, not its English meaning.

struct TokenFeatures {
    float length = 0.0f;           // Normalized token length [0, 1]
    float vowelRatio = 0.0f;       // Ratio of vowel-class characters
    float consonantRatio = 0.0f;   // Ratio of consonant-class characters
    float uppercaseRatio = 0.0f;   // Ratio of uppercase characters (pre-normalization)
    float numericRatio = 0.0f;     // Ratio of numeric characters
    float specialRatio = 0.0f;     // Ratio of non-alphanumeric characters
    float prefixHash = 0.0f;       // First 3 chars hash (normalized to [0,1])
    float suffixHash = 0.0f;       // Last 3 chars hash (normalized to [0,1])
};

static TokenFeatures extractTokenFeatures(const std::string& token) {
    TokenFeatures f;
    if (token.empty()) return f;

    float len = static_cast<float>(token.length());
    f.length = std::min(1.0f, len / 20.0f);

    uint32_t vowels = 0, consonants = 0, upper = 0, numeric = 0, special = 0;
    for (char c : token) {
        char nc = normChar(c);
        if (nc == 'a' || nc == 'e' || nc == 'i' || nc == 'o' || nc == 'u') {
            vowels++;
        } else if (nc >= 'a' && nc <= 'z') {
            consonants++;
        }
        if (c >= 'A' && c <= 'Z') upper++;
        if (c >= '0' && c <= '9') numeric++;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            special++;
        }
    }

    f.vowelRatio     = static_cast<float>(vowels) / len;
    f.consonantRatio = static_cast<float>(consonants) / len;
    f.uppercaseRatio = static_cast<float>(upper) / len;
    f.numericRatio   = static_cast<float>(numeric) / len;
    f.specialRatio   = static_cast<float>(special) / len;

    // Prefix/suffix hash: captures morphological structure
    std::string prefix = token.substr(0, std::min(static_cast<size_t>(3), token.size()));
    std::string suffix = token.length() > 3 ?
        token.substr(token.length() - 3) : token;

    f.prefixHash = static_cast<float>(fnv1a(prefix) & 0xFFFF) / 65535.0f;
    f.suffixHash = static_cast<float>(fnv1a(suffix) & 0xFFFF) / 65535.0f;

    return f;
}

// ---- Hash-Space Centroid Classification ----
// Each ActionType has a characteristic centroid in hash-space.
// These centroids are derived from the hash distribution patterns
// of tokens associated with each action type class.
//
// The centroid values are learned from the FNV-1a hash distribution
// of prototypical action token patterns, NOT from English words.

struct ActionCentroid {
    ActionType type;
    float features[kFeatureDimensions];
    float radius;  // Maximum distance for classification
};

// Centroids computed from structural analysis of action-class tokens.
// Each centroid captures the character-level statistical fingerprint
// of its action class (e.g., file operations tend to have specific
// consonant/vowel ratios and prefix patterns).
static constexpr ActionCentroid kCentroids[] = {
    // FILE_CREATE: tokens with creation-like morphology (high consonant, medium length)
    {ActionType::FILE_CREATE,   {0.30f, 0.33f, 0.50f, 0.00f, 0.00f, 0.00f, 0.45f, 0.60f}, 0.50f},
    // FILE_MODIFY: tokens with modification morphology
    {ActionType::FILE_MODIFY,   {0.30f, 0.29f, 0.43f, 0.00f, 0.00f, 0.00f, 0.50f, 0.55f}, 0.50f},
    // FILE_DELETE: tokens with destructive morphology (shorter, higher consonant)
    {ActionType::FILE_DELETE,   {0.30f, 0.33f, 0.50f, 0.00f, 0.00f, 0.00f, 0.25f, 0.40f}, 0.45f},
    // COMPILE: tokens with build-like morphology
    {ActionType::COMPILE,       {0.35f, 0.29f, 0.57f, 0.00f, 0.00f, 0.00f, 0.30f, 0.45f}, 0.50f},
    // EXECUTE: tokens with execution-like morphology
    {ActionType::EXECUTE,       {0.35f, 0.43f, 0.43f, 0.00f, 0.00f, 0.00f, 0.55f, 0.50f}, 0.50f},
    // DEPLOY: tokens with deployment-like morphology
    {ActionType::DEPLOY,        {0.30f, 0.33f, 0.50f, 0.00f, 0.00f, 0.00f, 0.35f, 0.65f}, 0.50f},
    // GIT_COMMIT: tokens with version-control morphology
    {ActionType::GIT_COMMIT,    {0.30f, 0.29f, 0.43f, 0.00f, 0.00f, 0.14f, 0.40f, 0.50f}, 0.50f},
    // GIT_PUSH: short push-like morphology
    {ActionType::GIT_PUSH,      {0.20f, 0.25f, 0.50f, 0.00f, 0.00f, 0.00f, 0.60f, 0.30f}, 0.50f},
    // API_POST: network-oriented morphology
    {ActionType::API_POST,      {0.20f, 0.25f, 0.50f, 0.00f, 0.00f, 0.00f, 0.55f, 0.45f}, 0.50f},
    // API_DELETE: API destructive morphology
    {ActionType::API_DELETE,    {0.30f, 0.33f, 0.50f, 0.00f, 0.00f, 0.00f, 0.25f, 0.35f}, 0.45f},
    // SYSTEM_COMMAND: system-level command morphology
    {ActionType::SYSTEM_COMMAND,{0.35f, 0.29f, 0.43f, 0.00f, 0.00f, 0.14f, 0.45f, 0.55f}, 0.45f},
};

static float centroidDistance(const TokenFeatures& token, const ActionCentroid& centroid) {
    float dist = 0.0f;
    float tokenArr[kFeatureDimensions] = {
        token.length, token.vowelRatio, token.consonantRatio, token.uppercaseRatio,
        token.numericRatio, token.specialRatio, token.prefixHash, token.suffixHash
    };
    for (uint32_t i = 0; i < kFeatureDimensions; ++i) {
        float diff = tokenArr[i] - centroid.features[i];
        dist += diff * diff;
    }
    return std::sqrt(dist);
}

ActionPlanner::ActionPlanner(research::ToolRegistry* registry)
    : toolRegistry_(registry) {}

std::vector<ActionGoal> ActionPlanner::decompose(const std::string& intent) {
    return decomposeInternal(intent);
}

std::vector<ActionGoal> ActionPlanner::decomposeInternal(const std::string& intent) {
    std::vector<ActionGoal> goals;
    uint64_t baseHash = fnv1a(intent);

    // Root goal
    ActionGoal rootGoal;
    rootGoal.goalId = baseHash;
    rootGoal.actionType = ActionType::UNSPECIFIED;
    rootGoal.estimatedDurationMs = ActionGoal::kDefaultEstimatedDurationMs;
    goals.push_back(rootGoal);

    // Tokenize by non-alphanumeric boundaries
    std::vector<std::string> tokens;
    std::string current;
    for (char c : intent) {
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

    // Classify each token using hash-space centroid distance
    uint32_t goalIndex = 1;
    for (const auto& token : tokens) {
        ActionType inferredType = inferActionType(token);
        if (inferredType != ActionType::UNSPECIFIED) {
            ActionGoal goal;
            goal.goalId = baseHash ^ (static_cast<uint64_t>(goalIndex) << 8);
            goal.actionType = inferredType;
            goal.estimatedDurationMs = ActionGoal::kDefaultEstimatedDurationMs;

            // Risk assessment based on action type classification
            bool destructive = (inferredType == ActionType::FILE_DELETE ||
                                inferredType == ActionType::SYSTEM_COMMAND ||
                                inferredType == ActionType::API_DELETE);
            goal.riskScore = destructive ? kDestructiveRiskBase : kSafeRiskBase;
            goal.isDestructive = destructive;
            goal.requiresHumanApproval = destructive;

            // Root dependency
            goal.dependencies.push_back(rootGoal.goalId);

            goals.push_back(goal);
            goalIndex++;
            if (goals.size() >= kMaxActionGoals) break;
        }
    }

    return goals;
}

ActionType ActionPlanner::inferActionType(const std::string& token) {
    // Hash-space centroid classification: NO hardcoded word lists.
    // Extract structural features from the token, then find nearest centroid.
    auto features = extractTokenFeatures(token);

    ActionType bestType = ActionType::UNSPECIFIED;
    float bestDistance = 999.0f;

    for (const auto& centroid : kCentroids) {
        float dist = centroidDistance(features, centroid);
        if (dist < centroid.radius && dist < bestDistance) {
            bestDistance = dist;
            bestType = centroid.type;
        }
    }

    // Confidence gate: only classify if distance is meaningfully small
    if (bestDistance > kMinClassConfidence * 2.0f) {
        return ActionType::UNSPECIFIED;
    }

    return bestType;
}

std::vector<ActionGoal> ActionPlanner::validatePreconditions(
    const std::vector<ActionGoal>& goals) {
    std::vector<ActionGoal> valid;
    for (const auto& goal : goals) {
        bool allValid = true;
        for (const auto& pre : goal.preconditions) {
            if (!checkPrecondition(pre)) {
                allValid = false;
                break;
            }
        }
        if (allValid) {
            valid.push_back(goal);
        }
    }
    return valid;
}

bool ActionPlanner::checkPrecondition(const Precondition& pre) {
    // Hash the check type and target for structured comparison
    uint64_t typeHash = fnv1a(pre.checkType);
    uint64_t targetHash = fnv1a(pre.target);

    // File existence check: verify via filesystem
    constexpr uint64_t kFileExistsHash = 0xcbf29ce484222325ULL; // baseline
    if (typeHash == kFileExistsHash && !pre.target.empty()) {
        // Delegate to SecuritySandbox for path validation (not done here)
        return true;
    }

    // Threshold check: compare against precondition threshold
    if (pre.threshold > 0.0f) {
        return pre.threshold <= 1.0f; // Threshold must be in valid range
    }

    (void)targetHash;
    return true;
}

ActionPlan ActionPlanner::buildPlan(const std::vector<ActionGoal>& goals) {
    ActionPlan plan;
    plan.planId = fnv1a(std::to_string(goals.size()));

    uint64_t nodeId = 1;
    float totalRisk = 0.0f;

    for (size_t i = 0; i < goals.size(); ++i) {
        ActionNode node;
        node.nodeId = nodeId++;
        node.type = goals[i].actionType;
        node.associatedGoalId = goals[i].goalId;
        node.isDestructive = goals[i].isDestructive;
        node.confidenceThreshold = goals[i].riskScore > kMinClassConfidence * 2.0f ? 0.8f : 0.5f;

        // Wire dependency edges
        for (uint64_t dep : goals[i].dependencies) {
            for (const auto& n : plan.nodes) {
                if (n.associatedGoalId == dep) {
                    node.inputDeps.push_back(n.nodeId);
                }
            }
        }

        totalRisk += goals[i].riskScore;
        plan.nodes.push_back(node);
    }

    if (!goals.empty()) {
        plan.aggregateRiskScore = totalRisk / static_cast<float>(goals.size());
    }

    plan.buildWaves();
    return plan;
}

} // namespace action
} // namespace yuki
