// LearningIngestor.cpp
// Yuki_1.0 — Intelligent Background Learning Layer v2
//
// Responsibilities:
//   1. Filter: quality, length, HTML, confidence floor
//   2. Deduplication: Jaccard similarity check before inserting
//   3. Contradiction detection: negation signals against existing facts
//   4. Confidence update: boost on multi-source agreement, penalize on conflict
//   5. Graph links: store related topics from daemon/web answers
//   6. Persistence: storeLearned() with full metadata

#include "brain/learning/LearningIngestor.h"
#include "database/DatabaseManager.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <set>
#include <cmath>

LearningIngestor& LearningIngestor::instance() {
    static LearningIngestor inst;
    return inst;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void LearningIngestor::start() {
    if (running_.load()) return;
    running_ = true;
    worker_  = std::thread([this]() { workerLoop(); });
    std::cout << "[Ingestor] Background learning started (v2: dedup+conflict+graph).\n";
}

void LearningIngestor::stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    std::cout << "[Ingestor] Background learning stopped.\n";
}

// ── Public API ─────────────────────────────────────────────────────────────

void LearningIngestor::submit(const LearnItem& item) {
    if (!running_.load()) return;
    LearnItem normalized = item;
    normalized.topic = normalizeTopic(item.topic);
    if (normalized.topic.empty()) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        // Bound the queue: drop oldest if overfull
        if (queue_.size() >= MAX_QUEUE_SIZE) {
            queue_.pop();
            std::cerr << "[Ingestor] Queue full — dropped oldest item.\n";
        }
        queue_.push(normalized);
    }
    cv_.notify_one();
}

void LearningIngestor::submitFromDaemon(const std::string& topic,
                                         const std::string& fact,
                                         float confidence,
                                         const std::string& related)
{
    if (topic.empty() || fact.empty()) return;
    LearnItem item;
    item.topic      = topic;
    item.fact       = fact;
    item.source     = "daemon";
    item.confidence = confidence;
    item.related    = related;
    item.timestamp  = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    submit(item);
}

// ── Filter: shouldLearn ────────────────────────────────────────────────────

bool LearningIngestor::shouldLearn(const LearnItem& item) const {
    if (item.confidence < MIN_CONFIDENCE) {
        std::cout << "[Ingestor] Skip (low conf=" << item.confidence
                  << ") topic=" << item.topic << "\n";
        return false;
    }
    if (item.fact.size() < MIN_FACT_LENGTH) {
        std::cout << "[Ingestor] Skip (too short) topic=" << item.topic << "\n";
        return false;
    }
    if (item.fact.size() > MAX_FACT_LENGTH) {
        std::cout << "[Ingestor] Skip (too long=" << item.fact.size()
                  << ") topic=" << item.topic << "\n";
        return false;
    }
    // Reject raw HTML
    if (item.fact.find("Jump to content")  != std::string::npos ||
        item.fact.find("Main menu")        != std::string::npos ||
        item.fact.find("</")               != std::string::npos ||
        item.fact.find("<!DOCTYPE")        != std::string::npos ||
        item.fact.find("window.onload")    != std::string::npos) {
        std::cout << "[Ingestor] Skip (HTML garbage) topic=" << item.topic << "\n";
        return false;
    }
    // Reject facts that are clearly navigational boilerplate
    if (item.fact.find("Retrieved from") != std::string::npos ||
        item.fact.find("Privacy policy")  != std::string::npos ||
        item.fact.find("Terms of use")    != std::string::npos) {
        std::cout << "[Ingestor] Skip (navigation boilerplate) topic=" << item.topic << "\n";
        return false;
    }
    return true;
}

// ── Source quality scoring (0.0–1.0) ──────────────────────────────────────
// Higher-quality sources get a multiplier applied before storage.

float LearningIngestor::scoreSource(const std::string& source) const {
    if (source == "bootstrap")       return 1.00f;   // identity facts — authoritative
    if (source == "user_stated")     return 0.95f;   // user told us directly
    if (source.find("daemon")        != std::string::npos) return 0.85f; // python daemon wiki
    if (source.find("wikipedia")     != std::string::npos) return 0.80f;
    if (source.find("web_fallback")  != std::string::npos) return 0.65f;
    if (source == "router")          return 0.75f;
    return 0.50f;   // unknown — neutral
}

// ── Deduplication: Jaccard similarity on word sets ────────────────────────
// Returns 0.0 (completely different) to 1.0 (identical) for two fact strings.

float LearningIngestor::jaccardSimilarity(const std::string& a, const std::string& b) const {
    auto tokenize = [](const std::string& s) -> std::set<std::string> {
        std::set<std::string> tokens;
        std::istringstream ss(s);
        std::string word;
        while (ss >> word) {
            // lowercase + strip punctuation
            std::string clean;
            for (char c : word) {
                if (std::isalnum(static_cast<unsigned char>(c)))
                    clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (clean.size() > 2) tokens.insert(clean);  // skip stop words by length
        }
        return tokens;
    };

    auto sa = tokenize(a);
    auto sb = tokenize(b);
    if (sa.empty() && sb.empty()) return 1.0f;
    if (sa.empty() || sb.empty()) return 0.0f;

    std::set<std::string> intersect, uni;
    for (const auto& t : sa) {
        if (sb.count(t)) intersect.insert(t);
        uni.insert(t);
    }
    for (const auto& t : sb) uni.insert(t);
    return static_cast<float>(intersect.size()) / static_cast<float>(uni.size());
}

// ── Contradiction detection ────────────────────────────────────────────────
// Looks for negation indicators that suggest the new fact contradicts stored ones.
// Not semantic — uses signal words as a heuristic (sufficient without an LLM).

static bool containsNegation(const std::string& text) {
    static const char* const NEGATIONS[] = {
        " not ", " no ", " never ", " incorrect", " wrong", " false",
        " opposite", " contrary", " unlike", " denied", " disproven",
        nullptr
    };
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    for (int i = 0; NEGATIONS[i]; ++i) {
        if (lower.find(NEGATIONS[i]) != std::string::npos)
            return true;
    }
    return false;
}

ContradictionResult LearningIngestor::checkContradiction(
    const std::string& topic,
    const std::string& newFact) const
{
    ContradictionResult res;
    auto existingFacts = DatabaseManager::instance().queryAllFacts(topic, 0.3f);
    if (existingFacts.empty()) return res;

    for (const auto& existing : existingFacts) {
        // High similarity = duplicate — not a contradiction
        float sim = jaccardSimilarity(existing, newFact);
        if (sim >= DUPLICATE_THRESHOLD) {
            res.isDuplicate = true;
            res.similarFact = existing;
            res.similarity  = sim;
            return res;
        }

        // If both facts contain negation signals and are on the same topic,
        // flag as a potential conflict
        bool newHasNeg  = containsNegation(newFact);
        bool oldHasNeg  = containsNegation(existing);
        if (newHasNeg != oldHasNeg && sim > 0.25f) {
            // One says something, the other denies it
            res.isConflict  = true;
            res.conflictFact = existing;
            res.similarity   = sim;
            return res;
        }
    }
    return res;
}

// ── Topic normalization ────────────────────────────────────────────────────

std::string LearningIngestor::normalizeTopic(const std::string& raw) {
    std::string s = raw;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    s = s.substr(start, end - start + 1);
    std::string out;
    out.reserve(s.size());
    bool lastSpace = false;
    for (char c : s) {
        if (c == ' ') { if (!lastSpace) out += c; lastSpace = true; }
        else          { out += c; lastSpace = false; }
    }
    return out;
}

// ── Background worker ──────────────────────────────────────────────────────

void LearningIngestor::workerLoop() {
    while (running_.load()) {
        LearnItem item;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this]{ return !queue_.empty() || !running_.load(); });
            if (!running_.load() && queue_.empty()) break;
            item = queue_.front();
            queue_.pop();
        }

        // 1. Quality filter
        if (!shouldLearn(item)) continue;

        // 2. Apply source quality multiplier to confidence
        float srcScore = scoreSource(item.source);
        float effectiveConf = item.confidence * srcScore;
        if (effectiveConf < MIN_CONFIDENCE) {
            std::cout << "[Ingestor] Skip after source scoring: " << item.topic << "\n";
            continue;
        }

        // 3. Deduplication + contradiction check
        ContradictionResult cr = checkContradiction(item.topic, item.fact);

        if (cr.isDuplicate) {
            // Same fact from another source = corroboration → boost confidence
            std::cout << "[Ingestor] Duplicate (sim=" << cr.similarity
                      << ") for topic=" << item.topic << " — boosting existing.\n";
            // Boost all existing records for this topic from any source
            DatabaseManager::instance().boostConfidence(item.topic, item.source, 0.08f);
            continue;
        }

        std::string conflictStatus = "ok";
        if (cr.isConflict) {
            std::cout << "[Ingestor] Conflict detected for topic=" << item.topic
                      << " (sim=" << cr.similarity << ") — penalizing older facts.\n";
            // Penalize all existing sources EXCEPT the incoming one.
            // We know at least one conflicts, penalize all stored versions.
            // (They may have come from web_fallback, daemon, router — all weaker than user_stated)
            // Use boostConfidence with negative sign via penalizeConflict on each existing source.
            // Since we don't have the source names here, apply to every record for this topic.
            // The SQL UPDATE uses MAX(0.0, confidence-penalty) — safe to apply broadly.
            DatabaseManager::instance().penalizeConflict(item.topic, item.source, 0.15f);
            effectiveConf = std::max(0.35f, effectiveConf - 0.10f);
            conflictStatus = "conflict";
        }

        // 4. Store with graph links
        bool ok = DatabaseManager::instance().storeLearned(
            item.topic,
            item.fact,
            item.source,
            effectiveConf,
            item.timestamp,
            item.related,
            conflictStatus);

        // 5. Store related topic graph links
        if (ok && !item.related.empty()) {
            DatabaseManager::instance().storeRelated(item.topic, item.related);
        }

        if (ok) {
            std::cout << "[Ingestor] Stored: topic=" << item.topic
                      << " conf=" << effectiveConf
                      << " src=" << item.source
                      << " status=" << conflictStatus << "\n";
        } else {
            std::cerr << "[Ingestor] DB store failed for topic=" << item.topic << "\n";
        }
    }
}
