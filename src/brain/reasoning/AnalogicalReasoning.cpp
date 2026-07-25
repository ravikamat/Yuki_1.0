#include "brain/reasoning/AnalogicalReasoning.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace yuki { namespace reasoning {

class AnalogicalReasoning::Impl {
public:
    Impl() = default;

    double scoreMapping(const Domain& source, const Domain& target, const Mapping& mapping) const {
        if (source.relations.empty() || target.relations.empty()) return 0.0;

        double matchScore = 0.0;
        size_t consistentCount = 0;

        for (const auto& kv : mapping.relationMap) {
            RelationId sRelId = kv.first;
            RelationId tRelId = kv.second;

            if (sRelId < source.relations.size() && tRelId < target.relations.size()) {
                const auto& sRel = source.relations[sRelId];
                const auto& tRel = target.relations[tRelId];

                if (sRel.type == tRel.type) {
                    double relScore = sRel.weight * tRel.weight;
                    if (sRel.isHigherOrder) relScore *= 2.0;

                    // Check structural consistency of participants
                    bool consistent = true;
                    if (sRel.participants.size() == tRel.participants.size()) {
                        for (size_t i = 0; i < sRel.participants.size(); ++i) {
                            auto eIt = mapping.entityMap.find(sRel.participants[i]);
                            if (eIt != mapping.entityMap.end() && eIt->second != tRel.participants[i]) {
                                consistent = false;
                                break;
                            }
                        }
                    } else {
                        consistent = false;
                    }

                    if (consistent) {
                        consistentCount++;
                        matchScore += relScore;
                    }
                }
            }
        }

        double sysBonus = static_cast<double>(consistentCount) / static_cast<double>(source.relations.size());
        return matchScore * (1.0 + sysBonus);
    }
};

AnalogicalReasoning::AnalogicalReasoning() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "AnalogicalReasoning initialized");
}

AnalogicalReasoning::~AnalogicalReasoning() = default;

AnalogicalReasoning::AnalogicalReasoning(AnalogicalReasoning&&) noexcept = default;
AnalogicalReasoning& AnalogicalReasoning::operator=(AnalogicalReasoning&&) noexcept = default;

Mapping AnalogicalReasoning::findAnalogy(const Domain& source, const Domain& target) {
    Mapping bestMapping;
    bestMapping.score = 0.0;

    if (source.relations.empty() || target.relations.empty()) return bestMapping;

    // Group relations by type
    std::unordered_map<std::string, std::vector<RelationId>> sTypes, tTypes;
    for (size_t i = 0; i < source.relations.size(); ++i) {
        sTypes[source.relations[i].type].push_back(i);
    }
    for (size_t i = 0; i < target.relations.size(); ++i) {
        tTypes[target.relations[i].type].push_back(i);
    }

    Mapping candidate;
    for (const auto& kv : sTypes) {
        auto tIt = tTypes.find(kv.first);
        if (tIt != tTypes.end()) {
            const auto& sList = kv.second;
            const auto& tList = tIt->second;
            size_t pairCount = std::min(sList.size(), tList.size());

            for (size_t p = 0; p < pairCount; ++p) {
                RelationId sId = sList[p];
                RelationId tId = tList[p];
                candidate.relationMap[sId] = tId;

                const auto& sRel = source.relations[sId];
                const auto& tRel = target.relations[tId];
                size_t pCount = std::min(sRel.participants.size(), tRel.participants.size());

                for (size_t i = 0; i < pCount; ++i) {
                    candidate.entityMap[sRel.participants[i]] = tRel.participants[i];
                }
            }
        }
    }

    candidate.score = pImpl->scoreMapping(source, target, candidate);
    candidate.structuralConsistency = candidate.entityMap.empty() ? 0.0 : 1.0;
    candidate.systematicity = candidate.relationMap.empty() ? 0.0 : static_cast<double>(candidate.relationMap.size()) / source.relations.size();

    return candidate;
}

std::vector<Mapping> AnalogicalReasoning::findAnalogies(const Domain& source,
                                                       const std::vector<Domain>& targets,
                                                       size_t topK) {
    std::vector<Mapping> results;
    results.reserve(targets.size());

    for (const auto& target : targets) {
        results.push_back(findAnalogy(source, target));
    }

    std::sort(results.begin(), results.end(), [](const Mapping& a, const Mapping& b) {
        return a.score > b.score;
    });

    if (results.size() > topK) {
        results.resize(topK);
    }

    return results;
}

TransferResult AnalogicalReasoning::transfer(const Domain& source, const Mapping& mapping, Domain& target) {
    TransferResult res;
    res.confidence = mapping.score > 0.0 ? 0.8 : 0.0;

    for (size_t i = 0; i < source.relations.size(); ++i) {
        if (mapping.relationMap.find(i) == mapping.relationMap.end()) {
            const auto& sRel = source.relations[i];
            std::vector<EntityId> mappedParts;
            bool allMapped = true;

            for (EntityId eid : sRel.participants) {
                auto eIt = mapping.entityMap.find(eid);
                if (eIt != mapping.entityMap.end()) {
                    mappedParts.push_back(eIt->second);
                } else {
                    allMapped = false;
                    break;
                }
            }

            if (allMapped && !mappedParts.empty()) {
                Relation infRel;
                infRel.id = target.relations.size();
                infRel.type = sRel.type;
                infRel.participants = mappedParts;
                infRel.weight = sRel.weight * 0.9;
                infRel.isHigherOrder = sRel.isHigherOrder;

                target.relations.push_back(infRel);
                res.inferredRelations.push_back(infRel);
                res.justifications.push_back("Inferred from source relation: " + sRel.type);
            }
        }
    }

    return res;
}

double AnalogicalReasoning::computeSimilarity(const Domain& a, const Domain& b) {
    Mapping map = findAnalogy(a, b);
    return map.score;
}

double AnalogicalReasoning::evaluateMapping(const Domain& source, const Domain& target, const Mapping& mapping) {
    return pImpl->scoreMapping(source, target, mapping);
}

std::vector<uint8_t> AnalogicalReasoning::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x414E414C; // 'ANAL'

    buf.resize(4);
    std::memcpy(buf.data(), &magic, 4);

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool AnalogicalReasoning::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x414E414C) return false;

    return true;
}

}} // namespace yuki::reasoning
