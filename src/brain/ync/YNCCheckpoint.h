// YNCCheckpoint.h — static save/load + directory listing.
#pragma once
#include "NeuromorphicSimulator.h"
#include <string>
#include <vector>

namespace ync {

class YNCCheckpoint {
public:
    static constexpr uint32_t MAGIC   = 0x594E434B;
    static constexpr uint16_t VERSION = 1;

    static bool save(const NeuromorphicSimulator& sim, const std::string& path);
    static bool load(NeuromorphicSimulator& sim, const std::string& path);
    static std::vector<std::string> listCheckpoints(const std::string& directory);
};

} // namespace ync
