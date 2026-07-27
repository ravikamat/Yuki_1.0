#include "brain/sleep/SleepThread.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "brain/memory/ProceduralStore.h"
#include "brain/self/SelfModel.h"
#include "brain/memory/PromotionMetrics.h"
#include "brain/memory/ArchiveWriter.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/learning/neural/QLearningCore.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/causal/CounterfactualSimulator.h"
#include "brain/organism/DriveSystem.h"
#include "brain/core/Logger.h"
#include <iostream>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace yuki {
namespace sleep {

SleepThread::SleepThread(Config cfg)
    : cfg_(cfg),
      running_(false),
      idle_(false),
      last_activity_(std::chrono::steady_clock::now()) {}

SleepThread::~SleepThread() { stop(); }

void SleepThread::setEpisodicStore(memory::EpisodicStore* store) { episodic_ = store; }
void SleepThread::setSemanticGraph(memory::HdcSemanticGraph* graph) { semantic_ = graph; }
void SleepThread::setVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
void SleepThread::setCMF(memory::CognitiveMemoryFabric* cmf) { cmf_ = cmf; }
void SleepThread::setDifferentialMemoryController(memory::DifferentialMemoryController* dmc) { dmc_ = dmc; }

void SleepThread::start() {
    if (running_.exchange(true)) return;
    idle_ = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        last_activity_ = std::chrono::steady_clock::now();
    }
    thread_ = std::thread(&SleepThread::run, this);
    std::cout << "[SleepThread] started (idle_threshold="
              << cfg_.idle_threshold.count() << "s)\n";
}

void SleepThread::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void SleepThread::signalActivity() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        last_activity_ = std::chrono::steady_clock::now();
    }
    if (idle_.exchange(false)) {
        cv_.notify_all();   // wake from inter-epoch sleep immediately
    }
}

DreamReport SleepThread::lastReport() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return last_report_;
}

// â”€â”€ Main loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void SleepThread::run() {
    while (running_) {
        // Block until idle OR stop
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait_for(lk, cfg_.poll_ms, [this]() -> bool {
                if (!running_) return true;
                return (std::chrono::steady_clock::now() - last_activity_)
                        >= cfg_.idle_threshold;
            });
            if (!running_) break;
            if ((std::chrono::steady_clock::now() - last_activity_) < cfg_.idle_threshold)
                continue;
        }

        idle_ = true;
        std::cout << "[SleepThread] idle â€” starting dream epoch\n";

        DreamReport report = dreamEpoch();
        {
            std::lock_guard<std::mutex> lk(mtx_);
            last_report_ = report;
            // A3 Fix: Reset idle clock so the full idle_threshold (30 s) must
            // elapse again before the next dream epoch.  Without this reset the
            // predicate (now - last_activity_ >= idle_threshold) is immediately
            // true on the next loop iteration, causing epochs to fire every
            // ~epoch_interval (5 s) instead of every ~idle_threshold (30 s).
            last_activity_ = std::chrono::steady_clock::now();
        }

        std::cout << "[SleepThread] epoch done:"
                  << " visited="  << report.episodes_visited
                  << " nodes="    << report.semantic_nodes_added
                  << " edges="    << report.edges_inferred
                  << " cf="       << report.counterfactuals_run
                  << " dG="       << report.avg_free_energy_delta
                  << " promo="    << report.promotions_t1_t2
                  << "/" << report.promotions_t2_t3 << "\n";

        // A3 Fix: Mark not-idle between epochs so signalActivity() idle_.exchange(false)
        // path and the inter-epoch wait predicate both behave correctly.
        idle_ = false;

        // Inter-epoch pause â€” wake early on signalActivity()
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait_for(lk, cfg_.epoch_interval, [this]() {
                return !running_.load() || !idle_.load();
            });
        }
    }
}

// â”€â”€ Dream Epoch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
DreamReport SleepThread::dreamEpoch() {
    DreamReport r;
    r.epoch_start = std::chrono::steady_clock::now();

    // runPromotionPolicy FIRST: must see unconsolidated T1 episodes before
    // patternSeparation() marks them all consolidated (visited=0 fix)
    auto [t1, t2]          = runPromotionPolicy();
    r.promotions_t1_t2     = t1;
    r.promotions_t2_t3     = t2;
    // Phase D: Permanent sleep metric logging
    {
        // Fetch threshold info from config for diagnostics
        float access_base = cfg_.promotion_access_min;
        float reinf_base  = cfg_.promotion_reinf_min;
        std::cerr << "[SLEEP] promotions T1->T2=" << t1
                  << " T2->T3=" << t2
                  << " access_base=" << access_base
                  << " reinf_base=" << reinf_base << "\n";
    }

    r.episodes_visited     = patternSeparation();
    r.semantic_nodes_added = r.episodes_visited;
    r.edges_inferred       = patternCompletion();
    r.counterfactuals_run  = counterfactualReplay(r.avg_free_energy_delta);
    r.precision_updates    = precisionRecalibration();
    if (dmc_) {
        try {
            dmcConsolidation(r.avg_free_energy_delta);
        } catch (const std::exception& e) {
            std::cerr << "[SleepThread] DMC consolidation exception: " << e.what() << "\n";
        }
    }
    archiveEpoch();
    r.lsh_rehashed         = lshRehashing();

    if (self_model_) {
        self_model_->consolidate();
    }

    // Section 4: [SLEEP] epoch summary log — gate: visited > 0
    {
        static size_t epoch_count = 0;
        ++epoch_count;
        std::cerr << "[SLEEP] epoch=" << epoch_count
                  << " visited=" << r.episodes_visited
                  << " cf=" << r.counterfactuals_run
                  << " dG=" << r.avg_free_energy_delta
                  << " promo=" << (r.promotions_t1_t2 + r.promotions_t2_t3) << "\n";
    }

    return r;
}

// â”€â”€ 1. Pattern Separation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Cluster T1 episodes into 5-minute time windows â†’ create T2 concept nodes
size_t SleepThread::patternSeparation() {
    if (!episodic_ || !semantic_) return 0;
    auto snaps = episodic_->queryRecentSnapshots(cfg_.max_episodes_per_epoch, false);
    if (snaps.empty()) return 0;



    constexpr double WINDOW_S = 300.0;
    double bucket_start = snaps[0].timestamp;
    int    bucket_idx   = 0;
    size_t nodes_added  = 0;

    for (const auto& s : snaps) {
        if (s.timestamp - bucket_start > WINDOW_S) {
            bucket_start = s.timestamp;
            ++bucket_idx;
        }
        std::string concept_str = "cluster_" + std::to_string(bucket_idx)
                            + "_t" + std::to_string(static_cast<int64_t>(bucket_start));
        std::string episode = "ep_" + std::to_string(s.episode_id);
        semantic_->ingestProposition(concept_str, "contains", episode, 0.8f);
        episodic_->markConsolidated(s.episode_id);
        nodes_added++;
    }
    return nodes_added;
}

// â”€â”€ 2. Pattern Completion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Compute pairwise co-occurrence for top T2 concepts â†’ ingest causal edges
size_t SleepThread::patternCompletion() {
    if (!episodic_ || !semantic_) return 0;
    auto concepts = semantic_->getAllConcepts(50);
    if (concepts.size() < 2) return 0;

    size_t edges = 0;
    const size_t max_pairs = 20;   // guard O(NÂ²) blowup
    for (size_t i = 0; i < concepts.size() && i < max_pairs; ++i) {
        for (size_t j = i + 1; j < concepts.size() && j < max_pairs; ++j) {
            float cooc = episodic_->computeCooccurrence(
                concepts[i].name, concepts[j].name, 300'000LL); // 5-min window ms
            if (cooc >= cfg_.cooccurrence_threshold) {
                std::string rel = (cooc > 0.6f)
                    ? "strongly_associated" : "co_occurs_with";
                semantic_->ingestProposition(
                    concepts[i].name, rel, concepts[j].name, cooc);
                edges++;
            }
        }
    }
    return edges;
}

// â”€â”€ 3. Counterfactual Replay â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Simulate alt policies on recent episodes using VSE's FEC; compute Î”G
size_t SleepThread::counterfactualReplay(float& avg_delta_out) {
    avg_delta_out = 0.0f;
    if (!vse_) return 0;
    if (!episodic_) return 0;

    auto snaps = episodic_->queryRecentSnapshots(
        cfg_.max_counterfactuals_per_epoch, true);
    if (snaps.empty()) return 0;

    // Copy belief state â€” simulation must NOT mutate live VSE
    auto belief_copy = vse_->currentBelief();
    auto& fec   = vse_->freeEnergyCalculator();
    auto& model = vse_->generativeModel();

    // 5 canonical policies spanning the parameter space
    std::vector<inference::Policy> policies(5);
    for (int p = 0; p < 5; ++p) {
        policies[p].parameters.resize(8, 0.5f);
        policies[p].parameters[0] = 0.2f + p * 0.15f;   // responseLength
        policies[p].parameters[4] = p * 0.25f;           // proactivity
        policies[p].description   = "cf_" + std::to_string(p);
    }

    float total_delta = 0.0f;
    size_t count = 0;
    for (const auto& s : snaps) {
        int actual_idx = static_cast<int>(
            (s.vector_slot >= 0 ? s.vector_slot : 0) % 5);
        float g_actual = fec.computeG(policies[actual_idx], belief_copy, model);
        float g_best   = g_actual;
        for (int p = 0; p < 5; ++p) {
            if (p == actual_idx) continue;
            float g = fec.computeG(policies[p], belief_copy, model);
            if (g < g_best) g_best = g;
        }
        total_delta += (g_best - g_actual);   // â‰¤ 0 means improvement possible
        count++;
    }
    avg_delta_out = count > 0 ? total_delta / count : 0.0f;
    return count;
}

// â”€â”€ 4. Precision Recalibration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Report consolidation signal as a proxy outcome for PrecisionEngine
size_t SleepThread::precisionRecalibration() {
    if (!vse_) return 0;
    auto& pe = vse_->precisionEngine();

    // Use consolidation ratio as rough accuracy proxy
    auto consolidated   = episodic_->queryRecentSnapshots(50, true);
    auto unconsolidated = episodic_->queryRecentSnapshots(50, false);
    float total = static_cast<float>(consolidated.size() + unconsolidated.size());
    bool good = total > 0 && (consolidated.size() / total) > 0.5f;

    pe.updateHistoricalAccuracy("sleep_t1_consolidation", good);
    pe.updateHistoricalAccuracy("sleep_semantic_coverage", good);
    return 2;
}

// â”€â”€ 5. LSH Rehashing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Rebuild LSH tables if collision rate is degraded
bool SleepThread::lshRehashing() {
    if (!episodic_) return false;
    if (episodic_->getLshCollisionRate() > 0.5f) {
        episodic_->rebuildLshTables();
        return true;
    }
    return false;
}

// â”€â”€ 6. Auto-Promotion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// T1 â†’ T2: reinforce frequently-visited episodes; T2 â†’ T3: mark procedural
std::pair<size_t,size_t> SleepThread::promoteEpisodes(float access_min, float reinf_min) {
    if (!episodic_ || !semantic_) return {0, 0};
    size_t t1_t2 = 0, t2_t3 = 0;

    // T1 â†’ T2: episodes with high access_count
    auto snaps = episodic_->queryRecentSnapshots(100, false);
    for (const auto& s : snaps) {
        if (s.access_count >= static_cast<int>(access_min)) {
            semantic_->reinforce("ep_" + std::to_string(s.episode_id));
            episodic_->markConsolidated(s.episode_id);
            t1_t2++;
        }
    }

    // T2 â†’ T3: heavily reinforced concepts
    auto concepts = semantic_->getAllConcepts(200);
    for (const auto& c : concepts) {
        if (c.access_count >= static_cast<int>(reinf_min * 2.0f)
            && c.type != "procedural") {
            semantic_->markProcedural(c.name);
            t2_t3++;
        }
    }

    return {t1_t2, t2_t3};
}

// â”€â”€ 7. DMC-guided Promotion Policy â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Phase B: Adaptive promotion policy â€” PromotionMetrics decay * adaptive thresholds.
std::pair<size_t,size_t> SleepThread::runPromotionPolicy() {
    if (!episodic_ || !semantic_ || !dmc_) return {0, 0};

    // â”€â”€ Adaptive promotion thresholds â”€â”€
    // Derived from DMC stability, not hardcoded constants.
    // When DMC outcomes are stable (low variance), the model has learned
    // reliable patterns â†’ promote more aggressively.
    // When unstable (high variance), model is still learning â†’ promote
    // conservatively to avoid polluting T2/T3 with noise.

    double outcome_variance = dmcOutcomeVariance();
    double mean_surprise    = runningMeanSurprise();
    double stability_threshold = (mean_surprise > 1e-6)
        ? mean_surprise * 0.1
        : runningSurpriseStd() * 0.5;

    // Adaptive access threshold: base 5, scaled by stability
    // Stable â†’ lower threshold (promote more)
    // Unstable â†’ higher threshold (promote less)
    float adaptive_access_min = cfg_.promotion_access_min;
    float adaptive_reinf_min  = cfg_.promotion_reinf_min;
    if (outcome_variance < stability_threshold) {
        // Stable: be aggressive
        adaptive_access_min *= 0.7f;
        adaptive_reinf_min  *= 0.7f;
    } else {
        // Unstable: be conservative
        adaptive_access_min *= 1.5f;
        adaptive_reinf_min  *= 1.5f;
    }

    // Clamp to sane bounds
    adaptive_access_min = std::max(1.0f, std::min(adaptive_access_min, 20.0f));
    adaptive_reinf_min  = std::max(1.0f, std::min(adaptive_reinf_min,  15.0f));

    // â”€â”€ Execute promotion with adaptive thresholds â”€â”€
    return promoteEpisodes(adaptive_access_min, adaptive_reinf_min);
}

void SleepThread::dmcConsolidation(float avg_free_energy_delta) {
    if (!dmc_) return;
    if (!vse_) return;

    // â”€â”€ Synthesize sleep-derived training sample â”€â”€
    // The DMC ring buffer is empty during idle epochs (no user turns).
    // We inject a counterfactual-replay outcome so the MLP learns from sleep.

    // 1. Extract current VSE posterior as context features
    std::array<float, 24> context{};
    const auto& belief = vse_->currentBelief();
    for (int i = 0; i < 8; ++i) context[i] = belief.q_intent[i];

    // 2. Evaluate: fill one ring slot with sleep context
    auto [decision, token] = dmc_->evaluate(context, context);

    // 3. Record outcome: success if Free Energy decreased (delta < 0)
    bool success = (avg_free_energy_delta < 0.0f);
    float precision = std::min(1.0f, std::abs(avg_free_energy_delta) / 10.0f);
    dmc_->recordOutcome(token, success, precision);

    // 4. Now consolidate: the ring has 1 recorded outcome
    bool learned = dmc_->consolidate();
    if (learned) {
        std::cout << "[SleepThread] DMC weights consolidated and persisted.\n";
    }
}

// Minimal serialization: concept name as UTF-8 bytes
std::vector<uint8_t> SleepThread::compileConceptToBlob(const std::string& name) {
    return std::vector<uint8_t>(name.begin(), name.end());
}

void SleepThread::recordDmcOutcome(double free_energy_delta, double surprise) {
    DmcOutcome o;
    o.free_energy_delta = free_energy_delta;
    o.surprise = surprise;
    o.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    dmc_outcomes_.push_back(o);
    if (dmc_outcomes_.size() > 100) dmc_outcomes_.erase(dmc_outcomes_.begin());
    ++dmc_outcome_count_;
    updateRunningSurprise(surprise);
}

void SleepThread::updateRunningSurprise(double surprise) {
    ++running_surprise_n_;
    double delta = surprise - running_mean_surprise_;
    running_mean_surprise_ += delta / running_surprise_n_;
    double delta2 = surprise - running_mean_surprise_;
    running_m2_surprise_ += delta * delta2;
}

double SleepThread::dmcOutcomeVariance() const {
    if (dmc_outcomes_.size() < 2) return std::numeric_limits<double>::max();
    return running_m2_surprise_ / (running_surprise_n_ - 1);
}

double SleepThread::runningMeanSurprise() const {
    return running_mean_surprise_;
}

double SleepThread::runningSurpriseStd() const {
    if (running_surprise_n_ < 2) return std::numeric_limits<double>::max();
    return std::sqrt(running_m2_surprise_ / (running_surprise_n_ - 1));
}

// Phase C: Archive consolidated T1 episodes to a .yuk archive file (one per epoch).
void SleepThread::archiveEpoch() {
    if (!cmf_) return;
    auto* archive  = cmf_->archiveWriter();
    auto* episodic = cmf_->episodicStore();
    if (!archive || !episodic) return;

    // â”€â”€ Record DMC outcome from this epoch â”€â”€
    // avg_free_energy_delta is already computed by existing sleep logic
    double epoch_surprise = std::abs(last_report_.avg_free_energy_delta);
    recordDmcOutcome(last_report_.avg_free_energy_delta, epoch_surprise);

    // â”€â”€ T3â†’T4 Promotion Gate â”€â”€
    // Derived from DMC stability: variance low relative to mean surprise
    double outcome_variance = dmcOutcomeVariance();
    double mean_surprise    = runningMeanSurprise();
    double surprise_std     = runningSurpriseStd();
    
    // Guard: need enough samples for statistical significance
    if (dmc_outcome_count_ < t4_policy_.min_stable_outcomes) {
        std::cerr << "[SleepThread] T3â†’T4: ACCUMULATING (outcomes=" 
                  << dmc_outcome_count_ << "/" << t4_policy_.min_stable_outcomes 
                  << ")\n";
        return;
    }

    // Stability = variance less than 10% of mean surprise
    // (If mean is near zero, use absolute threshold based on std)
    double stability_threshold = (mean_surprise > 1e-6) 
        ? mean_surprise * 0.1 
        : surprise_std * 0.5;
    
    bool should_promote = (outcome_variance < stability_threshold);
    
    if (!should_promote) {
        std::cerr << "[SleepThread] T3â†’T4: UNSTABLE (var=" << outcome_variance
                  << ", threshold=" << stability_threshold << ")\n";
        return;
    }

    std::cerr << "[SleepThread] T3â†’T4: STABLE â€” archiving epoch\n";

    // â”€â”€ Gather rich cognitive state â”€â”€
    auto snaps = episodic->queryRecentSnapshots(
        cfg_.max_episodes_per_epoch, /*consolidated_only=*/true);
    if (snaps.empty()) return;

    // Extended schema: store full cognitive state, not just IDs
    static const std::vector<memory::ColumnarArchiveFormat::ColumnSchema> kSchema = {
        { "episode_id",   memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64    },
        { "timestamp",    memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE   },
        { "slot",         memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64    },
        { "free_energy",  memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE   },
        { "surprise",     memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE   }
    };

    uint64_t ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    std::string archive_name = "epoch_" + std::to_string(ts);

    if (!archive->beginArchive(archive_name, kSchema)) return;

    std::vector<int64_t> ids;
    std::vector<double>  timestamps;
    std::vector<int64_t> slots;
    ids.reserve(snaps.size());
    timestamps.reserve(snaps.size());
    slots.reserve(snaps.size());
    for (const auto& s : snaps) {
        ids.push_back(s.episode_id);
        timestamps.push_back(s.timestamp);
        slots.push_back(s.vector_slot);
    }

    // ═══ Gap 6 Fix: Write all 5 schema columns (id, ts, slot, free_energy, surprise) ═══
    // Previously only 3 of 5 columns were written, making readArchiveByMerkle and queryBySurprise
    // return zero/empty data since free_energy and surprise columns were missing.
    std::vector<double> free_energies(snaps.size(), 0.0);
    std::vector<double> surprises(snaps.size(), 0.0);
    // Populate with the epoch's aggregate metrics as per-episode estimates
    double fe_per_ep = std::abs(last_report_.avg_free_energy_delta) / std::max(size_t(1), snaps.size());
    double surprise_per_ep = epoch_surprise / std::max(size_t(1), snaps.size());
    for (size_t i = 0; i < snaps.size(); ++i) {
        free_energies[i] = fe_per_ep;
        surprises[i]     = surprise_per_ep;
    }

    std::vector<memory::ColumnarArchiveFormat::ColumnData> rg;
    rg.push_back({ "episode_id",   ids          });
    rg.push_back({ "timestamp",    timestamps   });
    rg.push_back({ "slot",         slots        });
    rg.push_back({ "free_energy",  free_energies });
    rg.push_back({ "surprise",     surprises    });
    archive->writeRowGroup(rg);

    std::string merkle_root;
    archive->finalizeArchive(merkle_root);
}

// ══════════════════════════════════════════════════════════════════════════════
// DreamEngine Implementation
// ══════════════════════════════════════════════════════════════════════════════

class DreamEngine::Impl {
public:
    yuki::learning::generative::VariationalAutoencoder* vae_ = nullptr;
    yuki::memory::MemoryFabric* fabric_ = nullptr;
    yuki::causal::CounterfactualSimulator* sim_ = nullptr;
    std::vector<yuki::organism::DriveGoal> goals_;
    DreamConfig config_;
    size_t totalDreams_ = 0;
    std::mt19937_64 rng_{2026};

    Impl() = default;
};

DreamEngine::DreamEngine() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "DreamEngine initialized");
}

DreamEngine::~DreamEngine() = default;

DreamEngine::DreamEngine(DreamEngine&&) noexcept = default;
DreamEngine& DreamEngine::operator=(DreamEngine&&) noexcept = default;

void DreamEngine::setVAE(yuki::learning::generative::VariationalAutoencoder* vae) {
    pImpl->vae_ = vae;
}

void DreamEngine::setMemoryFabric(yuki::memory::MemoryFabric* fabric) {
    pImpl->fabric_ = fabric;
}

void DreamEngine::setCounterfactualSimulator(yuki::causal::CounterfactualSimulator* sim) {
    pImpl->sim_ = sim;
}

void DreamEngine::setDriveGoals(const std::vector<yuki::organism::DriveGoal>& goals) {
    pImpl->goals_ = goals;
}

void DreamEngine::setConfig(const DreamConfig& config) {
    pImpl->config_ = config;
}

std::vector<double> DreamEngine::sampleDirichlet(size_t k, double alpha) {
    if (k == 0) return {};
    std::gamma_distribution<double> dist(alpha, 1.0);
    std::vector<double> samples(k);
    double sum = 0.0;
    for (size_t i = 0; i < k; ++i) {
        samples[i] = dist(pImpl->rng_);
        sum += samples[i];
    }
    if (sum > 1e-12) {
        for (size_t i = 0; i < k; ++i) samples[i] /= sum;
    }
    return samples;
}

std::vector<size_t> DreamEngine::sampleMemoryIndices(size_t count, size_t total) {
    std::vector<size_t> indices;
    if (total == 0 || count == 0) return indices;
    count = std::min(count, total);

    std::vector<size_t> pool(total);
    for (size_t i = 0; i < total; ++i) pool[i] = i;

    std::shuffle(pool.begin(), pool.end(), pImpl->rng_);
    indices.assign(pool.begin(), pool.begin() + count);
    return indices;
}

DreamEpisode DreamEngine::generateBlendDream() {
    DreamEpisode ep;
    ep.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (!pImpl->vae_) {
        size_t dim = 128;
        ep.features.resize(dim, 0.5);
        pImpl->totalDreams_++;
        return ep;
    }

    size_t dim = pImpl->vae_->getConfig().inputDim;
    size_t zDim = pImpl->vae_->getConfig().latentDim;

    size_t numBlend = pImpl->config_.minBlendMemories;
    auto weights = sampleDirichlet(numBlend, 1.0);

    std::vector<double> zBlended(zDim, 0.0);
    std::uniform_real_distribution<double> dist(-pImpl->config_.latentPerturbationScale, pImpl->config_.latentPerturbationScale);

    for (size_t i = 0; i < zDim; ++i) {
        zBlended[i] = dist(pImpl->rng_);
    }

    ep.features = pImpl->vae_->decode(zBlended);
    if (ep.features.empty()) ep.features.resize(dim, 0.0);

    ep.noveltyScore = pImpl->vae_->anomalyScore(ep.features);
    ep.isCounterfactual = false;
    pImpl->totalDreams_++;
    return ep;
}

DreamEpisode DreamEngine::generateCounterfactualDream(uint64_t memoryId, const std::vector<double>& goalDirection) {
    DreamEpisode ep;
    ep.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ep.sourceMemoryIds.push_back(memoryId);
    ep.isCounterfactual = true;

    if (pImpl->vae_) {
        size_t dim = pImpl->vae_->getConfig().inputDim;
        std::vector<double> base = pImpl->vae_->samplePrior();
        if (base.size() == goalDirection.size()) {
            for (size_t i = 0; i < base.size(); ++i) {
                base[i] = 0.7 * base[i] + 0.3 * goalDirection[i];
            }
        }
        ep.features = base;
        ep.noveltyScore = pImpl->vae_->anomalyScore(base);
    } else {
        ep.features = goalDirection;
        ep.noveltyScore = 0.5;
    }

    pImpl->totalDreams_++;
    return ep;
}

std::vector<DreamEpisode> DreamEngine::generateDreamCycle() {
    std::vector<DreamEpisode> cycle;
    cycle.reserve(pImpl->config_.dreamsPerCycle);

    for (size_t i = 0; i < pImpl->config_.dreamsPerCycle; ++i) {
        if (pImpl->config_.enableCounterfactualDreams && (i % 2 == 1)) {
            std::vector<double> goalDir = {0.1, 0.2, 0.3};
            cycle.push_back(generateCounterfactualDream(i + 1, goalDir));
        } else {
            cycle.push_back(generateBlendDream());
        }
    }
    return cycle;
}

size_t DreamEngine::trainVAEDreamBatch(size_t batchSize) {
    if (!pImpl->vae_ || batchSize == 0) return 0;

    size_t dim = pImpl->vae_->getConfig().inputDim;
    std::vector<std::vector<double>> batch;
    batch.reserve(batchSize);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (size_t b = 0; b < batchSize; ++b) {
        std::vector<double> sample(dim);
        for (size_t i = 0; i < dim; ++i) sample[i] = dist(pImpl->rng_);
        batch.push_back(sample);
    }

    pImpl->vae_->trainBatch(batch);
    return batchSize;
}

size_t DreamEngine::getTotalDreamsGenerated() const {
    return pImpl->totalDreams_;
}

std::vector<uint8_t> DreamEngine::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x4452454D;
    uint64_t count = pImpl->totalDreams_;

    buf.resize(20);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &count, 8);
    std::memcpy(buf.data() + 12, &pImpl->config_.dreamsPerCycle, 8);

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool DreamEngine::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 28) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x4452454D) return false;

    uint64_t count = 0, perCycle = 0;
    std::memcpy(&count, data.data() + 4, 8);
    std::memcpy(&perCycle, data.data() + 12, 8);

    pImpl->totalDreams_ = count;
    pImpl->config_.dreamsPerCycle = perCycle;

    return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// CounterfactualReplayEngine Implementation
// ══════════════════════════════════════════════════════════════════════════════

CounterfactualReplayEngine::CounterfactualReplayEngine(
    memory::EpisodicStore&                episodic,
    inference::VariationalStateEstimator& vse,
    CounterfactualReplayConfig            config)
    : episodic_(episodic)
    , vse_(vse)
    , config_(config)
    , rng_(std::random_device{}())
{
    ring_buffer_.reserve(config_.ring_buffer_capacity);
}

size_t CounterfactualReplayEngine::replay(size_t max_episodes, float& avg_delta_out) {
    avg_delta_out = 0.0f;

    auto snaps = episodic_.queryRecentSnapshots(max_episodes, true);
    if (snaps.empty()) return 0;

    auto belief_copy = vse_.currentBelief();
    auto& fec        = vse_.freeEnergyCalculator();
    auto& model      = vse_.generativeModel();

    auto policy_bank = build_policy_bank();
    const size_t n_base = policy_bank.size();

    float total_delta = 0.0f;
    size_t replayed   = 0;

    for (const auto& snap : snaps) {
        size_t base_idx = static_cast<size_t>(
            std::max<int64_t>(snap.vector_slot, 0)) % n_base;
        const auto& base_policy = policy_bank[base_idx];

        float g_base = fec.computeG(base_policy, belief_copy, model);

        float   g_best    = g_base;
        int     best_cf   = -1;
        inference::Policy best_cf_policy = base_policy;

        for (size_t cf = 0; cf < config_.n_counterfactuals; ++cf) {
            inference::Policy cf_policy = perturb(base_policy, config_.perturbation_sigma);
            float g_cf = fec.computeG(cf_policy, belief_copy, model);
            if (g_cf < g_best) {
                g_best       = g_cf;
                best_cf      = static_cast<int>(cf);
                best_cf_policy = cf_policy;
            }
        }

        float delta_g = g_best - g_base;

        ReplayExperience exp;
        exp.episode_id      = snap.episode_id;
        exp.original_policy = static_cast<int>(base_idx);
        exp.best_cf_policy  = best_cf;
        exp.g_original      = g_base;
        exp.g_best_cf       = g_best;
        exp.delta_g         = delta_g;
        push_experience(exp);

        if (delta_g < -config_.improvement_threshold) {
            update_generative_model(best_cf_policy, model, config_.model_update_lr);
            ++total_improvements_;
        }

        total_delta += delta_g;
        update_running_avg(delta_g);
        ++replayed;
    }

    total_replayed_ += replayed;
    avg_delta_out = replayed > 0 ? total_delta / static_cast<float>(replayed) : 0.0f;
    return replayed;
}

std::vector<inference::Policy> CounterfactualReplayEngine::build_policy_bank() const {
    std::vector<inference::Policy> bank(config_.n_baseline_policies);
    for (size_t p = 0; p < config_.n_baseline_policies; ++p) {
        bank[p].parameters.assign(8, 0.5f);
        float frac = (config_.n_baseline_policies > 1)
            ? static_cast<float>(p) / static_cast<float>(config_.n_baseline_policies - 1)
            : 0.5f;
        bank[p].parameters[0] = 0.2f + frac * 0.6f;
        bank[p].parameters[4] = frac;
        bank[p].description   = "base_" + std::to_string(p);
    }
    return bank;
}

inference::Policy CounterfactualReplayEngine::perturb(
    const inference::Policy& base, float sigma)
{
    inference::Policy p;
    p.parameters.resize(base.parameters.size());
    std::normal_distribution<float> noise(0.0f, sigma);
    for (size_t i = 0; i < base.parameters.size(); ++i) {
        float v = base.parameters[i] + noise(rng_);
        p.parameters[i] = std::max(0.0f, std::min(1.0f, v));
    }
    p.description = "cf_perturbed";
    return p;
}

void CounterfactualReplayEngine::push_experience(const ReplayExperience& exp) {
    if (ring_buffer_.size() < config_.ring_buffer_capacity) {
        ring_buffer_.push_back(exp);
    } else {
        ring_buffer_[ring_head_ % config_.ring_buffer_capacity] = exp;
        ++ring_head_;
    }
}

void CounterfactualReplayEngine::update_generative_model(
    const inference::Policy& better_policy,
    inference::GenerativeModel& model,
    float lr)
{
    float rl = better_policy.responseLength();
    size_t intent_idx = static_cast<size_t>(std::round(rl * 7.0f));
    auto intent = static_cast<yuki::IntentClass>(intent_idx);

    std::vector<float> implied_features(12, 0.0f);
    if (better_policy.parameters.size() >= 8) {
        for (size_t i = 0; i < std::min<size_t>(8, implied_features.size()); ++i) {
            implied_features[i] = better_policy.parameters[i];
        }
    }

    model.updateMapping(
        intent,
        yuki::perception::Modality::TEXT,
        implied_features,
        lr);
}

void CounterfactualReplayEngine::update_running_avg(float delta_g) {
    constexpr float DECAY = 0.98f;
    running_avg_delta_g_ = DECAY * running_avg_delta_g_ + (1.0f - DECAY) * delta_g;
}

size_t CounterfactualReplayEngine::generateCounterfactuals(size_t count, uint64_t max_episode_age_ms) {
    if (!causal_graph_) return 0;

    size_t generated = 0;
    float avg_delta = 0.0f;
    size_t replayed = replay(count, avg_delta);

    if (rl_core_) {
        for (const auto& exp : ring_buffer_) {
            if (exp.delta_g < -config_.improvement_threshold) {
                yuki::learning::neural::Matrix state_mat(1, 2);
                yuki::learning::neural::Matrix next_mat(1, 2);
                state_mat(0, 0) = static_cast<float>(exp.episode_id);
                state_mat(0, 1) = exp.g_original;
                next_mat(0, 0) = static_cast<float>(exp.episode_id);
                next_mat(0, 1) = exp.g_best_cf;

                yuki::learning::neural::Experience cf_exp{state_mat, static_cast<size_t>(std::max(0, exp.best_cf_policy)), -exp.delta_g, next_mat, true};
                rl_core_->store_experience(cf_exp);
                generated++;
            }
        }
    } else {
        generated = replayed;
    }

    return generated;
}

} // namespace sleep
} // namespace yuki
