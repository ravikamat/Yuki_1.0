// tests/test_sleep_consolidation.cpp
// Unit + integration tests for SleepThread lifecycle, idle detection,
// pattern separation, and auto-promotion.
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdio>

#include "brain/sleep/SleepThread.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/inference/VariationalStateEstimator.h"

using namespace yuki;
namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string tmpDb(const std::string& tag) {
    return (fs::temp_directory_path() / ("sleep_test_" + tag + ".db")).string();
}
static void rmDb(const std::string& tag) {
    std::string p = tmpDb(tag);
    std::remove(p.c_str());
}

// ── 1. Thread lifecycle ────────────────────────────────────────────────────────
TEST(SleepConsolidation, ThreadLifecycle) {
    sleep::SleepThread st;
    EXPECT_FALSE(st.isIdle());
    st.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(st.isIdle());   // idle_threshold default = 30 s, not yet reached
    st.stop();
    EXPECT_FALSE(st.isIdle());
}

// ── 2. Idle detection ─────────────────────────────────────────────────────────
TEST(SleepConsolidation, IdleDetectionTriggersEpoch) {
    sleep::SleepThread::Config cfg;
    cfg.idle_threshold  = std::chrono::seconds(1);
    cfg.epoch_interval  = std::chrono::seconds(1);  // bound epoch rate
    cfg.poll_ms         = std::chrono::milliseconds(100);

    sleep::SleepThread st(cfg);
    // No components wired — epoch will log "components not wired" but still run
    st.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    EXPECT_TRUE(st.isIdle());

    // signalActivity should reset idle flag
    st.signalActivity();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(st.isIdle());

    st.stop();
}

// ── 3. signalActivity prevents idle ───────────────────────────────────────────
TEST(SleepConsolidation, SignalActivityPreventsIdle) {
    sleep::SleepThread::Config cfg;
    cfg.idle_threshold = std::chrono::seconds(1);
    cfg.poll_ms        = std::chrono::milliseconds(100);

    sleep::SleepThread st(cfg);
    st.start();

    // Keep calling signalActivity faster than idle_threshold
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        st.signalActivity();
    }
    EXPECT_FALSE(st.isIdle());
    st.stop();
}

// ── 4. DreamReport with empty EpisodicStore ───────────────────────────────────
TEST(SleepConsolidation, EmptyStoreGivesZeroReport) {
    std::string db = tmpDb("empty");
    std::string idx = tmpDb("empty_idx");
    memory::EpisodicStore store(db, idx);
    ASSERT_TRUE(store.init());

    memory::HdcSemanticGraph graph(db);
    ASSERT_TRUE(graph.init());

    inference::VariationalStateEstimator vse;

    sleep::SleepThread::Config cfg;
    cfg.idle_threshold = std::chrono::seconds(1);
    cfg.epoch_interval = std::chrono::seconds(1);   // bound epoch rate
    cfg.poll_ms        = std::chrono::milliseconds(100);

    sleep::SleepThread st(cfg);
    st.setEpisodicStore(&store);
    st.setSemanticGraph(&graph);
    st.setVSE(&vse);
    st.start();

    // Wait for one epoch
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    EXPECT_TRUE(st.isIdle());

    auto report = st.lastReport();
    // Empty T1 → no consolidation, no edges, no counterfactuals
    EXPECT_EQ(report.episodes_visited, 0u);
    EXPECT_EQ(report.edges_inferred,   0u);
    EXPECT_EQ(report.counterfactuals_run, 0u);
    // epoch_start must be non-zero
    EXPECT_GT(report.epoch_start.time_since_epoch().count(), 0);

    st.stop();
    rmDb("empty");
    rmDb("empty_idx");
}

// ── 5. markConsolidated and queryRecentSnapshots ──────────────────────────────
TEST(SleepConsolidation, MarkConsolidatedRoundTrip) {
    std::string db  = tmpDb("mark");
    std::string idx = tmpDb("mark_idx");
    memory::EpisodicStore store(db, idx);
    ASSERT_TRUE(store.init());

    // Insert a record
    memory::EpisodeRecord rec;
    rec.timestamp_ms = 1000;
    rec.source       = "test";
    rec.text         = "hello";
    rec.intent_label = "greeting";
    rec.confidence   = 0.9f;
    std::vector<float> vec(24, 0.5f);
    ASSERT_TRUE(store.insert(rec, vec));

    // Should appear as unconsolidated
    auto snaps = store.queryRecentSnapshots(10, false);
    EXPECT_GE(snaps.size(), 1u);
    if (!snaps.empty()) {
        EXPECT_FALSE(snaps[0].consolidated);
        // Mark consolidated
        store.markConsolidated(snaps[0].episode_id);
        // Should now appear in consolidated query
        auto cons = store.queryRecentSnapshots(10, true);
        EXPECT_GE(cons.size(), 1u);
    }

    rmDb("mark");
    rmDb("mark_idx");
}

// ── 6. HdcSemanticGraph sleep interface ───────────────────────────────────────
TEST(SleepConsolidation, SemanticGraphGetAllConceptsEmpty) {
    std::string db = tmpDb("graph");
    memory::HdcSemanticGraph graph(db);
    ASSERT_TRUE(graph.init());
    auto concepts = graph.getAllConcepts(100);
    EXPECT_EQ(concepts.size(), 0u);   // fresh DB
    rmDb("graph");
}

TEST(SleepConsolidation, SemanticGraphMarkProcedural) {
    std::string db = tmpDb("proc");
    memory::HdcSemanticGraph graph(db);
    ASSERT_TRUE(graph.init());
    // Create a concept via ingestProposition
    graph.ingestProposition("dog", "is_a", "animal", 0.9f);
    // Mark it procedural
    bool ok = graph.markProcedural("dog");
    EXPECT_TRUE(ok);
    // Verify type changed
    auto concepts = graph.getAllConcepts(10);
    bool found = false;
    for (const auto& c : concepts) {
        if (c.name == "dog" && c.type == "procedural") { found = true; break; }
    }
    EXPECT_TRUE(found);
    rmDb("proc");
}
