#pragma once
// KnowledgeDaemon — background, self-directed knowledge acquisition.
//
// Owns what Yuki actually knows: curriculum topics queued for learning, facts
// distilled from text, and pages fetched through an optional host-provided
// fetcher. Knowledge lives in a bounded in-memory store, queries are answered
// by token overlap over that store, and the observability counters report the
// real contents. Earlier revisions were empty stubs that logged success while
// storing nothing (AUDIT_REPORT.md §1).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace yuki {
namespace brain {
namespace learning {

struct KnowledgeDaemonConfig {
    std::chrono::hours sync_interval = std::chrono::hours(24);
    int max_concurrent_downloads = 3;
    std::vector<std::string> trusted_domains;

    // Store bounds: oldest facts are dropped once max_facts is reached so a
    // long-running session cannot grow without limit.
    size_t max_facts = 50000;
    size_t min_fact_chars = 12;        // fragments shorter than this are noise
    size_t max_context_keywords = 64;  // recency-bounded context window
};

class KnowledgeDaemon {
public:
    enum class LearnPriority {
        P0_URGENT = 0,
        P1_HIGH = 1,
        P2_NORMAL = 2,
        P3_LOW = 3,
        P4_BACKGROUND = 4
    };

    // One distilled assertion.
    struct KnowledgeFact {
        std::string topic;
        std::string text;
        std::string source;
        float confidence = 0.5f;
        std::chrono::system_clock::time_point learned_at;
    };

    explicit KnowledgeDaemon(const KnowledgeDaemonConfig& cfg = KnowledgeDaemonConfig{});
    ~KnowledgeDaemon();
    
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    void learnTopic(const std::string& topic, LearnPriority priority = LearnPriority::P2_NORMAL);
    void learnFromText(const std::string& text, const std::string& source = "unknown");
    void learnFromUrl(const std::string& url);

    // Fetch hook injected by the host (e.g. SmartScraper). Receives a topic or
    // URL and returns raw text; when absent, learnFromUrl() records the URL as
    // pending instead of pretending the fetch happened.
    using Fetcher = std::function<std::string(const std::string& request)>;
    void setFetcher(Fetcher fetcher);

    std::string query(const std::string& query, int max_results = 5);
    std::vector<std::string> getKnownTopics() const;
    std::vector<KnowledgeFact> snapshotFacts() const;

    // Feeds recent user input so lookups can bias towards the current context.
    void updateContextKeywords(const std::string& text);

    // Set relevance topics from SelfModel gaps for filtering learned facts.
    // Only facts matching these topics (by token overlap) will be stored.
    void setRelevanceTopics(const std::vector<std::string>& topics);

    // Observability: real contents, not placeholders.
    size_t factCount() const;
    size_t topicCount() const;
    size_t pendingTopicCount() const;
    size_t fetchedOkCount() const;
    size_t fetchedFailedCount() const;
    
private:
    KnowledgeDaemonConfig config_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<std::pair<std::string, int>> learning_queue_;

    mutable std::mutex store_mutex_;
    std::deque<KnowledgeFact> facts_;
    std::unordered_set<std::string> known_topics_;
    std::unordered_set<uint64_t> fact_hashes_;
    std::deque<std::string> context_keywords_;

    Fetcher fetcher_;
    bool fetcher_missing_warned_ = false;
    std::atomic<size_t> fetched_ok_{0};
    std::atomic<size_t> fetched_failed_{0};

    // Relevance filtering: topics from SelfModel.getGaps() that we care about learning
    mutable std::mutex relevance_mutex_;
    std::unordered_set<std::string> relevance_topics_;

    void workerLoop();
    void processQueue();
    void processTopic(const std::string& topic, int priority);
    void extractAndStoreFacts(const std::string& text, const std::string& source);
    std::string inferTopic(const std::string& sentence) const;
    void storeFact(const std::string& topic, const std::string& text,
                   const std::string& source, float confidence);
};

} // namespace learning
} // namespace brain
} // namespace yuki