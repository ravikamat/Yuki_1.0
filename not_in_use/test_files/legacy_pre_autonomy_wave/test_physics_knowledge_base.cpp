#include "brain/knowledge/PhysicsKnowledgeBase.h"
#include "brain/causality/CausalGraph.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] PhysicsKnowledgeBase..." << std::endl;

    yuki::knowledge::PhysicsKnowledgeBase pkb;
    bool ok = pkb.load("data/physics_knowledge.jsonl");
    assert(ok);

    const auto* water = pkb.getMaterial("water");
    assert(water != nullptr);
    assert(water->density == 1000.0f);
    assert(water->boiling_point == 100.0f);
    assert(water->is_fluid == true);

    auto laws = pkb.getLawsByDomain("terrestrial");
    assert(!laws.empty());
    assert(laws[0].name == "newton_gravity");

    yuki::causality::CausalGraph graph;
    pkb.syncToCausalGraph(&graph);
    assert(!graph.nodes.empty());

    pkb.exportToPhysicsWorld("test_physics_materials.txt");
    std::remove("test_physics_materials.txt");

    std::cout << "[TEST] PhysicsKnowledgeBase PASSED!" << std::endl;
    return 0;
}
