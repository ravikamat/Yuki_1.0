#pragma once
#include "brain/memory/EpisodicStore.h"
#include "LanguageBootstrap.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>

namespace yuki {
namespace brain {
namespace language {

using yuki::brain::memory::EpisodicStore;

struct LanguageEpisode {
    std::string id;
    std::string text;
    std::string context;
    std::map<std::string, std::string> word_to_concept;  // word -> concept_id
    std::string outcome;  // "success", "correction", "clarification", "failure"
    std::string correction_target;  // if correction: what was corrected
    float reward = 0.0f;  // -1.0 to 1.0
    std::chrono::system_clock::time_point timestamp;
    uint64_t episode_id = 0;
};

struct WordUsageStats {
    std::string word;
    std::string concept_id;
    int usage_count = 0;
    int success_count = 0;
    int correction_count = 0;
    float avg_reward = 0.0f;
    std::chrono::system_clock::time_point last_used;
    std::vector<std::string> contexts;  // recent contexts where used
};

class EpisodicLanguageGrounding {
public:
    explicit EpisodicLanguageGrounding(
        EpisodicStore* episodic_store,
        LanguageBootstrap* bootstrap);
    ~EpisodicLanguageGrounding();

    // Record a language interaction episode
    void recordEpisode(const LanguageEpisode& episode);

    // Query similar past episodes
    std::vector<LanguageEpisode> findSimilarEpisodes(
        const std::string& text, const std::string& context, size_t k = 10) const;

    // Get usage statistics for a word
    std::optional<WordUsageStats> getWordStats(const std::string& word) const;

    // Get recent episodes for a concept
    std::vector<LanguageEpisode> getEpisodesForConcept(
        const std::string& concept_id, size_t limit = 20) const;

    // Get episodes by outcome
    std::vector<LanguageEpisode> getEpisodesByOutcome(
        const std::string& outcome, size_t limit = 20) const;

    // Compute word-concept confidence from episodic history
    float computeConceptConfidence(const std::string& word, const std::string& concept_id) const;

    // Get words that often appear in similar contexts
    std::vector<std::string> getContextualNeighbors(
        const std::string& word, size_t k = 10) const;

    // Statistics
    struct Stats {
        uint64_t total_episodes = 0;
        uint64_t successful_episodes = 0;
        uint64_t correction_episodes = 0;
        uint64_t unique_words = 0;
        uint64_t unique_concepts = 0;
    };
    Stats getStats() const;

private:
    EpisodicStore* episodic_store_;
    LanguageBootstrap* bootstrap_;

    // In-memory indices for fast queries
    mutable std::mutex index_mutex_;
    std::unordered_map<std::string, WordUsageStats> word_stats_;
    std::unordered_map<std::string, std::vector<uint64_t>> concept_episodes_;  // concept_id -> episode_ids
    std::unordered_map<std::string, std::vector<uint64_t>> word_episodes_;  // word -> episode_ids
    std::vector<uint64_t> all_episode_ids_;
    uint64_t next_episode_id_ = 1;

    void updateIndices(const LanguageEpisode& episode);
    void updateWordStats(const LanguageEpisode& episode);
    void updateConceptIndex(const LanguageEpisode& episode);
    void updateWordIndex(const LanguageEpisode& episode);
};

} // namespace language
} // namespace brain
} // namespace yuki