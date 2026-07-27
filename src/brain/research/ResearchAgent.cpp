#include "brain/research/ResearchAgent.h"
#include "brain/research/core/Synthesizer.h"

namespace yuki {
namespace research {

// ---- Constants ----
static constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime       = 0x100000001b3ULL;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

ResearchAgent::ResearchAgent(ToolRegistry* registry,
                              security::SecuritySandbox* sandbox)
    : registry_(registry) {
    planner_ = std::make_unique<ResearchPlanner>(registry);
    riskGate_ = std::make_unique<RiskGate>(sandbox);
}

KnowledgePack ResearchAgent::research(const ResearchRequest& request) {
    auto goals = planner_->decompose(request.query);
    goals = planner_->detectGaps(goals);
    auto candidates = planner_->matchTools(goals);

    auto plan = planner_->buildPlan(goals, candidates);
    plan.parentRequestId = request.requestId;

    auto validation = riskGate_->validatePlan(plan);
    if (validation == ValidationResult::DEFER || 
        validation == ValidationResult::BLOCK) {
        KnowledgePack pack;
        pack.overallConfidence = 0.0f;
        pack.gaps.push_back(0);
        return pack;
    }

    auto results = executePlan(plan);

    Synthesizer synth;
    auto pack = synth.synthesize(goals, results);
    pack.parentRequestId = request.requestId;

    return pack;
}

std::vector<ToolResult> ResearchAgent::executePlan(const ResearchPlan& plan) {
    std::vector<ToolResult> allResults;

    for (const auto& wave : plan.executionWaves) {
        std::vector<ToolResult> waveResults;
        for (uint64_t nodeId : wave) {
            for (const auto& node : plan.nodes) {
                if (node.nodeId == nodeId) {
                    auto result = executeNode(node);
                    result.nodeId = node.associatedGoalId;
                    waveResults.push_back(result);
                    break;
                }
            }
        }
        allResults.insert(allResults.end(), waveResults.begin(), waveResults.end());
    }

    return allResults;
}

ToolResult ResearchAgent::executeNode(const PlanNode& node) {
    ToolResult result;
    if (node.type == NodeType::HUMAN_CLARIFICATION) {
        result.status = ToolStatus::PERMISSION_DENIED;
        return result;
    }

    auto tool = registry_->getTool(node.toolId);
    if (!tool) {
        result.status = ToolStatus::UNKNOWN_ERROR;
        return result;
    }

    for (uint32_t attempt = 0; attempt <= node.maxRetries; ++attempt) {
        result = tool->execute({});
        if (result.isSuccess()) break;
        result.retryCount = attempt + 1;
    }

    return result;
}

bool ResearchAgent::consume(const metacognition::ActionableHypothesis& hypothesis) {
    if (hypothesis.symptom == metacognition::SymptomCode::NONE) {
        return false;
    }

    // Build research request dynamically from hypothesis structure.
    // Query is derived from the hypothesis hash signature — no hardcoded strings.
    ResearchRequest req;
    req.requestId = hypothesis.trigger_audit_id;

    // Construct query hash from hypothesis fields:
    // symptom code + target module + target domain → unique research fingerprint
    uint64_t queryHash = kFnvOffsetBasis;
    queryHash ^= static_cast<uint64_t>(hypothesis.symptom);
    queryHash *= kFnvPrime;
    queryHash ^= static_cast<uint64_t>(hypothesis.target_module_id);
    queryHash *= kFnvPrime;
    queryHash ^= static_cast<uint64_t>(hypothesis.target_domain);
    queryHash *= kFnvPrime;

    // Encode the query hash as a hex string — machine-readable, not human-readable
    char hexBuf[17] = {};
    for (int i = 15; i >= 0; --i) {
        uint8_t nibble = static_cast<uint8_t>(queryHash & 0xF);
        hexBuf[i] = nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('a' + nibble - 10);
        queryHash >>= 4;
    }
    req.query = std::string(hexBuf, 16);

    // Set minimum confidence from hypothesis confidence
    req.minConfidence = hypothesis.action_confidence;

    // Add required schema hashes based on symptom type
    switch (hypothesis.symptom) {
        case metacognition::SymptomCode::KNOWLEDGE_GAP:
            req.requiredSchemaHashes.push_back(fnv1a("knowledge"));
            req.requiredSchemaHashes.push_back(fnv1a("documentation"));
            break;
        case metacognition::SymptomCode::FEATURE_STAGNATION:
            req.requiredSchemaHashes.push_back(fnv1a("training_data"));
            req.requiredSchemaHashes.push_back(fnv1a("model_weights"));
            break;
        case metacognition::SymptomCode::COMPETENCE_DEGRADATION:
            req.requiredSchemaHashes.push_back(fnv1a("performance_metrics"));
            break;
        case metacognition::SymptomCode::PERFORMANCE_DEGRADATION:
            req.requiredSchemaHashes.push_back(fnv1a("variance_analysis"));
            break;
        case metacognition::SymptomCode::RISK_ESCALATION:
            req.requiredSchemaHashes.push_back(fnv1a("risk_assessment"));
            break;
        default:
            req.requiredSchemaHashes.push_back(fnv1a("general"));
            break;
    }

    pendingCount_++;
    research(req);
    pendingCount_--;
    completedCount_++;

    return true;
}

} // namespace research
} // namespace yuki
