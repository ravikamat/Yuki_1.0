#include "MemoryDistiller.h"
#include "CognitiveMemoryFabric.h"
#include "EpisodicStore.h"
#include "SemanticGraph.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/ControlPlane.h"
#include "infrastructure/ModuleRegistry.h"
#include <iostream>
#include <chrono>
#include <cstdio>

using namespace yuki::memory;
using yuki::gw::CoreBus;
using yuki::gw::Topic;
using yuki::gw::Message;

static constexpr int IDLE_THRESHOLD_SEC = 30;

MemoryDistiller::MemoryDistiller() {
    last_activity_ = std::chrono::steady_clock::now();
}
MemoryDistiller::~MemoryDistiller() { stop(); }

void MemoryDistiller::init(
    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf,
    yuki::inference::VariationalStateEstimator* vse)
{
    cmf_  = cmf;
    vse_  = vse;
    last_activity_ = std::chrono::steady_clock::now();
}

void MemoryDistiller::start() {
    if (running_.load()) return;
    running_ = true;
    thread_  = std::thread(&MemoryDistiller::sleepLoop, this);
    std::cout << "[MemoryDistiller] Sleep-consolidation thread started.\n";
}

void MemoryDistiller::stop() {
    running_        = false;
    sleep_requested_= false;
    if (thread_.joinable()) thread_.join();
}

void MemoryDistiller::bumpActivity() {
    last_activity_ = std::chrono::steady_clock::now();
    if (consolidating_.load()) {
        sleep_requested_.store(false); // abort pending pass if user returns
    }
}

void MemoryDistiller::onSystemState(const std::string& state_json) {
    // Triggered when ControlPlane broadcasts SYSTEM_STATE = SLEEPING
    if (state_json.find("\"to\":5")        != std::string::npos ||
        state_json.find("\"state\":5")     != std::string::npos ||
        state_json.find("SLEEPING")        != std::string::npos) {
        sleep_requested_.store(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sleep loop — polls every 10s, triggers at idle >= 30s
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::sleepLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        auto idle_sec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_activity_).count();

        bool should_run = sleep_requested_.load() || (idle_sec >= IDLE_THRESHOLD_SEC);
        if (!should_run || consolidating_.load()) continue;

        sleep_requested_.store(false);
        consolidating_.store(true);

        // Transition system to SLEEPING while consolidating
        yuki::infra::ControlPlane::instance().transition(
            yuki::infra::SystemState::SLEEPING);

        runConsolidationPass();

        // Return to IDLE when done
        consolidating_.store(false);
        yuki::infra::ControlPlane::instance().transition(
            yuki::infra::SystemState::IDLE);

        yuki::infra::ModuleRegistry::instance().heartbeat("MemoryDistiller");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Consolidation pipeline
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::runConsolidationPass() {
    if (!cmf_) return;

    auto* es = cmf_->episodicStore();
    if (!es) { lshRehashing(); return; }

    // Retrieve recent episodes via retrieveByTopic("", 25) — empty = all topics
    auto episodes = es->retrieveByTopic("", 25);

    if (episodes.empty()) {
        lshRehashing();
        return;
    }

    // Compute average urgency as proxy for surprise
    float total_urgency = 0.0f;
    for (const auto& ep : episodes) total_urgency += ep.urgency_score;
    float avg_urgency = total_urgency / static_cast<float>(episodes.size());

    // 1. Pattern separation — episodic → semantic
    patternSeparation(episodes);

    // 2. Pattern completion — decay + Hebbian inference
    patternCompletion();

    // 3. Counterfactual replay on high-urgency episodes
    if (avg_urgency > 0.15f) {
        counterfactualReplay(episodes);
    }

    // 4. Precision recalibration event
    precisionRecalibration(episodes);

    // 5. LSH index maintenance
    lshRehashing();

    episodes_consolidated_ += static_cast<uint64_t>(episodes.size());

    // Broadcast completion
    Message msg;
    msg.topic         = Topic::META_COGNITIVE;
    msg.source_module = "MemoryDistiller";
    msg.salience      = 0.4f;
    msg.payload_json  = "{\"event\":\"consolidation_complete\""
                        ",\"episodes\":"    + std::to_string(episodes.size()) +
                        ",\"avg_urgency\":" + std::to_string(avg_urgency) +
                        ",\"total_consolidated\":" +
                            std::to_string(episodes_consolidated_.load()) + "}";
    CoreBus::instance().publish(msg);

    std::cout << "[MemoryDistiller] Consolidated " << episodes.size()
              << " episodes. Total=" << episodes_consolidated_.load() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern Separation: promote episodic text → semantic concepts
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::patternSeparation(const std::vector<EpisodeRecord>& episodes) {
    if (!cmf_) return;
    for (const auto& ep : episodes) {
        if (ep.text.empty()) continue;
        std::string tag = ep.topic_tag.empty() ? "general" : ep.topic_tag;
        cmf_->ingestFact(ep.text, tag, ep.confidence > 0.f ? ep.confidence : 0.5f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern Completion: decay stale weak concepts, infer new edges
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::patternCompletion() {
    if (!cmf_) return;
    // Decay concepts below 0.1 strength — cleans up rarely reinforced nodes
    size_t removed = cmf_->decayWeakConcepts(0.1f);
    if (removed > 0) {
        std::cout << "[MemoryDistiller] Pattern completion: removed "
                  << removed << " weak concept edges.\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Counterfactual Replay: high-urgency episodes replayed with altered intent
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::counterfactualReplay(const std::vector<EpisodeRecord>& episodes) {
    size_t replayed = 0;
    for (const auto& ep : episodes) {
        if (ep.urgency_score < 0.3f) continue;
        if (replayed >= 5) break; // cap at 5 per pass

        // Proxy: ingest a negated/alternative intent label to probe semantic graph
        std::string cf_text = ep.text + " [counterfactual]";
        std::string cf_tag  = "counterfactual_" + ep.topic_tag;
        if (cmf_) cmf_->ingestFact(cf_text, cf_tag, 0.3f);
        ++replayed;
    }
    counterfactuals_run_ += static_cast<uint64_t>(replayed);
    if (replayed > 0) {
        std::cout << "[MemoryDistiller] Counterfactual replay: " << replayed << " episodes.\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Precision Recalibration — broadcast event for downstream consumers
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::precisionRecalibration(const std::vector<EpisodeRecord>& /*episodes*/) {
    Message msg;
    msg.topic         = Topic::META_COGNITIVE;
    msg.source_module = "MemoryDistiller";
    msg.salience      = 0.2f;
    msg.payload_json  = "{\"event\":\"precision_recalibrated\"}";
    CoreBus::instance().publish(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// LSH Rehashing — persist vector index to disk
// ─────────────────────────────────────────────────────────────────────────────

void MemoryDistiller::lshRehashing() {
    if (!cmf_) return;
    auto* es = cmf_->episodicStore();
    if (es) {
        if (es->saveIndex()) {
            std::cout << "[MemoryDistiller] Vector index saved.\n";
        }
    }
}
