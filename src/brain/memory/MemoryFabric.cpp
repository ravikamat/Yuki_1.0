#include "brain/memory/MemoryFabric.h"

namespace yuki {
namespace memory {

void MemoryFabric::store(const MemoryItem& item) {
    switch (item.tier) {
        case MemoryTier::T0_WORKING:      t0Working_.push_back(item); break;
        case MemoryTier::T1_EPISODIC:     t1Episodic_.push_back(item); break;
        case MemoryTier::T2_SEMANTIC_HDC: t2Semantic_.push_back(item); break;
        case MemoryTier::T3_PROCEDURAL:   t3Procedural_.push_back(item); break;
        case MemoryTier::T4_ARCHIVE_MERKLE: t4Archive_.push_back(item); break;
    }
}

std::vector<MemoryItem> MemoryFabric::retrieve(const std::string& query, RetrieveMode mode) {
    std::vector<MemoryItem> results;
    // Search across T0 and T1 tiers
    for (const auto& item : t0Working_) {
        if (item.key.find(query) != std::string::npos || mode == RetrieveMode::FUZZY) {
            results.push_back(item);
        }
    }
    for (const auto& item : t1Episodic_) {
        if (item.key.find(query) != std::string::npos || mode == RetrieveMode::FUZZY) {
            results.push_back(item);
        }
    }
    return results;
}

void MemoryFabric::consolidateT0toT1() {
    for (auto& item : t0Working_) {
        item.tier = MemoryTier::T1_EPISODIC;
        t1Episodic_.push_back(item);
    }
    t0Working_.clear();
}

void MemoryFabric::consolidateT1toT2() {
    for (auto& item : t1Episodic_) {
        item.tier = MemoryTier::T2_SEMANTIC_HDC;
        t2Semantic_.push_back(item);
    }
    t1Episodic_.clear();
}

void MemoryFabric::archiveToT4() {
    for (auto& item : t2Semantic_) {
        item.tier = MemoryTier::T4_ARCHIVE_MERKLE;
        t4Archive_.push_back(item);
    }
    t2Semantic_.clear();
}

size_t MemoryFabric::getItemCount(MemoryTier tier) const {
    switch (tier) {
        case MemoryTier::T0_WORKING:      return t0Working_.size();
        case MemoryTier::T1_EPISODIC:     return t1Episodic_.size();
        case MemoryTier::T2_SEMANTIC_HDC: return t2Semantic_.size();
        case MemoryTier::T3_PROCEDURAL:   return t3Procedural_.size();
        case MemoryTier::T4_ARCHIVE_MERKLE: return t4Archive_.size();
    }
    return 0;
}

void MemoryFabric::clear() {
    t0Working_.clear();
    t1Episodic_.clear();
    t2Semantic_.clear();
    t3Procedural_.clear();
    t4Archive_.clear();
}

} // namespace memory
} // namespace yuki
