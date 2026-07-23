#ifndef YUKI_KNOWLEDGE_TAG_H
#define YUKI_KNOWLEDGE_TAG_H

#include <cstdint>
#include <string>

namespace yuki {
namespace memory {

struct KnowledgeTag {
    std::string tagId;
    std::string colorHex = "#3498db";
    uint64_t    associatedConceptHash = 0;
};

} // namespace memory
} // namespace yuki

#endif
