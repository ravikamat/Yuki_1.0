#include "brain/capability/CapabilityGraph.h"
#include "brain/capability/CapabilityMatcher.h"
#include "brain/capability/PathFinder.h"
#include "brain/capability/ResourceOptimizer.h"
#include "brain/capability/SequencingEngine.h"
#include "brain/system/ResourceMonitor.h"
#include <iostream>
#include <cassert>

using namespace yuki::capability;

void testProfileSerialization() {
    CapabilityProfile prof;
    prof.tool_id = "test_tool";
    prof.inputs = {"code", "config"};
    prof.outputs = {"binary", "log"};
    prof.avg_duration_ms = 1500.0f;
    prof.avg_ram_mb = 256.0f;
    prof.avg_cpu_percent = 25.0f;
    prof.base_risk = 0.2f;
    prof.required_competence = 0.8f;
    prof.platform_tags = {"windows", "linux"};
    prof.produces_artifacts = true;
    prof.is_destructive = false;

    auto serialized = prof.serialize();
    assert(!serialized.empty());

    auto deserialized = CapabilityProfile::deserialize(serialized);
    assert(deserialized.has_value());
    const auto& p = deserialized.value();

    assert(p.tool_id == "test_tool");
    assert(p.inputs.size() == 2);
    assert(p.outputs.size() == 2);
    assert(p.avg_duration_ms == 1500.0f);
    assert(p.avg_ram_mb == 256.0f);
    assert(p.produces_artifacts == true);
    assert(p.is_destructive == false);

    std::cout << "[PASS] testProfileSerialization" << std::endl;
}

void testGraphOperations() {
    CapabilityGraph graph;

    CapabilityProfile p1;
    p1.tool_id = "compiler";
    p1.inputs = {"cpp_source"};
    p1.outputs = {"obj_file"};
    p1.avg_duration_ms = 500.0f;
    p1.avg_ram_mb = 128.0f;

    CapabilityProfile p2;
    p2.tool_id = "linker";
    p2.inputs = {"obj_file"};
    p2.outputs = {"exe_binary"};
    p2.avg_duration_ms = 300.0f;
    p2.avg_ram_mb = 64.0f;

    uint32_t id1 = graph.registerTool("compiler", p1, "C++ Compiler Tool");
    uint32_t id2 = graph.registerTool("linker", p2, "Executable Linker Tool");
    uint32_t id3 = graph.registerGoal("Build Executable", {"exe_binary"});

    assert(id1 == 1);
    assert(id2 == 2);
    assert(id3 == 3);
    assert(graph.nodeCount() == 3);

    graph.autoBuildEdges();
    assert(graph.edgeCount() >= 1);

    auto neighbors = graph.getNeighbors(id1);
    assert(!neighbors.empty());
    assert(neighbors[0].to_node == id2);

    auto ser = graph.serialize();
    assert(!ser.empty());

    CapabilityGraph graph2;
    bool ok = graph2.deserialize(ser);
    assert(ok);
    assert(graph2.nodeCount() == 3);
    assert(graph2.edgeCount() == graph.edgeCount());

    std::cout << "[PASS] testGraphOperations" << std::endl;
}

void testMatcherAndPathFinder() {
    CapabilityGraph graph;

    CapabilityProfile p1;
    p1.tool_id = "source_gen";
    p1.inputs = {};
    p1.outputs = {"cpp_code"};
    p1.required_competence = 0.5f;

    CapabilityProfile p2;
    p2.tool_id = "build_tool";
    p2.inputs = {"cpp_code"};
    p2.outputs = {"executable"};
    p2.required_competence = 0.6f;

    uint32_t id1 = graph.registerTool("source_gen", p1, "Code Generator");
    uint32_t id2 = graph.registerTool("build_tool", p2, "Build Runner");
    uint32_t goal_id = graph.registerGoal("Generate Binary", {"executable"});

    graph.autoBuildEdges();

    CapabilityMatcher matcher(graph);
    auto matches = matcher.matchGoal("Generate Binary", {"executable"}, {}, "windows", 0.7f, 5);
    assert(!matches.empty());

    PathFinder pathfinder(graph);
    PathFinderConfig config;
    auto best_path = pathfinder.findBestPath(id1, goal_id, config);
    assert(best_path.has_value());
    assert(best_path->feasible);
    assert(best_path->node_sequence.size() == 3);

    std::cout << "[PASS] testMatcherAndPathFinder" << std::endl;
}

void testResourceOptimizerAndSequencing() {
    CapabilityGraph graph;

    CapabilityProfile p1;
    p1.tool_id = "code_gen";
    p1.inputs = {};
    p1.outputs = {"code_ast"};
    p1.avg_ram_mb = 100.0f;
    p1.avg_cpu_percent = 10.0f;

    CapabilityProfile p2;
    p2.tool_id = "code_compile";
    p2.inputs = {"code_ast"};
    p2.outputs = {"exe_file"};
    p2.avg_ram_mb = 300.0f;
    p2.avg_cpu_percent = 40.0f;

    uint32_t id1 = graph.registerTool("code_gen", p1, "Generator");
    uint32_t id2 = graph.registerTool("code_compile", p2, "Compiler");
    uint32_t goal_id = graph.registerGoal("Build", {"exe_file"});

    graph.autoBuildEdges();

    PathFinder pathfinder(graph);
    PathFinderConfig config;
    auto path_opt = pathfinder.findBestPath(id1, goal_id, config);
    assert(path_opt.has_value());

    yuki::system::ResourceMonitor monitor;
    ResourceOptimizer optimizer(&monitor, nullptr);

    auto schedule = optimizer.computeWaveSchedule(path_opt.value(), graph);
    assert(schedule.feasible);
    assert(!schedule.waves.empty());

    SequencingEngine sequencer;
    auto plan_opt = sequencer.toActionPlan(path_opt.value(), schedule, graph);
    assert(plan_opt.has_value());
    assert(!plan_opt->nodes.empty());

    std::cout << "[PASS] testResourceOptimizerAndSequencing" << std::endl;
}

int main() {
    std::cout << "Starting CapabilityGraph tests (M5)..." << std::endl;
    testProfileSerialization();
    testGraphOperations();
    testMatcherAndPathFinder();
    testResourceOptimizerAndSequencing();
    std::cout << "All CapabilityGraph tests PASSED successfully!" << std::endl;
    return 0;
}
