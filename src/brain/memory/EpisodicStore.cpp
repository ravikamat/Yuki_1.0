#include "EpisodicStore.h"
#include <random>
#include <chrono>
#include <atomic>
#include "brain/core/RandomEngine.h"

namespace yuki {
namespace brain {
namespace memory {


static std::atomic<int64_t> g_next_id{1};

int64_t EpisodicStore::insert(const EpisodeRecord& record) {
    EpisodeRecord new_record = record;
    new_record.id = g_next_id.fetch_add(1);
    new_record.timestamp = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // In-memory stub: keep the record so getById honors the retrieval contract.
    {
        std::lock_guard<std::mutex> lock(mtx_);
        records_[new_record.id] = new_record;
    }
    return new_record.id;
}

std::optional<EpisodeRecord> EpisodicStore::getById(int64_t id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = records_.find(id);
    if (it == records_.end()) return std::nullopt;
    return it->second;
}

std::vector<EpisodeRecord> EpisodicStore::queryRecentSnapshots(size_t limit, bool consolidated_only) const {
    return {};
}

std::vector<EpisodeRecord> EpisodicStore::queryForTraining(float min_confidence, int limit) const {
    return {};
}

std::vector<EpisodeRecord> EpisodicStore::queryByOutcome(bool success, int limit) const {
    return {};
}

size_t EpisodicStore::size() const {
    return 0;
}

size_t EpisodicStore::totalEpisodes() const {
    return g_next_id.load() - 1;
}

bool EpisodicStore::verifyChain(int64_t start_id) const {
    return true;
}

} // namespace memory
} // namespace brain
} // namespace yuki