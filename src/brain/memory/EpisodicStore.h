#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <cstdint>
#include <map>
#include <mutex>

namespace yuki {
namespace brain {
namespace memory {

struct EpisodeRecord {
    int64_t id = -1;
    int64_t session_id = 0;
    double timestamp = 0.0;
    int64_t vector_slot = -1;
    bool consolidated = false;
    std::string source;
    std::string text;
    std::string intent_label;
    float confidence = 0.0f;
    std::string topic_tag;
    
    // Scores for classification
    float question_score = 0.0f;
    float command_score = 0.0f;
    float emotional_score = 0.0f;
    float technical_score = 0.0f;
    float greeting_score = 0.0f;
    float urgency_score = 0.0f;
    float action_score = 0.0f;
    float polarity_score = 0.0f;
    float phatic_score = 0.0f;
    bool outcome_success = false;
};

class EpisodicStore {
public:
    EpisodicStore() = default;
    ~EpisodicStore() = default;
    
    int64_t insert(const EpisodeRecord& record);
    std::optional<EpisodeRecord> getById(int64_t id) const;
    std::vector<EpisodeRecord> queryRecentSnapshots(size_t limit, bool consolidated_only = false) const;
    std::vector<EpisodeRecord> queryForTraining(float min_confidence, int limit) const;
    std::vector<EpisodeRecord> queryByOutcome(bool success, int limit) const;
    size_t size() const;
    size_t totalEpisodes() const;
    bool verifyChain(int64_t start_id) const;
    
private:
    // In-memory stub storage (id → record). No persistence layer yet;
    // insert/getById contract is honored within the process lifetime.
    mutable std::mutex              mtx_;
    std::map<int64_t, EpisodeRecord> records_;
};

} // namespace memory
} // namespace brain
} // namespace yuki