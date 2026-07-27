#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>

namespace yuki::world {

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float lengthSq() const { return x * x + y * y; }
};

struct AABB {
    Vec2 min, max;
    bool intersects(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y;
    }
    Vec2 center() const { return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f}; }
};

class RigidBody {
public:
    uint64_t concept_id = 0;   // links to HdcSemanticGraph node
    Vec2 position;
    Vec2 velocity;
    Vec2 force_accum;
    float mass = 1.0f;
    float inv_mass = 1.0f;
    float restitution = 0.5f;  // bounciness
    float friction = 0.3f;
    AABB bounds;
    bool is_static = false;

    void applyForce(const Vec2& f) { force_accum = force_accum + f; }
    void integrate(float dt);
    void updateBounds(const Vec2& half_extents = {1.0f, 1.0f});
};

class SpatialGrid {
public:
    explicit SpatialGrid(float cell_size = 10.0f);
    void clear();
    void insert(RigidBody* body);
    std::vector<std::pair<RigidBody*, RigidBody*>> queryPairs() const;

private:
    float cell_size_;
    std::unordered_map<int64_t, std::vector<RigidBody*>> cells_;
    static int64_t hashKey(int32_t x, int32_t y);
};

class PhysicsWorld {
public:
    explicit PhysicsWorld(const Vec2& gravity = {0.0f, -9.81f});

    void addBody(std::unique_ptr<RigidBody> body);
    void removeBody(uint64_t concept_id);
    RigidBody* getBody(uint64_t concept_id);

    // Full physics tick: integrate → broadphase → narrowphase → resolve
    void step(float dt);

    // Apply impulse to body and simulate for duration_ms. Returns state change log.
    std::vector<std::string> simulateIntervention(uint64_t body_id,
                                                   const Vec2& impulse,
                                                   float duration_ms,
                                                   float dt = 0.016f);

    // Extract causal rules for CausalGraph insertion.
    // Format: (cause_predicate, relation, effect_predicate)
    std::vector<std::tuple<std::string, std::string, std::string>> extractCausalRules();

    size_t bodyCount() const { return bodies_.size(); }

private:
    Vec2 gravity_;
    std::vector<std::unique_ptr<RigidBody>> bodies_;
    SpatialGrid grid_;

    void broadphase();
    void resolveCollision(RigidBody* a, RigidBody* b);
    void clearForces();
    std::string describeStateChange(const RigidBody* before, const RigidBody* after);
};

} // namespace yuki::world
