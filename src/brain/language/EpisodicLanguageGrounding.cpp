#include "EpisodicLanguageGrounding.h"
#include "EpisodicStore.h"
#include "LanguageBootstrap.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <unordered_set>
#include <regex>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace language {

EpisodicLanguageGrounding::EpisodicLanguageGrounding(
    EpisodicStore* episodic_store,
    LanguageBootstrap* bootstrap)
    : episodic_store_(episodic_store), bootstrap_(bootstrap) {}

EpisodicLanguageGrounding::~EpisodicLanguageGrounding() = default;

void EpisodicLanguageGrounding::recordEpisode(const LanguageEpisode& episode) {
    if (!episodic_store_) return;

    try {
        nlohmann::json episode_data;
        episode_data["text"] = episode.text;
        episode_data["context"] = episode.context;
        episode_data["word_to_concept"] = episode.word_to_concept;
        episode_data["outcome"] = episode.outcome;
        episode_data["correction_target"] = episode.correction_target;
        episode_data["reward"] = episode.reward;

        // EpisodeRecord lives in yuki::brain::memory and its timestamp is a
        // double in seconds (the store previously exposed a ms field).
        yuki::brain::memory::EpisodeRecord record;
        record.text = episode_data.dump();
        record.source = "language_episode";
        record.timestamp = std::chrono::duration<double>(
            episode.timestamp.time_since_epoch()).count();
        record.confidence = 0.9f;
        record.topic_tag = "language_episode";

        // The current EpisodicStore is a text store without a vector-slot API,
        // so no embedding is persisted alongside the record.
        episodic_store_->insert(record);

        LanguageEpisode indexed_episode = episode;
        indexed_episode.id = std::to_string(next_episode_id_);
        indexed_episode.episode_id = next_episode_id_++;

        updateIndices(indexed_episode);

    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "EpisodicLanguageGrounding",
            "Failed to record episode: " + std::string(e.what()));
    }
}

std::vector<LanguageEpisode> EpisodicLanguageGrounding::findSimilarEpisodes(
    const std::string& text, const std::string& context, size_t k) const {
    
    std::vector<LanguageEpisode> similar;
    if (!episodic_store_) return similar;

    // EpisodicStore has no vector-similarity API yet: take a recency window
    // and prefer records whose text shares a token with the query (text +
    // context), so callers still get a relevance-ranked candidate set.
    std::vector<std::string> query_tokens;
    {
        std::istringstream iss(text + " " + context);
        std::string tok;
        while (iss >> tok) {
            if (tok.size() >= 3) query_tokens.push_back(tok);
        }
    }
    auto matchesQuery = [&query_tokens](const std::string& haystack) {
        for (const auto& tok : query_tokens) {
            if (haystack.find(tok) != std::string::npos) return true;
        }
        return false;
    };

    auto results = episodic_store_->queryRecentSnapshots(k * 8, false);

    for (const auto& record : results) {
        if (similar.size() >= k) break;
        if (!query_tokens.empty() && !matchesQuery(record.text)) continue;
        
        LanguageEpisode ep;
        ep.id = std::to_string(record.id);
        ep.episode_id = record.id;
        ep.text = record.text;
        ep.context = record.topic_tag;
        similar.push_back(ep);
    }

    return similar;
}

std::optional<WordUsageStats> EpisodicLanguageGrounding::getWordStats(const std::string& word) const {
    std::lock_guard<std::mutex> lock(index_mutex_);
    auto it = word_stats_.find(word);
    if (it != word_stats_.end()) return it->second;
    return std::nullopt;
}

std::vector<LanguageEpisode> EpisodicLanguageGrounding::getEpisodesForConcept(
    const std::string& concept_id, size_t limit) const {
    
    std::vector<LanguageEpisode> episodes;
    if (!episodic_store_) return episodes;

    std::lock_guard<std::mutex> lock(index_mutex_);
    auto it = concept_episodes_.find(concept_id);
    if (it == concept_episodes_.end()) return episodes;

    for (size_t i = 0; i < std::min(limit, it->second.size()); ++i) {
        LanguageEpisode ep;
        ep.id = std::to_string(it->second[it->second.size() - 1 - i]);
        ep.episode_id = it->second[it->second.size() - 1 - i];
        episodes.push_back(ep);
    }
    return episodes;
}

std::vector<LanguageEpisode> EpisodicLanguageGrounding::getEpisodesByOutcome(
    const std::string& outcome, size_t limit) const {
    
    std::vector<LanguageEpisode> episodes;
    // In production: query by outcome metadata
    return episodes;
}

float EpisodicLanguageGrounding::computeConceptConfidence(
    const std::string& word, const std::string& concept_id) const {
    
    auto stats = getWordStats(word);
    if (!stats.has_value()) return 0.0f;

    if (stats->concept_id != concept_id) return 0.0f;

    int total = stats->success_count + stats->correction_count;
    if (total == 0) return 0.0f;

    float confidence = static_cast<float>(stats->success_count) / total;
    
    // Boost by average reward
    confidence = std::clamp(confidence + stats->avg_reward * 0.2f, 0.0f, 1.0f);
    
    return confidence;
}

std::vector<std::string> EpisodicLanguageGrounding::getContextualNeighbors(
    const std::string& word, size_t k) const {
    
    std::vector<std::string> neighbors;
    if (bootstrap_) {
        return bootstrap_->getSynonyms(word);
    }
    return neighbors;
}

EpisodicLanguageGrounding::Stats EpisodicLanguageGrounding::getStats() const {
    std::lock_guard<std::mutex> lock(index_mutex_);
    Stats stats;
    stats.total_episodes = all_episode_ids_.size();
    stats.unique_words = word_stats_.size();
    stats.unique_concepts = concept_episodes_.size();

    for (const auto& [_, ws] : word_stats_) {
        stats.successful_episodes += ws.success_count;
        stats.correction_episodes += ws.correction_count;
    }
    return stats;
}

void EpisodicLanguageGrounding::updateIndices(const LanguageEpisode& episode) {
    std::lock_guard<std::mutex> lock(index_mutex_);
    
    all_episode_ids_.push_back(episode.episode_id);
    updateWordStats(episode);
    updateConceptIndex(episode);
    updateWordIndex(episode);
}

void EpisodicLanguageGrounding::updateWordStats(const LanguageEpisode& episode) {
    for (const auto& [word, c_id] : episode.word_to_concept) {
        auto& stats = word_stats_[word];
        stats.word = word;
        stats.concept_id = c_id;
        stats.usage_count++;
        
        if (episode.outcome == "success") stats.success_count++;
        else if (episode.outcome == "correction") stats.correction_count++;
        
        // Update running average reward
        float n = static_cast<float>(stats.usage_count);
        stats.avg_reward = (stats.avg_reward * (n - 1) + episode.reward) / n;
        
        stats.last_used = episode.timestamp;
        if (stats.contexts.size() > 20) stats.contexts.erase(stats.contexts.begin());
        stats.contexts.push_back(episode.context);
    }
}

void EpisodicLanguageGrounding::updateConceptIndex(const LanguageEpisode& episode) {
    for (const auto& [_, c_id] : episode.word_to_concept) {
        concept_episodes_[c_id].push_back(episode.episode_id);
    }
}

void EpisodicLanguageGrounding::updateWordIndex(const LanguageEpisode& episode) {
    for (const auto& [word, _] : episode.word_to_concept) {
        word_episodes_[word].push_back(episode.episode_id);
    }
}

} // namespace language
} // namespace brain
} // namespace yuki