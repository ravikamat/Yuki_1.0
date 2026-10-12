#include "brain/learning/KnowledgeDaemon.h"
#include "brain/core/Logger.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

constexpr size_t kMinTokenChars = 3;
constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;

uint64_t fnv1a(const std::string& text) {
    uint64_t hash = kFnvOffset;
    for (unsigned char c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kFnvPrime;
    }
    return hash;
}

std::string toLower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

// Splits into alphanumeric tokens of at least kMinTokenChars characters.
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : toLower(text)) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current += c;
        } else {
            if (current.size() >= kMinTokenChars) tokens.push_back(current);
            current.clear();
        }
    }
    if (current.size() >= kMinTokenChars) tokens.push_back(current);
    return tokens;
}

bool startsWithHttp(const std::string& text) {
    return text.rfind("http://", 0) == 0 || text.rfind("https://", 0) == 0;
}

// Sentence-ish split: statement terminators and newlines.
std::vector<std::string> splitStatements(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    auto flush = [&]() {
        std::string statement = trim(current);
        current.clear();
        if (!statement.empty()) out.push_back(std::move(statement));
    };
    for (char c : text) {
        if (c == '.' || c == '!' || c == '?' || c == '\n' || c == '\r' || c == ';') {
            flush();
        } else {
            current += c;
        }
    }
    flush();
    return out;
}

} // namespace

namespace yuki {
namespace brain {
namespace learning {

KnowledgeDaemon::KnowledgeDaemon(const KnowledgeDaemonConfig& cfg)
    : config_(cfg), running_(false) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "KnowledgeDaemon", "Initialized");
}

KnowledgeDaemon::~KnowledgeDaemon() {
    stop();
}

bool KnowledgeDaemon::start() {
    if (running_.exchange(true)) return false;
    worker_thread_ = std::thread(&KnowledgeDaemon::workerLoop, this);
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "KnowledgeDaemon", "Started");
    return true;
}

void KnowledgeDaemon::stop() {
    if (!running_.exchange(false)) return;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "KnowledgeDaemon", "Stopped");
}

void KnowledgeDaemon::setFetcher(Fetcher fetcher) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    fetcher_ = std::move(fetcher);
}

void KnowledgeDaemon::learnTopic(const std::string& topic, LearnPriority priority) {
    if (topic.empty()) return;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        learning_queue_.emplace_back(topic, static_cast<int>(priority));
    }
    queue_cv_.notify_one();
}

void KnowledgeDaemon::learnFromText(const std::string& text, const std::string& source) {
    extractAndStoreFacts(text, source);
}

void KnowledgeDaemon::learnFromUrl(const std::string& url) {
    if (url.empty()) return;

    Fetcher fetcher;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        fetcher = fetcher_;
    }

    if (!fetcher) {
        storeFact(url, "URL queued for learning (no fetcher configured)", "queue", 0.2f);
        if (!fetcher_missing_warned_) {
            fetcher_missing_warned_ = true;
            yuki::core::Logger::instance().log(
                yuki::core::LogLevel::WARN, "KnowledgeDaemon",
                "learnFromUrl() called before setFetcher(); URL recorded as pending");
        }
        return;
    }

    std::string body;
    try {
        body = fetcher(url);
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "KnowledgeDaemon",
                                           std::string("Fetch failed: ") + e.what());
    } catch (...) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "KnowledgeDaemon",
                                           "Fetch failed with unknown error");
    }

    if (body.empty()) {
        ++fetched_failed_;
        return;
    }
    ++fetched_ok_;
    extractAndStoreFacts(body, url);
}

std::string KnowledgeDaemon::query(const std::string& query, int max_results) {
    const std::vector<std::string> query_tokens = tokenize(query);
    if (query_tokens.empty() || max_results <= 0) return {};

    std::lock_guard<std::mutex> lock(store_mutex_);
    std::unordered_set<std::string> context(context_keywords_.begin(), context_keywords_.end());

    struct Hit {
        const KnowledgeFact* fact;
        float score;
    };
    std::vector<Hit> hits;
    for (const KnowledgeFact& fact : facts_) {
        const std::string haystack = toLower(fact.topic + " " + fact.text);
        float score = 0.0f;
        for (const std::string& token : query_tokens) {
            if (haystack.find(token) != std::string::npos) score += 1.0f;
        }
        if (score <= 0.0f) continue;
        score *= fact.confidence;
        if (context.count(toLower(fact.topic))) score += 0.5f;  // context bias
        hits.push_back({&fact, score});
    }

    if (hits.empty()) {
        return "No learned knowledge matches \"" + query + "\" yet (" +
               std::to_string(facts_.size()) + " facts stored).";
    }

    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        return a.score > b.score;
    });

    const size_t limit = (std::min)(hits.size(), static_cast<size_t>(max_results));
    std::ostringstream out;
    for (size_t i = 0; i < limit; ++i) {
        if (i > 0) out << "\n";
        out << "- [" << (hits[i].fact->topic.empty() ? "general" : hits[i].fact->topic)
            << "] " << hits[i].fact->text;
    }
    return out.str();
}

std::vector<std::string> KnowledgeDaemon::getKnownTopics() const {
    std::lock_guard<std::mutex> lock(store_mutex_);
    std::vector<std::string> topics(known_topics_.begin(), known_topics_.end());
    std::sort(topics.begin(), topics.end());
    return topics;
}

std::vector<KnowledgeDaemon::KnowledgeFact> KnowledgeDaemon::snapshotFacts() const {
    std::lock_guard<std::mutex> lock(store_mutex_);
    return std::vector<KnowledgeFact>(facts_.begin(), facts_.end());
}

void KnowledgeDaemon::updateContextKeywords(const std::string& text) {
    std::vector<std::string> tokens = tokenize(text);
    if (tokens.empty()) return;

    std::lock_guard<std::mutex> lock(store_mutex_);
    for (std::string& token : tokens) {
        auto it = std::find(context_keywords_.begin(), context_keywords_.end(), token);
        if (it != context_keywords_.end()) context_keywords_.erase(it);
        context_keywords_.push_back(std::move(token));
    }
    while (context_keywords_.size() > config_.max_context_keywords) {
        context_keywords_.pop_front();
    }
}

size_t KnowledgeDaemon::factCount() const {
    std::lock_guard<std::mutex> lock(store_mutex_);
    return facts_.size();
}

size_t KnowledgeDaemon::topicCount() const {
    std::lock_guard<std::mutex> lock(store_mutex_);
    return known_topics_.size();
}

size_t KnowledgeDaemon::pendingTopicCount() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return learning_queue_.size();
}

size_t KnowledgeDaemon::fetchedOkCount() const { return fetched_ok_.load(); }
size_t KnowledgeDaemon::fetchedFailedCount() const { return fetched_failed_.load(); }

void KnowledgeDaemon::workerLoop() {
    while (true) {
        std::pair<std::string, int> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wake on new work or shutdown instead of spinning once per second
            // (audit §3: sleep_for poll loops).
            queue_cv_.wait_for(lock, std::chrono::seconds(5), [this] {
                return !running_.load() || !learning_queue_.empty();
            });
            if (!running_.load()) break;
            if (learning_queue_.empty()) continue;

            // Lower enum value == more urgent (P0_URGENT == 0).
            auto best = std::min_element(
                learning_queue_.begin(), learning_queue_.end(),
                [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                    return a.second < b.second;
                });
            job = *best;
            learning_queue_.erase(best);
        }
        processTopic(job.first, job.second);
    }
}

void KnowledgeDaemon::processQueue() {
    std::vector<std::pair<std::string, int>> batch;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        batch.swap(learning_queue_);
    }
    for (const auto& job : batch) {
        processTopic(job.first, job.second);
    }
}

void KnowledgeDaemon::processTopic(const std::string& topic, int priority) {
    if (topic.empty()) return;

    yuki::core::Logger::instance().log(
        yuki::core::LogLevel::DEBUG, "KnowledgeDaemon",
        "Processing topic '" + topic + "' (priority " + std::to_string(priority) + "/4)");

    if (startsWithHttp(topic)) {
        learnFromUrl(topic);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(store_mutex_);
        known_topics_.insert(topic);
    }

    Fetcher fetcher;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        fetcher = fetcher_;
    }

    if (!fetcher) {
        storeFact(topic, "Topic registered for background learning", "curriculum", 0.3f);
        return;
    }

    std::string body;
    try {
        body = fetcher(topic);
    } catch (...) {
        body.clear();
    }
    if (body.empty()) {
        ++fetched_failed_;
        storeFact(topic, "Topic registered; fetch produced no text", "curriculum", 0.25f);
        return;
    }
    ++fetched_ok_;
    storeFact(topic, "Topic registered for background learning", "curriculum", 0.3f);
    extractAndStoreFacts(body, "topic:" + topic);
}

void KnowledgeDaemon::extractAndStoreFacts(const std::string& text, const std::string& source) {
    if (text.empty()) return;

    size_t added = 0;
    for (const std::string& statement : splitStatements(text)) {
        if (statement.size() < config_.min_fact_chars) continue;

        const uint64_t hash = fnv1a(toLower(statement));
        {
            std::lock_guard<std::mutex> lock(store_mutex_);
            if (fact_hashes_.count(hash)) continue;
            fact_hashes_.insert(hash);
        }
        storeFact(inferTopic(statement), statement, source, 0.5f);
        ++added;
    }

    if (added > 0) {
        yuki::core::Logger::instance().log(
            yuki::core::LogLevel::INFO, "KnowledgeDaemon",
            "Learned " + std::to_string(added) + " fact(s) from " + source);
    }
}

std::string KnowledgeDaemon::inferTopic(const std::string& sentence) const {
    const std::vector<std::string> tokens = tokenize(sentence);
    if (tokens.empty()) return "general";

    {
        std::lock_guard<std::mutex> lock(store_mutex_);
        for (const std::string& keyword : context_keywords_) {
            if (sentence.find(keyword) != std::string::npos) return keyword;
        }
    }
    return tokens.front();
}

void KnowledgeDaemon::storeFact(const std::string& topic, const std::string& text,
                                const std::string& source, float confidence) {
    std::lock_guard<std::mutex> lock(store_mutex_);
    known_topics_.insert(topic);
    KnowledgeFact fact;
    fact.topic = topic;
    fact.text = text;
    fact.source = source;
    fact.confidence = confidence;
    fact.learned_at = std::chrono::system_clock::now();
    facts_.push_back(std::move(fact));

    while (facts_.size() > config_.max_facts) {
        facts_.pop_front();
    }
}

} // namespace learning
} // namespace brain
} // namespace yuki