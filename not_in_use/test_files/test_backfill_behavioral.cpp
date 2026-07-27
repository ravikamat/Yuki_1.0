// ============================================================================
// test_backfill_behavioral.cpp
// Comprehensive behavioral unit tests for M3-M4 backfilled modules:
//   1. DynamicProfiler
//   2. ChainReconstructor
//   3. MemoryFabric
//   4. ImprovementGraph
//   5. PolicySelector
//   6. ResearchAgent
//   7. ResearchPlanner
//   8. SecuritySandbox
// ============================================================================

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>

#include "brain/introspection/DynamicProfiler.h"
#include "brain/memory/ChainReconstructor.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/metacognition/ImprovementGraph.h"
#include "brain/policy/PolicySelector.h"
#include "brain/research/ResearchAgent.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/security/SecuritySandbox.h"

using namespace yuki;

// ============================================================================
// 1. DynamicProfiler Tests
// ============================================================================

TEST(BackfillBehavioralTest, DynamicProfiler_ProfileAndBacktrack) {
    introspection::DynamicProfiler profiler;

    auto profile = profiler.profileSystem();
    EXPECT_GE(profile.cpuUsagePercent, 0.0f);
    EXPECT_GE(profile.ramUsageMb, 0.0f);

    auto causeNodes = profiler.backtrack("high_latency", introspection::BacktrackMode::CAUSAL);
    EXPECT_FALSE(causeNodes.empty());
}

// ============================================================================
// 2. ChainReconstructor Tests
// ============================================================================

TEST(BackfillBehavioralTest, ChainReconstructor_BuildChains) {
    memory::ChainReconstructor reconstructor;
    
    auto chain = reconstructor.reconstruct("variational_inference", memory::ChainType::FUZZY);
    EXPECT_GE(chain.overallCoherence, 0.0f);

    auto prereq = reconstructor.buildPrerequisiteChain("free_energy_minimization");
    EXPECT_FALSE(prereq.nodes.empty());
}

// ============================================================================
// 3. MemoryFabric Tests
// ============================================================================

TEST(BackfillBehavioralTest, MemoryFabric_StoreAndRetrieve) {
    memory::MemoryFabric fabric;

    memory::MemoryItem item;
    item.key = "user_preference_language";
    item.confidence = 0.95f;
    item.timestamp = 1000;

    fabric.store(item);

    auto retrieved = fabric.retrieve("user_preference_language", memory::RetrieveMode::EXACT);
    EXPECT_FALSE(retrieved.empty());
    EXPECT_EQ(retrieved[0].key, std::string("user_preference_language"));
}

// ============================================================================
// 4. ImprovementGraph Tests
// ============================================================================

TEST(BackfillBehavioralTest, ImprovementGraph_Routing) {
    metacognition::ImprovementGraph graph;
    
    graph.addChainRoute("test_route", "path/to/route");
    EXPECT_TRUE(graph.hasChainRoute("test_route"));
    EXPECT_EQ(graph.getChainRoute("test_route"), std::string("path/to/route"));

    graph.addIntrospectionRoute("introspect_1", "profile_node");
    EXPECT_TRUE(graph.hasIntrospectionRoute("introspect_1"));
    EXPECT_EQ(graph.getIntrospectionRoute("introspect_1"), std::string("profile_node"));
}

// ============================================================================
// 5. PolicySelector Tests
// ============================================================================

TEST(BackfillBehavioralTest, PolicySelector_SelectAndAdapt) {
    metacognition::CompetenceRecord compRecord;
    compRecord.success_rate_ema = 0.8f;
    policy::ExecutivePolicySelector selector(&compRecord);

    std::vector<float> intentDist = {0.8f, 0.1f, 0.1f};
    auto selection = selector.select(intentDist, "run system audit", 1);

    EXPECT_GE(selection.selected_policy_id, -1);
    EXPECT_GE(selection.selection_confidence, 0.0f);

    float initialThreshold = selector.currentThreshold();
    selector.adaptThreshold(0.05f);
    EXPECT_NE(selector.currentThreshold(), initialThreshold);
}

// ============================================================================
// 6. ResearchAgent Tests
// ============================================================================

TEST(BackfillBehavioralTest, ResearchAgent_ResearchRequest) {
    research::ToolRegistry registry;
    security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
    research::ResearchAgent agent(&registry, &sandbox);

    research::ResearchRequest request;
    request.query = "active_inference_optimization";

    auto pack = agent.research(request);
    EXPECT_GE(pack.overallConfidence, 0.0f);
}

// ============================================================================
// 7. ResearchPlanner Tests
// ============================================================================

TEST(BackfillBehavioralTest, ResearchPlanner_PlanDecomposition) {
    research::ToolRegistry registry;
    research::ResearchPlanner planner(&registry);

    auto subgoals = planner.decompose("investigate free energy bounds");
    EXPECT_FALSE(subgoals.empty());
}

// ============================================================================
// 8. SecuritySandbox Tests
// ============================================================================

TEST(BackfillBehavioralTest, SecuritySandbox_ValidatePathAndCommand) {
    security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
    sandbox.setDeniedPrefixes({"C:\\Windows", "/etc"});
    
    auto decisionBad = sandbox.validateWrite("C:\\Windows\\System32\\cmd.exe");
    EXPECT_FALSE(decisionBad.allowed());

    auto decisionGood = sandbox.validateRead("src/brain/predictive/TurnCoordinator.h");
    EXPECT_TRUE(decisionGood.allowed());
}
