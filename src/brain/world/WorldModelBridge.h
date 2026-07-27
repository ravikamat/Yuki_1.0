#pragma once
#include "PhysicsWorld.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/causality/CausalGraph.h"
#include <string>
#include <vector>
#include <utility>

namespace yuki::world {

class WorldModelBridge {
public:
    WorldModelBridge(PhysicsWorld* world, yuki::memory::HdcSemanticGraph* graph);

    // Bind a concept to a physical body at initial position with given mass.
    void bindConcept(uint64_t concept_id, const Vec2& initial_pos, float mass,
                     const Vec2& half_extents = {1.0f, 1.0f}, bool is_static = false);

    // Answer natural language spatial queries via simulation.
    std::string answerQuery(const std::string& query_frame,
                            const std::vector<std::pair<std::string, uint64_t>>& entities);

    // Export all collision/push causal rules to CausalGraph.
    void syncCausalRulesToGraph(yuki::causality::CausalGraph* graph);

private:
    PhysicsWorld* world_;
    yuki::memory::HdcSemanticGraph* graph_;
};

} // namespace yuki::world
