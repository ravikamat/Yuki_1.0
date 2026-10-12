// YNCCheckpoint.h — static save/load + directory listing.
#pragma once
#include "NeuromorphicSimulator.h"
#include <string>
#include <vector>

#include <unordered_map>
#include <mutex>

namespace ync {

class YNCCheckpoint {
public:
    static constexpr uint32_t MAGIC   = 0x594E434B;
    static constexpr uint16_t VERSION = 1;

    static bool save(const NeuromorphicSimulator& sim, const std::string& path);
    static bool load(NeuromorphicSimulator& sim, const std::string& path);
    static std::vector<std::string> listCheckpoints(const std::string& directory);

    // Task 5.2 SQLite Synaptic Weight Checkpointing & Restore
    static bool checkpointToSQLite(const NeuromorphicSimulator& sim,
                                   const std::string& db_path = "data/brain/ync_synapses.db");
    static size_t checkpointIncrementalToSQLite(const NeuromorphicSimulator& sim,
                                                const std::string& db_path = "data/brain/ync_synapses.db");
    static bool restoreFromCheckpoint(NeuromorphicSimulator& sim,
                                      const std::string& db_path = "data/brain/ync_synapses.db");

private:
    static std::unordered_map<std::string, float> last_checkpointed_weights_;
    static std::mutex cache_mutex_;
};

} // namespace ync
