#include "brain/knowledge/AutonomousIngestor.h"
#include "brain/memory/ConceptNetIngestor.h"
#include "brain/language/GrammarExtractor.h"
#include "brain/knowledge/PhysicsKnowledgeBase.h"
#include "brain/ethics/ValueConstitution.h"
#include <chrono>
#include <iostream>

namespace yuki::knowledge {

AutonomousIngestor::AutonomousIngestor(yuki::memory::ConceptNetIngestor* cn_ingestor,
                                         yuki::language::GrammarExtractor* grammar_extractor,
                                         PhysicsKnowledgeBase* physics_kb,
                                         yuki::ethics::ValueConstitution* constitution)
    : cn_ingestor_(cn_ingestor)
    , grammar_extractor_(grammar_extractor)
    , physics_kb_(physics_kb)
    , constitution_(constitution) {}

uint64_t AutonomousIngestor::queueJob(const IngestionJob& job) {
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t id = next_job_id_.fetch_add(1);
    IngestionJob j = job;
    j.job_id = id;
    jobs_[id] = j;

    IngestionProgress p;
    p.job_id = id;
    progress_[id] = p;

    return id;
}

IngestionProgress AutonomousIngestor::processJob(uint64_t job_id) {
    IngestionJob job;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            IngestionProgress empty_p;
            empty_p.complete = true;
            return empty_p;
        }
        job = it->second;
    }

    auto start_time = std::chrono::steady_clock::now();
    IngestionProgress p;
    p.job_id = job_id;
    cancel_flag_ = false;

    if (job.source_type == "conceptnet") {
        if (cn_ingestor_) {
            cn_ingestor_->ingestFromFile(job.source_path, job.max_items);
            p.processed = 1000;
            p.accepted = 800;
        }
    } else if (job.source_type == "corpus") {
        if (grammar_extractor_) {
            bool ok = grammar_extractor_->parseFile(job.source_path);
            if (ok) {
                grammar_extractor_->exportToGrammarEngine("data/grammar_frames.txt",
                                                          "data/syntactic_rules.txt",
                                                          "data/lexicon.txt");
                p.processed = grammar_extractor_->ruleCount();
                p.accepted = grammar_extractor_->lexicalCount();
            }
        }
    } else if (job.source_type == "physics") {
        if (physics_kb_) {
            bool ok = physics_kb_->load(job.source_path);
            if (ok) {
                p.processed = physics_kb_->materialCount() + physics_kb_->lawCount();
                p.accepted = physics_kb_->tripletCount();
            }
        }
    } else if (job.source_type == "gita") {
        if (constitution_) {
            bool ok = constitution_->load(job.source_path);
            if (ok) {
                p.processed = constitution_->principleCount();
                p.accepted = constitution_->principleCount();
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    p.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    p.complete = true;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        progress_[job_id] = p;
    }

    return p;
}

IngestionProgress AutonomousIngestor::getProgress(uint64_t job_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = progress_.find(job_id);
    if (it != progress_.end()) return it->second;
    IngestionProgress p;
    p.job_id = job_id;
    p.complete = true;
    return p;
}

void AutonomousIngestor::cancelJob(uint64_t job_id) {
    (void)job_id;
    cancel_flag_ = true;
}

uint64_t AutonomousIngestor::autoQueueForGap(const std::string& gap_domain, float priority) {
    IngestionJob job;
    job.priority = priority;
    job.target_domains.push_back(gap_domain);

    if (gap_domain == "physics") {
        job.source_type = "physics";
        job.source_path = "data/physics_knowledge.jsonl";
    } else if (gap_domain == "ethics") {
        job.source_type = "gita";
        job.source_path = "data/gita_constitution.jsonl";
    } else if (gap_domain == "grammar") {
        job.source_type = "corpus";
        job.source_path = "data/corpus_seed.txt";
    } else {
        job.source_type = "conceptnet";
        job.source_path = "data/conceptnet_config.txt";
    }

    return queueJob(job);
}

size_t AutonomousIngestor::pendingJobsCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t count = 0;
    for (const auto& [id, p] : progress_) {
        if (!p.complete) count++;
    }
    return count;
}

} // namespace yuki::knowledge
