// SleepThread.cpp â€” Yuki_1.0 CMF Phase 3: Sleep Consolidation
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
#include <iostream>
#include <chrono>

namespace yuki {
namespace sleep {

SleepThread::SleepThread(Config cfg)
    : cfg_(cfg), last_activity_(std::chrono::steady_clock::now()) {}

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
        }

        std::cout << "[SleepThread] epoch done:"
                  << " visited="  << report.episodes_visited
                  << " nodes="    << report.semantic_nodes_added
                  << " edges="    << report.edges_inferred
                  << " cf="       << report.counterfactuals_run
                  << " dG="       << report.avg_free_energy_delta
                  << " promo="    << report.promotions_t1_t2
                  << "/" << report.promotions_t2_t3 << "\n";

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
        std::string concept = "cluster_" + std::to_string(bucket_idx)
                            + "_t" + std::to_string(static_cast<int64_t>(bucket_start));
        std::string episode = "ep_" + std::to_string(s.episode_id);
        semantic_->ingestProposition(concept, "contains", episode, 0.8f);
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

} // namespace sleep
} // namespace yuki
