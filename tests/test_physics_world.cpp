#include "brain/world/PhysicsWorld.h"
#include "brain/world/WorldModelBridge.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] PhysicsWorld & WorldModelBridge..." << std::endl;

    using namespace yuki::world;
    PhysicsWorld world({0.0f, -9.81f});

    auto b1 = std::make_unique<RigidBody>();
    b1->concept_id = 101;
    b1->position = {0.0f, 10.0f};
    b1->mass = 1.0f;
    b1->inv_mass = 1.0f;
    b1->updateBounds();

    world.addBody(std::move(b1));
    assert(world.bodyCount() == 1);

    // Gravity test
    world.step(0.1f);
    RigidBody* body = world.getBody(101);
    assert(body != nullptr);
    assert(body->velocity.y < 0.0f);

    // WorldModelBridge test
    yuki::memory::HdcSemanticGraph graph;
    WorldModelBridge bridge(&world, &graph);
    bridge.bindConcept(202, {5.0f, 0.0f}, 2.0f);

    std::string ans = bridge.answerQuery("PUSH_NORTH", {{"cup", 202}});
    assert(!ans.empty());

    std::cout << "[TEST] PhysicsWorld PASSED!" << std::endl;
    return 0;
}
