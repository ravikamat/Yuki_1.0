#include "brain/world/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>

namespace yuki::world {

// ============================================================================
// RigidBody Implementation
// ============================================================================

void RigidBody::updateBounds(const Vec2& half_extents) {
    bounds.min = position - half_extents;
    bounds.max = position + half_extents;
}

void RigidBody::integrate(float dt) {
    if (is_static) return;

    Vec2 gravity{0.0f, -9.81f};
    Vec2 accel = (force_accum * inv_mass) + gravity;
    velocity = velocity + (accel * dt);

    // Apply friction damping
    velocity = velocity * (1.0f - std::clamp(friction * dt, 0.0f, 1.0f));

    // Velocity clamp (max 100.0)
    float max_v = 100.0f;
    if (velocity.lengthSq() > max_v * max_v) {
        float len = std::sqrt(velocity.lengthSq());
        velocity = velocity * (max_v / len);
    }

    position = position + (velocity * dt);
    updateBounds();
}

// ============================================================================
// SpatialGrid Implementation
// ============================================================================

SpatialGrid::SpatialGrid(float cell_size) : cell_size_(cell_size) {}

int64_t SpatialGrid::hashKey(int32_t x, int32_t y) {
    return (static_cast<int64_t>(x) << 32) | (static_cast<uint32_t>(y));
}

void SpatialGrid::clear() {
    cells_.clear();
}

void SpatialGrid::insert(RigidBody* body) {
    if (!body) return;
    int32_t cx = static_cast<int32_t>(std::floor(body->position.x / cell_size_));
    int32_t cy = static_cast<int32_t>(std::floor(body->position.y / cell_size_));
    cells_[hashKey(cx, cy)].push_back(body);
}

std::vector<std::pair<RigidBody*, RigidBody*>> SpatialGrid::queryPairs() const {
    std::vector<std::pair<RigidBody*, RigidBody*>> pairs;
    std::set<std::pair<uint64_t, uint64_t>> visited;

    for (const auto& [key, list] : cells_) {
        for (size_t i = 0; i < list.size(); ++i) {
            for (size_t j = i + 1; j < list.size(); ++j) {
                RigidBody* a = list[i];
                RigidBody* b = list[j];
                uint64_t id_a = std::min(a->concept_id, b->concept_id);
                uint64_t id_b = std::max(a->concept_id, b->concept_id);
                if (visited.insert({id_a, id_b}).second) {
                    pairs.push_back({a, b});
                }
            }
        }
    }
    return pairs;
}

// ============================================================================
// PhysicsWorld Implementation
// ============================================================================

PhysicsWorld::PhysicsWorld(const Vec2& gravity) : gravity_(gravity), grid_(10.0f) {}

void PhysicsWorld::addBody(std::unique_ptr<RigidBody> body) {
    if (body) bodies_.push_back(std::move(body));
}

void PhysicsWorld::removeBody(uint64_t concept_id) {
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
        [concept_id](const auto& b) { return b->concept_id == concept_id; }), bodies_.end());
}

RigidBody* PhysicsWorld::getBody(uint64_t concept_id) {
    for (auto& b : bodies_) {
        if (b->concept_id == concept_id) return b.get();
    }
    return nullptr;
}

void PhysicsWorld::clearForces() {
    for (auto& b : bodies_) {
        b->force_accum = {0.0f, 0.0f};
    }
}

void PhysicsWorld::broadphase() {
    grid_.clear();
    for (auto& b : bodies_) {
        grid_.insert(b.get());
    }
}

void PhysicsWorld::resolveCollision(RigidBody* a, RigidBody* b) {
    if (!a || !b || (a->is_static && b->is_static)) return;

    Vec2 rv = b->velocity - a->velocity;
    Vec2 normal{1.0f, 0.0f};
    float vel_along_normal = rv.dot(normal);

    if (vel_along_normal > 0) return;

    float e = std::min(a->restitution, b->restitution);
    float j = -(1.0f + e) * vel_along_normal;
    j /= (a->inv_mass + b->inv_mass);

    Vec2 impulse = normal * j;
    if (!a->is_static) a->velocity = a->velocity - (impulse * a->inv_mass);
    if (!b->is_static) b->velocity = b->velocity + (impulse * b->inv_mass);
}

void PhysicsWorld::step(float dt) {
    clearForces();

    for (auto& b : bodies_) {
        b->integrate(dt);
    }

    broadphase();
    auto pairs = grid_.queryPairs();

    for (int iter = 0; iter < 3; ++iter) {
        for (auto& [a, b] : pairs) {
            if (a->bounds.intersects(b->bounds)) {
                resolveCollision(a, b);
            }
        }
    }
}

std::string PhysicsWorld::describeStateChange(const RigidBody* before, const RigidBody* after) {
    std::ostringstream oss;
    oss << "body_" << after->concept_id << " moved from ("
        << before->position.x << "," << before->position.y << ") to ("
        << after->position.x << "," << after->position.y << ")";
    return oss.str();
}

std::vector<std::string> PhysicsWorld::simulateIntervention(uint64_t body_id,
                                                           const Vec2& impulse,
                                                           float duration_ms,
                                                           float dt) {
    std::vector<std::string> log;
    RigidBody* target = getBody(body_id);
    if (!target) return log;

    RigidBody before = *target;
    target->velocity = target->velocity + (impulse * target->inv_mass);

    int steps = static_cast<int>(duration_ms / (dt * 1000.0f));
    for (int i = 0; i < steps; ++i) {
        step(dt);
    }

    if ((target->position - before.position).lengthSq() > 0.0001f) {
        log.push_back(describeStateChange(&before, target));
    }

    return log;
}

std::vector<std::tuple<std::string, std::string, std::string>> PhysicsWorld::extractCausalRules() {
    std::vector<std::tuple<std::string, std::string, std::string>> rules;
    for (size_t i = 0; i < bodies_.size(); ++i) {
        for (size_t j = i + 1; j < bodies_.size(); ++j) {
            if (bodies_[i]->bounds.intersects(bodies_[j]->bounds)) {
                rules.push_back({
                    "concept_" + std::to_string(bodies_[i]->concept_id),
                    "pushes",
                    "concept_" + std::to_string(bodies_[j]->concept_id)
                });
            }
        }
    }
    return rules;
}

} // namespace yuki::world
