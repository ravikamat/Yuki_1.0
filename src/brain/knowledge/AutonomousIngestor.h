#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace yuki::memory { class ConceptNetIngestor; }
namespace yuki::language { class GrammarExtractor; }
namespace yuki::knowledge { class PhysicsKnowledgeBase; }
namespace yuki::ethics { class ValueConstitution; }

namespace yuki::knowledge {

struct IngestionJob {
    uint64_t job_id = 0;
    std::string source_type;    // "conceptnet", "corpus", "physics", "gita"
    std::string source_path;
    size_t max_items = 0;       // 0 = unlimited
    float priority = 1.0f;
    std::vector<std::string> target_domains;
};

struct IngestionProgress {
    uint64_t job_id = 0;
    uint64_t processed = 0;
    uint64_t accepted = 0;
    uint64_t failed = 0;
    bool complete = false;
    uint64_t elapsed_ms = 0;
};

class AutonomousIngestor {
public:
    AutonomousIngestor(yuki::memory::ConceptNetIngestor* cn_ingestor,
                       yuki::language::GrammarExtractor* grammar_extractor,
                       PhysicsKnowledgeBase* physics_kb,
                       yuki::ethics::ValueConstitution* constitution);

    uint64_t queueJob(const IngestionJob& job);
    IngestionProgress processJob(uint64_t job_id);
    IngestionProgress getProgress(uint64_t job_id) const;
    void cancelJob(uint64_t job_id);

    // Auto-queue based on knowledge gap domain
    uint64_t autoQueueForGap(const std::string& gap_domain, float priority);

    size_t pendingJobsCount() const;

private:
    yuki::memory::ConceptNetIngestor* cn_ingestor_;
    yuki::language::GrammarExtractor* grammar_extractor_;
    PhysicsKnowledgeBase* physics_kb_;
    yuki::ethics::ValueConstitution* constitution_;

    std::unordered_map<uint64_t, IngestionJob> jobs_;
    std::unordered_map<uint64_t, IngestionProgress> progress_;
    std::atomic<uint64_t> next_job_id_{1};
    mutable std::mutex mtx_;
    std::atomic<bool> cancel_flag_{false};
};

} // namespace yuki::knowledge
