#include <iostream>
#include <cassert>
#include <random>
#include <string>
#include <vector>

#include "src/input/InputAnalyzer.h"
#include "src/brain/autonomy/AutonomyKernel.h"
#include "src/brain/autonomy/RequirementGraph.h"
#include "src/brain/autonomy/BeliefLedger.h"
#include "src/brain/autonomy/HypothesisEngine.h"
#include "src/brain/autonomy/FuturePossibilityRegistry.h"
#include "src/brain/autonomy/OwnerIntentArbiter.h"
#include "src/brain/autonomy/AgentSpawner.h"
#include "src/brain/autonomy/WatchdogSupervisor.h"
#include "src/brain/autonomy/ExperimentRegistry.h"
#include "src/brain/autonomy/EvolutionLedger.h"
#include "src/brain/autonomy/PromotionGovernor.h"
#include "src/brain/autonomy/DynamicPromptDirector.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"
#include "src/brain/platform/BackendSelector.h"
#include "src/brain/platform/PortabilityLayer.h"
#include "src/brain/language/DistillationExtractor.h"
#include "src/brain/language/LocalTransformer.h"
#include "src/brain/research/tools/GitHubSearchTool.h"
#include "src/brain/research/tools/GitHubReadTool.h"
#include "src/brain/research/tools/APICallTool.h"
#include "src/brain/research/tools/FileReadTool.h"
#include "src/brain/research/tools/ComputeTool.h"

using namespace yuki;
using namespace yuki::input;
using namespace yuki::autonomy;
using namespace yuki::platform;
using namespace yuki::language;
using namespace yuki::research;

static void test_input_analyzer_autonomy_signals() {
    InputAnalyzer analyzer;
    AnalyzedInput cmd = analyzer.analyze("run system diagnostic and compile patch");
    assert(cmd.autonomyEligible == true);
    assert(cmd.ownerDirectiveStrength >= 0.50f);

    AnalyzedInput question = analyzer.analyze("what is quantum computing?");
    assert(question.cognitiveIntent == CognitiveIntent::QUESTION || question.cognitiveIntent == CognitiveIntent::DEFINITION);

    std::cout << "[PASS] Test 1: InputAnalyzer Autonomy Signals\n";
}

static void test_autonomy_kernel_task_scoring() {
    AutonomyKernel kernel;
    kernel.initialize();

    kernel.enqueueOwnerDirective("Owner high priority directive");
    
    AutonomyTask bgTask;
    bgTask.taskId = "sys_task_1";
    bgTask.source = "system";
    bgTask.goalText = "Background maintenance";
    bgTask.ownerPriority = 0.1f;
    bgTask.urgency = 0.5f;
    bgTask.expectedValue = 0.4f;
    bgTask.confidence = 0.9f;
    kernel.enqueueSystemNeed(bgTask);

    assert(kernel.hasPendingTasks());
    auto queue = kernel.buildTaskQueue();
    assert(!queue.empty());
    assert(queue.front().source == "owner");

    AutonomyTask next = kernel.selectNextTask();
    assert(kernel.executeTask(next));

    std::cout << "[PASS] Test 2: AutonomyKernel Task Scoring & Execution\n";
}

static void test_requirement_graph_kahn_sort() {
    RequirementGraph graph;
    RequirementNode n1{"g1", RequirementNodeType::GOAL, "Main Goal", "", 1.0f, false, {}};
    RequirementNode n2{"c1", RequirementNodeType::CONSTRAINT, "Safety Constraint", "", 1.0f, false, {}};
    RequirementNode n3{"r1", RequirementNodeType::RESOURCE, "Disk Space", "", 1.0f, false, {}};

    graph.addNode(n1);
    graph.addNode(n2);
    graph.addNode(n3);

    graph.addEdge("c1", "g1");
    graph.addEdge("r1", "c1");

    assert(!graph.hasCycles());
    auto order = graph.topologicalOrder();
    assert(order.size() == 3);
    assert(order[0].nodeId == "r1");
    assert(order[1].nodeId == "c1");
    assert(order[2].nodeId == "g1");

    std::cout << "[PASS] Test 3: RequirementGraph Kahn Topological Sort\n";
}

static void test_belief_ledger_evidence_blending() {
    BeliefLedger ledger;
    BeliefRecord prior;
    prior.beliefId = "b1";
    prior.subject = "YUKI";
    prior.relation = "supports";
    prior.object = "autonomy";
    prior.confidence = 0.50f;
    prior.status = BeliefStatus::HYPOTHESIS;

    BeliefRecord updated = ledger.updateFromEvidence(prior, 0.90f, 0.85f, 0.95f, 0.80f, false);
    assert(updated.confidence > 0.50f);
    assert(updated.status == BeliefStatus::LIKELY || updated.status == BeliefStatus::VERIFIED);

    std::cout << "[PASS] Test 4: BeliefLedger Evidence Blending\n";
}

static void test_hypothesis_engine_ranking() {
    HypothesisEngine engine;
    auto h1 = engine.generateHypothesis("Memory leak in buffer", "Memory", 0.8f, 0.7f, 0.9f, 0.5f);
    auto h2 = engine.generateHypothesis("Minor log typo", "Logger", 0.2f, 0.1f, 0.9f, 0.1f);

    std::vector<HypothesisRecord> list = {h2, h1};
    auto ranked = engine.rankHypotheses(list);
    assert(ranked.front().hypothesisId == h1.hypothesisId);

    std::cout << "[PASS] Test 5: HypothesisEngine Signature Hashing & Ranking\n";
}

static void test_owner_intent_arbiter() {
    OwnerIntentArbiter arbiter;
    auto comply = arbiter.decide(true, true, true, false, false);
    assert(comply.mode == OwnerDecisionMode::COMPLY);

    auto alt = arbiter.decide(true, false, false, true, false);
    assert(alt.mode == OwnerDecisionMode::SAFE_ALTERNATIVE);

    auto defer = arbiter.decide(true, false, false, false, true);
    assert(defer.mode == OwnerDecisionMode::DEFER_BUILD_PATH);

    auto decline = arbiter.decide(false, false, false, false, false);
    assert(decline.mode == OwnerDecisionMode::DECLINE);

    std::cout << "[PASS] Test 6: OwnerIntentArbiter Decision Matrix\n";
}

static void test_backend_selector() {
    using yuki::brain::platform::BackendSelector;
    using yuki::brain::platform::BackendSelectionInput;
    using yuki::platform::DeviceProfile;
    using yuki::platform::DeviceTier;
    using yuki::brain::language::BackendKind;

    BackendSelector selector;
    BackendSelectionInput in1;
    in1.deviceProfile.tier = DeviceTier::MID;
    in1.localConfidence = 0.85f;
    in1.selfEvalScore = 0.80f;
    in1.riskScore = 0.20f;
    in1.localBackendAvailable = true;
    in1.externalBackendAvailable = true;

    auto kind1 = selector.select(in1);
    assert(kind1 == BackendKind::LOCAL_TRANSFORMER);

    BackendSelectionInput in2;
    in2.deviceProfile.tier = DeviceTier::MID;
    in2.localConfidence = 0.40f;
    in2.selfEvalScore = 0.40f;
    in2.riskScore = 0.80f;
    in2.localBackendAvailable = true;
    in2.externalBackendAvailable = true;

    auto kind2 = selector.select(in2);
    assert(kind2 == BackendKind::EXTERNAL_LLM);

    std::cout << "[PASS] Test 7: BackendSelector Multi-Tier Routing\n";
}


static void test_watchdog_supervisor() {
    WatchdogSupervisor watchdog;
    auto alertLoop = watchdog.checkBehaviorLoopRate(10);
    assert(alertLoop.level == WatchdogAlertLevel::CRITICAL);

    auto alertBlast = watchdog.checkCodeDiffBlastRadius(600, 12);
    assert(alertBlast.level == WatchdogAlertLevel::CRITICAL);

    std::cout << "[PASS] Test 8: WatchdogSupervisor Safety Alerts\n";
}

static void test_promotion_governor() {
    PromotionGovernor governor;
    PromotionCriteria c1;
    c1.compileSuccess = true;
    c1.testFailures = 0;
    c1.benchmarkRegression = false;
    c1.maxWatchdogAlert = WatchdogAlertLevel::NONE;
    c1.integritySealed = true;
    c1.approvalGranted = true;

    std::string reason;
    assert(governor.verifyPromotion(c1, reason));

    PromotionCriteria c2 = c1;
    c2.testFailures = 2;
    assert(!governor.verifyPromotion(c2, reason));

    std::cout << "[PASS] Test 9: PromotionGovernor Verification Matrix\n";
}

static void test_research_tools() {
    GitHubSearchTool searchTool;
    GitHubReadTool readTool;
    APICallTool apiTool;
    FileReadTool fileTool;
    ComputeTool computeTool;

    assert(searchTool.isAvailable());
    assert(readTool.isAvailable());
    assert(apiTool.isAvailable());
    assert(fileTool.isAvailable());
    assert(computeTool.isAvailable());

    std::string query = "test_query";
    std::vector<uint8_t> input(query.begin(), query.end());

    auto res = computeTool.execute(input);
    assert(res.isSuccess());

    std::cout << "[PASS] Test 10: Research Tools Execution\n";
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "  YUKI FULL AUTONOMY INTEGRATION TEST SUITE (C++20) \n";
    std::cout << "====================================================\n\n";

    test_input_analyzer_autonomy_signals();
    test_autonomy_kernel_task_scoring();
    test_requirement_graph_kahn_sort();
    test_belief_ledger_evidence_blending();
    test_hypothesis_engine_ranking();
    test_owner_intent_arbiter();
    test_backend_selector();
    test_watchdog_supervisor();
    test_promotion_governor();
    test_research_tools();

    std::cout << "\n====================================================\n";
    std::cout << "  ALL 10 FULL AUTONOMY INTEGRATION TESTS PASSED!  \n";
    std::cout << "====================================================\n";

    return 0;
}
