#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace yuki { namespace reasoning {

using EntityId = size_t;
using RelationId = size_t;

struct Entity {
    EntityId id = 0;
    std::string name;
    std::unordered_map<std::string, double> attributes;
};

struct Relation {
    RelationId id = 0;
    std::string type;
    std::vector<EntityId> participants;
    double weight = 1.0;
    bool isHigherOrder = false;
};

struct Domain {
    std::string name;
    std::vector<Entity> entities;
    std::vector<Relation> relations;
};

struct Mapping {
    std::unordered_map<EntityId, EntityId> entityMap;
    std::unordered_map<RelationId, RelationId> relationMap;
    double score = 0.0;
    double systematicity = 0.0;
    double structuralConsistency = 0.0;
};

struct TransferResult {
    std::vector<Relation> inferredRelations;
    double confidence = 0.0;
    std::vector<std::string> justifications;
};

class AnalogicalReasoning {
public:
    AnalogicalReasoning();
    ~AnalogicalReasoning();
    AnalogicalReasoning(const AnalogicalReasoning&) = delete;
    AnalogicalReasoning& operator=(const AnalogicalReasoning&) = delete;
    AnalogicalReasoning(AnalogicalReasoning&&) noexcept;
    AnalogicalReasoning& operator=(AnalogicalReasoning&&) noexcept;

    Mapping findAnalogy(const Domain& source, const Domain& target);
    std::vector<Mapping> findAnalogies(const Domain& source, const std::vector<Domain>& targets, size_t topK = 3);
    TransferResult transfer(const Domain& source, const Mapping& mapping, Domain& target);
    double computeSimilarity(const Domain& a, const Domain& b);

    double evaluateMapping(const Domain& source, const Domain& target, const Mapping& mapping);

    // Binary serialization: magic = 0x414E414C ('ANAL')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::reasoning
