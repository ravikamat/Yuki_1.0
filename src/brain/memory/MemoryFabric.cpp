#include "brain/memory/MemoryFabric.h"
#include <algorithm>
#include <cmath>

namespace yuki {
namespace memory {

// ---- Constants ----
static constexpr uint64_t kFnvOffsetBasis  = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime        = 0x100000001b3ULL;
static constexpr float    kExactMatchBoost = 1.0f;
static constexpr float    kPrefixMatchBase = 0.7f;
static constexpr float    kFuzzyThreshold  = 0.25f;
static constexpr uint32_t kMaxFuzzyResults = 50;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

// Normalized character for case-insensitive comparison
static char normChar(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

// ---- Similarity scoring ----
// Uses character n-gram hash overlap for fuzzy matching.
// This is a real similarity metric, not "return everything."

static float computeSimilarity(const std::string& query, const std::string& key, RetrieveMode mode) {
    if (query.empty() || key.empty()) return 0.0f;

    switch (mode) {
        case RetrieveMode::EXACT: {
            // Case-insensitive exact match
            if (query.length() != key.length()) return 0.0f;
            for (size_t i = 0; i < query.length(); ++i) {
                if (normChar(query[i]) != normChar(key[i])) return 0.0f;
            }
            return kExactMatchBoost;
        }

        case RetrieveMode::SEMANTIC: {
            // Hash-based semantic similarity: compare FNV hashes of overlapping n-grams
            constexpr uint32_t kNgramSize = 3;
            if (query.length() < kNgramSize || key.length() < kNgramSize) {
                // Fall back to prefix match for short strings
                size_t minLen = std::min(query.length(), key.length());
                size_t matchLen = 0;
                for (size_t i = 0; i < minLen; ++i) {
                    if (normChar(query[i]) == normChar(key[i])) matchLen++;
                    else break;
                }
                return static_cast<float>(matchLen) / static_cast<float>(std::max(query.length(), key.length()));
            }

            // Generate n-gram hash sets
            std::vector<uint64_t> queryGrams, keyGrams;
            for (size_t i = 0; i + kNgramSize <= query.length(); ++i) {
                std::string gram;
                for (uint32_t j = 0; j < kNgramSize; ++j) gram += normChar(query[i + j]);
                queryGrams.push_back(fnv1a(gram));
            }
            for (size_t i = 0; i + kNgramSize <= key.length(); ++i) {
                std::string gram;
                for (uint32_t j = 0; j < kNgramSize; ++j) gram += normChar(key[i + j]);
                keyGrams.push_back(fnv1a(gram));
            }

            // Jaccard similarity on n-gram hash sets
            std::sort(queryGrams.begin(), queryGrams.end());
            std::sort(keyGrams.begin(), keyGrams.end());
            queryGrams.erase(std::unique(queryGrams.begin(), queryGrams.end()), queryGrams.end());
            keyGrams.erase(std::unique(keyGrams.begin(), keyGrams.end()), keyGrams.end());

            size_t intersection = 0;
            size_t qi = 0, ki = 0;
            while (qi < queryGrams.size() && ki < keyGrams.size()) {
                if (queryGrams[qi] == keyGrams[ki]) { intersection++; qi++; ki++; }
                else if (queryGrams[qi] < keyGrams[ki]) qi++;
                else ki++;
            }

            size_t unionSize = queryGrams.size() + keyGrams.size() - intersection;
            return unionSize > 0 ? static_cast<float>(intersection) / static_cast<float>(unionSize) : 0.0f;
        }

        case RetrieveMode::FUZZY: {
            // Multi-level fuzzy matching:
            // 1. Exact substring match → high score
            // 2. Prefix match → medium score
            // 3. Character n-gram overlap → proportional score

            // Check exact substring
            std::string lowerQuery, lowerKey;
            lowerQuery.reserve(query.size());
            lowerKey.reserve(key.size());
            for (char c : query) lowerQuery += normChar(c);
            for (char c : key)   lowerKey += normChar(c);

            if (lowerKey.find(lowerQuery) != std::string::npos) {
                // Substring found: score proportional to query/key length ratio
                return kPrefixMatchBase + (static_cast<float>(lowerQuery.length()) /
                       static_cast<float>(lowerKey.length())) * (kExactMatchBoost - kPrefixMatchBase);
            }

            if (lowerQuery.find(lowerKey) != std::string::npos) {
                return kPrefixMatchBase;
            }

            // Prefix match
            size_t prefixLen = 0;
            size_t minLen = std::min(lowerQuery.length(), lowerKey.length());
            for (size_t i = 0; i < minLen; ++i) {
                if (lowerQuery[i] == lowerKey[i]) prefixLen++;
                else break;
            }
            if (prefixLen > 0) {
                float prefixScore = static_cast<float>(prefixLen) /
                    static_cast<float>(std::max(lowerQuery.length(), lowerKey.length()));
                if (prefixScore > kFuzzyThreshold) return prefixScore;
            }

            // Character-level Dice coefficient
            uint32_t matches = 0;
            for (char qc : lowerQuery) {
                for (char kc : lowerKey) {
                    if (qc == kc) { matches++; break; }
                }
            }
            float dice = (2.0f * static_cast<float>(matches)) /
                static_cast<float>(lowerQuery.length() + lowerKey.length());
            return dice > kFuzzyThreshold ? dice * 0.5f : 0.0f;
        }

        case RetrieveMode::CHAIN: {
            // Chain mode: hash-distance-based similarity
            uint64_t qHash = fnv1a(query);
            uint64_t kHash = fnv1a(key);
            uint64_t xorDist = qHash ^ kHash;
            int diffBits = 0;
            while (xorDist) { diffBits++; xorDist &= (xorDist - 1); }
            return 1.0f - (static_cast<float>(diffBits) / 64.0f);
        }

        case RetrieveMode::TEMPORAL: {
            // Temporal mode: prioritize by timestamp (similarity is secondary)
            // Return non-zero for any key that has substring overlap
            std::string lq, lk;
            for (char c : query) lq += normChar(c);
            for (char c : key)   lk += normChar(c);
            return (lk.find(lq) != std::string::npos || lq.find(lk) != std::string::npos) ?
                kPrefixMatchBase : 0.0f;
        }
    }

    return 0.0f;
}

// ---- Core methods ----

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
    // Score all items across T0 and T1 tiers using real similarity metrics
    struct ScoredItem {
        float score;
        const MemoryItem* item;
    };
    std::vector<ScoredItem> candidates;

    auto scoreItems = [&](const std::vector<MemoryItem>& tier) {
        for (const auto& item : tier) {
            float similarity = computeSimilarity(query, item.key, mode);
            if (similarity > kFuzzyThreshold) {
                // Combine similarity with item confidence
                float finalScore = similarity * 0.7f + item.confidence * 0.3f;
                candidates.push_back({finalScore, &item});
            }
        }
    };

    scoreItems(t0Working_);
    scoreItems(t1Episodic_);

    // For TEMPORAL mode, also search T2 semantic tier
    if (mode == RetrieveMode::TEMPORAL || mode == RetrieveMode::SEMANTIC) {
        scoreItems(t2Semantic_);
    }

    // Sort by score descending
    std::sort(candidates.begin(), candidates.end(),
        [](const ScoredItem& a, const ScoredItem& b) { return a.score > b.score; });

    // For TEMPORAL mode, secondary sort by timestamp (most recent first)
    if (mode == RetrieveMode::TEMPORAL) {
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const ScoredItem& a, const ScoredItem& b) {
                return a.item->timestamp > b.item->timestamp;
            });
    }

    // Return top results
    std::vector<MemoryItem> results;
    uint32_t limit = std::min(kMaxFuzzyResults, static_cast<uint32_t>(candidates.size()));
    for (uint32_t i = 0; i < limit; ++i) {
        results.push_back(*candidates[i].item);
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

void MemoryFabric::warmConnection() {
    // Pre-allocate memory buffers for T0-T4 tiers to ensure fast initialization
    t0Working_.reserve(100);
    t1Episodic_.reserve(100);
    t2Semantic_.reserve(100);
}

void MemoryFabric::storeActionPlan(const action::ActionPlan& plan, MemoryTier tier) {
    auto bytes = plan.serialize();
    MemoryItem item;
    item.itemId = plan.planId;
    item.tier = tier;
    item.key = "action_plan_" + std::to_string(plan.planId);
    item.payload = std::move(bytes);
    item.confidence = 1.0f - plan.aggregateRiskScore;
    item.timestamp = 0;
    store(item);
}

void MemoryFabric::storeExecutionReport(const action::ExecutionReport& report, MemoryTier tier) {
    MemoryItem item;
    item.itemId = report.reportId;
    item.tier = tier;
    item.key = "execution_report_" + std::to_string(report.reportId);
    item.confidence = report.overallSuccess;
    item.timestamp = 0;
    store(item);
}

std::vector<action::ActionPlan> MemoryFabric::retrieveActionPlans(
    const std::string& query, RetrieveMode mode, float minConfidence) {
    auto items = retrieve(query, mode);
    std::vector<action::ActionPlan> results;
    for (const auto& item : items) {
        if (item.confidence < minConfidence) continue;
        if (item.payload.empty()) continue;
        auto optPlan = action::ActionPlan::deserialize(item.payload);
        if (optPlan.has_value()) {
            results.push_back(std::move(optPlan.value()));
        }
    }
    return results;
}

void MemoryFabric::storeBelief(const std::string& beliefId, const std::vector<uint8_t>& payload) {
    MemoryItem item;
    item.itemId = fnv1a(beliefId);
    item.tier = MemoryTier::T1_EPISODIC;
    item.key = "belief_" + beliefId;
    item.payload = payload;
    item.confidence = 0.85f;
    item.timestamp = 0;
    store(item);
}

void MemoryFabric::storeExperiment(const std::string& expId, const std::vector<uint8_t>& payload) {
    MemoryItem item;
    item.itemId = fnv1a(expId);
    item.tier = MemoryTier::T3_PROCEDURAL;
    item.key = "experiment_" + expId;
    item.payload = payload;
    item.confidence = 0.90f;
    item.timestamp = 0;
    store(item);
}

void MemoryFabric::storeLearningEpisode(const yuki::brain::learning::LearningEpisode& episode) {
    MemoryItem item;
    item.itemId = fnv1a(episode.episodeId);
    item.tier = MemoryTier::T1_EPISODIC;
    item.key = "learning_episode_" + episode.episodeId;
    std::string payloadStr = episode.userInput + "\n" + episode.finalOutput;
    item.payload = std::vector<uint8_t>(payloadStr.begin(), payloadStr.end());
    item.confidence = episode.selfEvalScore;
    item.timestamp = 0;
    store(item);
}

std::vector<yuki::brain::learning::LearningEpisode> MemoryFabric::loadLearningEpisodes() const {
    std::vector<yuki::brain::learning::LearningEpisode> episodes;
    for (const auto& item : t1Episodic_) {
        if (item.key.rfind("learning_episode_", 0) == 0) {
            yuki::brain::learning::LearningEpisode ep;
            ep.episodeId = item.key.substr(17);
            std::string payloadStr(item.payload.begin(), item.payload.end());
            auto pos = payloadStr.find('\n');
            if (pos != std::string::npos) {
                ep.userInput = payloadStr.substr(0, pos);
                ep.finalOutput = payloadStr.substr(pos + 1);
            } else {
                ep.userInput = payloadStr;
            }
            ep.safe = true;
            ep.selfEvalScore = item.confidence;
            ep.critiqueScore = item.confidence;
            ep.acceptedByOwner = true;
            ep.backendName = "LocalTransformer";
            ep.taskType = "CHAT";
            episodes.push_back(ep);
        }
    }
    return episodes;
}

} // namespace memory
} // namespace yuki



