#include "brain/world/WorldModelBridge.h"
#include "brain/core/ConfigManager.h"
#include <sstream>

namespace yuki::world {

WorldModelBridge::WorldModelBridge(PhysicsWorld* world, yuki::memory::HdcSemanticGraph* graph)
    : world_(world), graph_(graph) {}

void WorldModelBridge::bindConcept(uint64_t concept_id, const Vec2& initial_pos, float mass,
                                   const Vec2& half_extents, bool is_static) {
    if (!world_) return;
    auto body = std::make_unique<RigidBody>();
    body->concept_id = concept_id;
    body->position = initial_pos;
    body->mass = mass;
    body->inv_mass = is_static ? 0.0f : 1.0f / (mass > 0 ? mass : 1.0f);
    body->is_static = is_static;
    body->updateBounds(half_extents);
    world_->addBody(std::move(body));
}

std::string WorldModelBridge::answerQuery(const std::string& query_frame,
                                          const std::vector<std::pair<std::string, uint64_t>>& entities) {
    if (!world_ || entities.empty()) return "";

    uint64_t target_id = entities[0].second;
    Vec2 impulse{5.0f, 0.0f};

    if (query_frame.find("NORTH") != std::string::npos) impulse = {0.0f, 5.0f};
    else if (query_frame.find("SOUTH") != std::string::npos) impulse = {0.0f, -5.0f};
    else if (query_frame.find("WEST") != std::string::npos) impulse = {-5.0f, 0.0f};

    auto logs = world_->simulateIntervention(target_id, impulse, 500.0f);
    if (logs.empty()) {
        return "Simulated intervention produced no spatial displacement for entity " + std::to_string(target_id);
    }

    std::ostringstream oss;
    oss << "Spatial simulation result: " << logs[0];
    return oss.str();
}

void WorldModelBridge::syncCausalRulesToGraph(yuki::causality::CausalGraph* causal_graph) {
    if (!world_ || !causal_graph) return;
    auto rules = world_->extractCausalRules();
    for (const auto& [cause, rel, effect] : rules) {
        causal_graph->addNode(cause);
        causal_graph->addNode(effect);
    }
}

} // namespace yuki::world
