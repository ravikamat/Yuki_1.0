#pragma once
// KnowledgeDaemon.h
// Yuki_1.0 — Self-Learning Knowledge Layer
//
// Manages the Python knowledge daemon subprocess.
// The daemon crawls Simple English Wikipedia in the background and answers
// factual questions from its accumulated SQLite knowledge base.
//
// Thread-safe. All public methods may be called from any thread.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <future>
#include <map>
#include <vector>
#include <deque>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "RuntimeWorkerBase.h"

namespace yuki { namespace memory { class CognitiveMemoryFabric; } }

struct KnowledgeAnswer {
    bool        found      = false;
    std::string text;        // empty if not found
    std::string topic;       // what topic the answer is about
    float       confidence = 0.0f;
    std::vector<std::string> related;  // Level 3: graph-connected topic names
};

// Layer 2/5: domain interest profile returned by the daemon
struct InterestProfile {
    std::map<std::string, int> domains;   // domain → article count
    std::string topDomain;               // highest-count domain
    std::string selfSummary;             // "I know a lot about psychology..."
    int         totalFacts = 0;
    bool        loaded     = false;      // false until first 'interests' response received
};

class KnowledgeDaemon : public RuntimeWorkerBase {
public:
    KnowledgeDaemon();
    ~KnowledgeDaemon() override;

    // Launch the Python daemon.  Returns true if started OK.
    bool start();

    // Gracefully stop and wait for the daemon to exit.
    void stop();

    bool isRunning() const { return running_; }

    // How many topics the daemon has learned this session.
    int factsLearned() const { return factsLearned_.load(); }
    int topicsLearned() const { return topicsLearned_.load(); }

    // Query the knowledge base.  Blocks up to timeoutMs waiting for an answer.
    // Returns {found=false} on timeout or if daemon is not running.
    KnowledgeAnswer query(const std::string& question, int timeoutMs = 400);

    // Translate text using the python daemon. Blocks up to timeoutMs.
    std::string translate(const std::string& text, int timeoutMs = 1500);

    // Layer 2/5: fire-and-forget interests request.
    // The daemon responds asynchronously; readLoop() caches the result.
    void requestInterests();

    // Layer 2/5: get latest cached interest profile (immediately, no blocking).
    InterestProfile getInterestProfile() const;

    // Tell the daemon to queue a topic for background learning.
    // Priority: P0=user asked and we didn't know (most urgent),
    //           P1=user stated interest, P2=general curriculum (default)
    enum class LearnPriority { P0_URGENT, P1_INTEREST, P2_GENERAL };
    void learnTopic(const std::string& topic,
                    LearnPriority priority = LearnPriority::P2_GENERAL);

    // Optional callback invoked each time the daemon learns a new topic.
    using LearningCallback = std::function<void(const std::string& topic, int count)>;
    void setLearningCallback(LearningCallback cb);

    void deferQuery(const std::string& query);
    void processDeferredQueries();

    void setMemoryFabric(std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf);

    // Knowledge packet ring buffer — filled by the daemon's learning loop.
    struct KnowledgePacket {
        std::string source_url;
        std::string topic;
        std::string summary;
        std::vector<std::string> key_entities;
        float    confidence    = 0.0f;
        uint64_t timestamp_ms  = 0;
    };
    // Returns the N most-recent packets (thread-safe).
    std::vector<KnowledgePacket> getRecentPackets(size_t n) const;

private:
    void readLoop();           // background thread reading daemon stdout

    bool    sendLine(const std::string& json);
    bool    waitForReady(int timeoutMs);

    // Tiny JSON helpers (no external deps)
    static std::string jStr(const std::string& s);
    static bool parseJson(const std::string& line,
                          std::string& typeOut,
                          std::string& textOut,
                          std::string& topicOut,
                          float&       confidenceOut,
                          bool&        foundOut,
                          int&         idOut,
                          int&         countOut,
                          std::vector<std::string>& relatedOut);

    HANDLE hProc_    = INVALID_HANDLE_VALUE;
    HANDLE hThread_  = INVALID_HANDLE_VALUE;
    HANDLE hWrite_   = INVALID_HANDLE_VALUE;   // write to daemon stdin
    HANDLE hRead_    = INVALID_HANDLE_VALUE;   // read daemon stdout

    std::atomic<bool> running_  {false};
    std::atomic<bool> ready_    {false};
    std::atomic<int>  factsLearned_  {0};
    std::atomic<int>  topicsLearned_ {0};

    struct PendingQuery {
        KnowledgeAnswer         answer;
        bool                    resolved = false;
        std::condition_variable cv;
        std::mutex              mtx;
    };
    std::mutex                                        queryMutex_;
    std::map<int, std::shared_ptr<PendingQuery>>     pending_;    // Fixed: use shared_ptr to avoid use-after-free
    std::atomic<int>                                 nextId_ {1};

    std::mutex              readyMutex_;
    std::condition_variable readyCv_;

    LearningCallback learningCb_;
    std::mutex       cbMutex_;

    // Layer 2/5: cached interest profile updated by readLoop()
    mutable std::mutex interestMutex_;
    InterestProfile    interestProfile_;

    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf_;

    // Packet ring buffer (thread-safe via packet_mutex_)
    mutable std::mutex            packet_mutex_;
    std::deque<KnowledgePacket>   recent_packets_;
    static constexpr size_t       MAX_PACKETS = 100;

    // Section 8: Context relevance filter — skip topics unrelated to user input
    std::vector<std::string> recent_context_keywords_;

    bool isRelevantToContext(const std::string& topic) const {
        if (recent_context_keywords_.empty()) return true;
        std::string lower_topic = topic;
        std::transform(lower_topic.begin(), lower_topic.end(), lower_topic.begin(), ::tolower);
        for (const auto& kw : recent_context_keywords_) {
            if (lower_topic.find(kw) != std::string::npos) return true;
        }
        return false;
    }

public:
    // Update context keywords from user input (call after each user turn)
    void updateContextKeywords(const std::string& user_input) {
        std::istringstream iss(user_input);
        std::string word;
        while (iss >> word) {
            // Lowercase and strip punctuation
            std::string clean;
            for (char c : word)
                if (std::isalpha(static_cast<unsigned char>(c)))
                    clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (clean.length() > 3) {
                recent_context_keywords_.push_back(clean);
            }
        }
        if (recent_context_keywords_.size() > 20) {
            recent_context_keywords_.erase(recent_context_keywords_.begin(),
                                           recent_context_keywords_.begin() +
                                           static_cast<std::ptrdiff_t>(recent_context_keywords_.size() - 20));
        }
    }
};
