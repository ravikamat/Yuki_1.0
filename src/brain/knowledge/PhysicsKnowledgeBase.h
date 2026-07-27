#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace yuki::causality { class CausalGraph; }

namespace yuki::knowledge {

struct MaterialProperties {
    std::string name;
    float density = 1000.0f;          // kg/m³
    float restitution = 0.5f;         // bounciness
    float static_friction = 0.3f;
    float dynamic_friction = 0.2f;
    float melting_point = 0.0f;       // Celsius
    float boiling_point = 100.0f;     // Celsius
    bool is_fluid = false;
    float viscosity = 0.0f;           // Pa·s
};

struct PhysicalLaw {
    std::string name;
    std::string formula;
    std::vector<std::string> variables;
    std::vector<std::string> applicable_domains;
    float confidence = 1.0f;
};

struct PhysicsTriplet {
    std::string subject;
    std::string relation;
    std::string object;
    float weight = 1.0f;
};

class PhysicsKnowledgeBase {
public:
    PhysicsKnowledgeBase() = default;

    // Load from data/physics_knowledge.jsonl
    bool load(const std::string& path);

    const MaterialProperties* getMaterial(const std::string& name) const;
    std::vector<PhysicalLaw> getLawsByDomain(const std::string& domain) const;

    void exportToPhysicsWorld(const std::string& config_path) const;
    void syncToCausalGraph(yuki::causality::CausalGraph* graph) const;

    size_t materialCount() const { return materials_.size(); }
    size_t lawCount() const { return laws_.size(); }
    size_t tripletCount() const { return triplets_.size(); }

private:
    std::unordered_map<std::string, MaterialProperties> materials_;
    std::vector<PhysicalLaw> laws_;
    std::vector<PhysicsTriplet> triplets_;
    std::unordered_map<std::string, std::vector<size_t>> domain_index_;

    std::string extractJsonString(const std::string& json, const std::string& key) const;
    float extractJsonFloat(const std::string& json, const std::string& key, float default_val = 0.0f) const;
    bool extractJsonBool(const std::string& json, const std::string& key) const;
    std::vector<std::string> extractJsonArray(const std::string& json, const std::string& key) const;
};

} // namespace yuki::knowledge
