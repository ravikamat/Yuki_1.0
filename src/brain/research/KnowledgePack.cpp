#include "brain/research/KnowledgePack.h"
#include <cstring>

namespace yuki {
namespace research {

std::vector<uint8_t> KnowledgePack::serialize() const {
    std::vector<uint8_t> data;
    data.resize(sizeof(uint64_t) * 3 + sizeof(float) * 3 + sizeof(uint32_t) * 2 + 1);
    size_t offset = 0;

    std::memcpy(data.data() + offset, &packId, sizeof(packId));
    offset += sizeof(packId);
    std::memcpy(data.data() + offset, &parentRequestId, sizeof(parentRequestId));
    offset += sizeof(parentRequestId);
    std::memcpy(data.data() + offset, &overallConfidence, sizeof(overallConfidence));
    offset += sizeof(overallConfidence);
    std::memcpy(data.data() + offset, &avgNovelty, sizeof(avgNovelty));
    offset += sizeof(avgNovelty);
    std::memcpy(data.data() + offset, &avgComplexity, sizeof(avgComplexity));
    offset += sizeof(avgComplexity);
    std::memcpy(data.data() + offset, &sourceToolCount, sizeof(sourceToolCount));
    offset += sizeof(sourceToolCount);
    std::memcpy(data.data() + offset, &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);
    data[offset] = static_cast<uint8_t>(confidence);

    return data;
}

bool KnowledgePack::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint64_t) * 3 + sizeof(float) * 3 + sizeof(uint32_t) * 2 + 1) {
        return false;
    }

    size_t offset = 0;
    std::memcpy(&packId, data.data() + offset, sizeof(packId));
    offset += sizeof(packId);
    std::memcpy(&parentRequestId, data.data() + offset, sizeof(parentRequestId));
    offset += sizeof(parentRequestId);
    std::memcpy(&overallConfidence, data.data() + offset, sizeof(overallConfidence));
    offset += sizeof(overallConfidence);
    std::memcpy(&avgNovelty, data.data() + offset, sizeof(avgNovelty));
    offset += sizeof(avgNovelty);
    std::memcpy(&avgComplexity, data.data() + offset, sizeof(avgComplexity));
    offset += sizeof(avgComplexity);
    std::memcpy(&sourceToolCount, data.data() + offset, sizeof(sourceToolCount));
    offset += sizeof(sourceToolCount);
    std::memcpy(&timestamp, data.data() + offset, sizeof(timestamp));
    offset += sizeof(timestamp);
    confidence = static_cast<KnowledgeConfidence>(data[offset]);

    return true;
}

} // namespace research
} // namespace yuki
