#pragma once
// LearningIngestor.h — v2
// Yuki_1.0 — Intelligent Background Learning Layer
//
// Pipeline per item:
//   Filter → Source-score → Dedup (Jaccard) → Contradiction → Store → Graph-link
//
// Thread-safe. All public methods may be called from any thread.

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <memory>

// Forward declarations
namespace yuki { namespace swarm { class ISemanticStore; } }

#include "brain/memory/HeuristicMemory.h"

// A single item of knowledge to evaluate and potentially store
struct LearnItem {
    std::string topic;        // normalized lowercase topic key
    std::string fact;         // the fact text to store
    std::string source;       // "daemon", "wikipedia", "user_stated", "web_fallback"
    float       confidence;   // raw confidence 0.0–1.0 (before source scoring)
    int64_t     timestamp;    // unix seconds
    std::string related;      // pipe-separated related topic names (graph links)
};

// Result from contradiction check
struct ContradictionResult {
    bool        isDuplicate  = false;   // near-identical to existing fact
    bool        isConflict   = false;   // contradicts existing fact
    std::string similarFact;
    std::string conflictFact;
    float       similarity   = 0.0f;
};

struct CurriculumWeights {
    float english = 0.25f;
    float math = 0.25f;
    float programming = 0.25f;
    float trading = 0.25f;
    
    void normalize() {
        float sum = english + math + programming + trading;
        if (sum > 0) {
            english /= sum; math /= sum; programming /= sum; trading /= sum;
        }
    }
};

class LearningIngestor {
public:
    static LearningIngestor& instance();

    // Start the background worker thread. Call once at startup.
    void start();

    // Stop the background worker gracefully. Call at shutdown.
    void stop();

    // Submit a fact for background evaluation and storage.
    // Non-blocking: puts item on the queue and returns immediately.
    void submit(const LearnItem& item);

    // Convenience: submit a result from the KnowledgeDaemon.
    void submitFromDaemon(const std::string& topic,
                          const std::string& fact,
                          float confidence,
                          const std::string& related = "");

    /// Configure the singleton with an ISemanticStore for dynamic heuristic loading.
    /// Must be called once at startup (after init()) before any items are submitted.
    /// Thread-safe: uses the internal mutex.
    static void configure(yuki::swarm::ISemanticStore* store);

    /// Callback: inform the ingestor whether the last ingestion was good (increased knowledge).
    /// Adjusts MIN_CONFIDENCE_DYNAMIC and updates negation keywords.
    void onIngestionOutcome(const std::string& negationWord, bool wasNegation);

private:
    LearningIngestor() = default;
    ~LearningIngestor(); // out-of-line: unique_ptr<SemanticHeuristicAccess> needs complete type
    LearningIngestor(const LearningIngestor&) = delete;
    LearningIngestor& operator=(const LearningIngestor&) = delete;

    // ── Intelligence layer ────────────────────────────────────────────────────────────────────────────────────

    // Returns true if item passes all quality filters
    bool shouldLearn(const LearnItem& item) const;

    // Source quality multiplier (0.0–1.0)
    float scoreSource(const std::string& source) const;

    // Jaccard similarity on word sets (0.0 = different, 1.0 = identical)
    float jaccardSimilarity(const std::string& a, const std::string& b) const;

    // Check for duplicate or conflicting fact against DB
    ContradictionResult checkContradiction(const std::string& topic,
                                           const std::string& newFact) const;

    // Normalize topic key: lowercase, trim, collapse spaces
    static std::string normalizeTopic(const std::string& raw);

    void workerLoop();

    std::queue<LearnItem>    queue_;
    std::mutex               queueMutex_;
    std::condition_variable  cv_;
    std::thread              worker_;
    std::atomic<bool>        running_ {false};

    // Static constexpr thresholds (compile-time fallbacks; dynamic values override these at runtime)
    static constexpr float  MIN_CONFIDENCE      = 0.40f;
    static constexpr size_t MIN_FACT_LENGTH     = 10;
    static constexpr size_t MAX_FACT_LENGTH     = 2000;
    static constexpr float  DUPLICATE_THRESHOLD = 0.72f;  // Jaccard >= this = duplicate
    static constexpr size_t MAX_QUEUE_SIZE      = 500;

    // ── Dynamic heuristic overrides (set by configure()) ──────────────────────────────────
    // Loaded via SemanticHeuristicAccess. If store is null, constexpr values are used.
    float  minConfidenceDynamic_{MIN_CONFIDENCE};
    float  duplicateThreshDynamic_{DUPLICATE_THRESHOLD};
    std::vector<std::string> negationKeywords_; // dynamic list; grows with onIngestionOutcome()

    std::unique_ptr<yuki::brain::memory::SemanticHeuristicAccess> heuristic_;
    void loadDynamicThresholds();
};
