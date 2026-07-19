// tests/test_t4_archive_cognitive.cpp
// T4-specific tests for:
//   1. ArchiveWriter cognitive payload roundtrip (begin/writeRowGroup/finalize with cognitive schema)
//   2. MerkleDAG::persistNode + loadNode + traceToGenesis
//   3. SleepThread::archiveEpoch promotion gate (observable via dmc_outcome_count guard)
//   4. ArchiveWriter::queryBySurprise API contract

#include <gtest/gtest.h>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "brain/memory/ArchiveWriter.h"
#include "brain/memory/ColumnarArchiveFormat.h"
#include "brain/memory/MerkleDAG.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/sleep/SleepThread.h"

namespace fs = std::filesystem;

// ─── Shared Helpers ───────────────────────────────────────────────────────────

static const std::string kT4TestDir = "data/test_t4";

static void ensureDir(const std::string& dir) {
    fs::create_directories(dir);
}

static void cleanDir(const std::string& dir) {
    if (fs::exists(dir)) {
        for (auto& e : fs::directory_iterator(dir))
            fs::remove(e.path());
    }
}

// Standard T4 cognitive schema used by archiveEpoch()
static std::vector<yuki::memory::ColumnarArchiveFormat::ColumnSchema> cognitiveSchema() {
    return {
        { "episode_id",  yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64  },
        { "timestamp",   yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE },
        { "slot",        yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64  },
        { "free_energy", yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE },
        { "surprise",    yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE }
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST GROUP 1: ArchiveWriter — Cognitive Payload Roundtrip
// Verifies the full T4 write path with the 5-column cognitive schema
// (episode_id, timestamp, slot, free_energy, surprise) that archiveEpoch() uses.
// ═══════════════════════════════════════════════════════════════════════════════

// 1a. Full T4 cognitive schema write → finalize → file exists with valid Merkle root
TEST(T4CognitivePayload, WriteCognitiveSchemaRoundtrip) {
    const std::string subdir = kT4TestDir + "/cognitive_roundtrip";
    cleanDir(subdir);
    ensureDir(subdir);

    yuki::memory::ArchiveWriter writer(subdir);
    ASSERT_TRUE(writer.beginArchive("cognitive_epoch_001", cognitiveSchema()))
        << "beginArchive must succeed for cognitive schema";

    // Simulate 5 episodes with all 5 columns populated
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "episode_id",  std::vector<int64_t>{1001, 1002, 1003, 1004, 1005}          },
        { "timestamp",   std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0}               },
        { "slot",        std::vector<int64_t>{0, 1, 2, 3, 4}                        },
        { "free_energy", std::vector<double>{-0.12, -0.08, -0.15, -0.05, -0.20}     },
        { "surprise",    std::vector<double>{0.30, 0.22, 0.41, 0.10, 0.55}          }
    };

    ASSERT_TRUE(writer.writeRowGroup(rg))
        << "writeRowGroup must succeed for cognitive payload";

    std::string merkle_root;
    ASSERT_TRUE(writer.finalizeArchive(merkle_root))
        << "finalizeArchive must succeed";

    // Merkle root must be a valid 64-char lowercase hex string
    ASSERT_EQ(merkle_root.size(), 64u)
        << "Merkle root must be 64 hex chars (SHA-256)";
    for (char c : merkle_root) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "Merkle root must contain only lowercase hex chars, got: " << c;
    }

    // File must exist and pass integrity verification
    std::string filepath = subdir + "/cognitive_epoch_001.yuk";
    EXPECT_TRUE(fs::exists(filepath))   << "Archive file must be created";
    EXPECT_GT(fs::file_size(filepath), 0u) << "Archive file must be non-empty";
    EXPECT_TRUE(yuki::memory::ArchiveWriter::verifyArchive(filepath))
        << "Cognitive payload archive must pass integrity verification";
}

// 1b. Chained epochs: parent Merkle root is threaded across two write cycles
TEST(T4CognitivePayload, ChainedEpochMerkleRoots) {
    const std::string subdir = kT4TestDir + "/cognitive_chain";
    cleanDir(subdir);
    ensureDir(subdir);

    // Epoch 1 — genesis (no parent)
    yuki::memory::ArchiveWriter w1(subdir);
    ASSERT_TRUE(w1.beginArchive("epoch_chain_001", cognitiveSchema()));
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg1 = {
        { "episode_id",  std::vector<int64_t>{10}   },
        { "timestamp",   std::vector<double>{100.0} },
        { "slot",        std::vector<int64_t>{0}    },
        { "free_energy", std::vector<double>{-0.1}  },
        { "surprise",    std::vector<double>{0.3}   }
    };
    ASSERT_TRUE(w1.writeRowGroup(rg1));
    std::string root1;
    ASSERT_TRUE(w1.finalizeArchive(root1));
    EXPECT_EQ(root1.size(), 64u);

    // Epoch 2 — parent = root1
    yuki::memory::ArchiveWriter w2(subdir);
    w2.setParentMerkleRoot(root1);
    ASSERT_TRUE(w2.beginArchive("epoch_chain_002", cognitiveSchema()));
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg2 = {
        { "episode_id",  std::vector<int64_t>{20}   },
        { "timestamp",   std::vector<double>{200.0} },
        { "slot",        std::vector<int64_t>{1}    },
        { "free_energy", std::vector<double>{-0.05} },
        { "surprise",    std::vector<double>{0.15}  }
    };
    ASSERT_TRUE(w2.writeRowGroup(rg2));
    std::string root2;
    ASSERT_TRUE(w2.finalizeArchive(root2));
    EXPECT_EQ(root2.size(), 64u);

    // Two chained epochs MUST produce distinct roots (different content + parent)
    EXPECT_NE(root1, root2)
        << "Chained epochs must produce distinct Merkle roots";

    // getCurrentMerkleRoot must track the latest finalized root
    EXPECT_EQ(w2.getCurrentMerkleRoot(), root2);
}

// 1c. Empty row group still produces a valid archive (edge case: 0 consolidated episodes)
TEST(T4CognitivePayload, EmptyRowGroupProducesValidArchive) {
    const std::string subdir = kT4TestDir + "/cognitive_empty";
    cleanDir(subdir);
    ensureDir(subdir);

    yuki::memory::ArchiveWriter writer(subdir);
    ASSERT_TRUE(writer.beginArchive("cognitive_empty_epoch", cognitiveSchema()));
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> empty_rg = {
        { "episode_id",  std::vector<int64_t>{} },
        { "timestamp",   std::vector<double>{}  },
        { "slot",        std::vector<int64_t>{} },
        { "free_energy", std::vector<double>{}  },
        { "surprise",    std::vector<double>{}  }
    };
    ASSERT_TRUE(writer.writeRowGroup(empty_rg));
    std::string root;
    ASSERT_TRUE(writer.finalizeArchive(root));
    EXPECT_EQ(root.size(), 64u);
    std::string filepath = subdir + "/cognitive_empty_epoch.yuk";
    EXPECT_TRUE(yuki::memory::ArchiveWriter::verifyArchive(filepath));
}


// ═══════════════════════════════════════════════════════════════════════════════
// TEST GROUP 2: MerkleDAG::persistNode + loadNode + traceToGenesis
// ═══════════════════════════════════════════════════════════════════════════════

// 2a. persistNode writes a .node file; loadNode reads back identical fields
TEST(MerkleDAGPersist, PersistAndLoadRoundtrip) {
    const std::string subdir = kT4TestDir + "/merkle_persist";
    cleanDir(subdir);
    ensureDir(subdir);

    MerkleDAG dag;
    std::string content_hash = dag.hashString("payload_data_abc");
    std::string parent_hash  = std::string(64, '0');  // genesis
    std::string merkle_hash  = dag.createNode(content_hash, parent_hash);
    uint64_t    ts           = 1717000000ULL;

    ASSERT_TRUE(dag.persistNode(subdir, merkle_hash, content_hash, parent_hash, ts))
        << "persistNode must return true on success";

    // .node file must exist on disk
    std::string node_path = subdir + "/" + merkle_hash + ".node";
    EXPECT_TRUE(fs::exists(node_path)) << ".node file must be created on disk";

    // loadNode must recover all fields exactly
    std::string loaded_content, loaded_parent;
    uint64_t    loaded_ts = 0;
    ASSERT_TRUE(dag.loadNode(subdir, merkle_hash, loaded_content, loaded_parent, loaded_ts))
        << "loadNode must succeed for a persisted node";

    EXPECT_EQ(loaded_content, content_hash) << "content_hash must survive persist/load";
    EXPECT_EQ(loaded_parent,  parent_hash)  << "parent_hash must survive persist/load";
    EXPECT_EQ(loaded_ts,      ts)           << "timestamp must survive persist/load";

    // Re-verify the loaded data passes verifyNode
    EXPECT_TRUE(dag.verifyNode(loaded_content, loaded_parent, merkle_hash))
        << "Loaded node fields must still pass verifyNode()";
}

// 2b. loadNode returns false for a non-existent merkle hash
TEST(MerkleDAGPersist, LoadNodeMissingReturnsFalse) {
    const std::string subdir = kT4TestDir + "/merkle_missing";
    cleanDir(subdir);
    ensureDir(subdir);

    MerkleDAG dag;
    std::string fake_hash(64, 'f');
    std::string c, p;
    uint64_t ts = 0;
    EXPECT_FALSE(dag.loadNode(subdir, fake_hash, c, p, ts))
        << "loadNode on non-existent hash must return false";
}

// 2c. traceToGenesis on a 3-node chain returns all nodes in head→genesis order
TEST(MerkleDAGPersist, TraceToGenesisThreeNodes) {
    const std::string subdir = kT4TestDir + "/merkle_trace";
    cleanDir(subdir);
    ensureDir(subdir);

    MerkleDAG dag;
    std::string genesis = std::string(64, '0');

    // Build a 3-node chain: genesis ← m1 ← m2 ← m3 (head)
    std::string c1 = dag.hashString("epoch_content_1");
    std::string m1 = dag.createNode(c1, genesis);
    ASSERT_TRUE(dag.persistNode(subdir, m1, c1, genesis, 1000ULL));

    std::string c2 = dag.hashString("epoch_content_2");
    std::string m2 = dag.createNode(c2, m1);
    ASSERT_TRUE(dag.persistNode(subdir, m2, c2, m1, 2000ULL));

    std::string c3 = dag.hashString("epoch_content_3");
    std::string m3 = dag.createNode(c3, m2);
    ASSERT_TRUE(dag.persistNode(subdir, m3, c3, m2, 3000ULL));

    // Trace from head (m3) back to genesis
    std::vector<std::string> chain;
    ASSERT_TRUE(dag.traceToGenesis(subdir, m3, chain))
        << "traceToGenesis must succeed on a valid 3-node chain";

    // Chain must be [m3, m2, m1] — genesis zero-hash is NOT included
    ASSERT_EQ(chain.size(), 3u) << "Chain must contain exactly 3 nodes";
    EXPECT_EQ(chain[0], m3) << "chain[0] must be the head node";
    EXPECT_EQ(chain[1], m2) << "chain[1] must be the middle node";
    EXPECT_EQ(chain[2], m1) << "chain[2] must be the genesis-linked node";
}

// 2d. traceToGenesis fails on a broken chain (missing intermediate node)
TEST(MerkleDAGPersist, TraceToGenesisBrokenChainFails) {
    const std::string subdir = kT4TestDir + "/merkle_broken";
    cleanDir(subdir);
    ensureDir(subdir);

    MerkleDAG dag;
    std::string genesis = std::string(64, '0');

    // Only persist head; skip m1 (middle) → chain is broken
    std::string c1 = dag.hashString("base_not_persisted");
    std::string m1 = dag.createNode(c1, genesis);
    // m1 is deliberately NOT persisted

    std::string c2 = dag.hashString("head_only");
    std::string m2 = dag.createNode(c2, m1);
    ASSERT_TRUE(dag.persistNode(subdir, m2, c2, m1, 9999ULL));

    std::vector<std::string> chain;
    EXPECT_FALSE(dag.traceToGenesis(subdir, m2, chain))
        << "traceToGenesis must return false when an intermediate node is missing";
}

// 2e. traceToGenesis on a single node returns a 1-element chain
TEST(MerkleDAGPersist, TraceToGenesisSingleNode) {
    const std::string subdir = kT4TestDir + "/merkle_single";
    cleanDir(subdir);
    ensureDir(subdir);

    MerkleDAG dag;
    std::string genesis = std::string(64, '0');
    std::string c1 = dag.hashString("sole_epoch");
    std::string m1 = dag.createNode(c1, genesis);
    ASSERT_TRUE(dag.persistNode(subdir, m1, c1, genesis, 5000ULL));

    std::vector<std::string> chain;
    ASSERT_TRUE(dag.traceToGenesis(subdir, m1, chain));
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0], m1);
}

// 2f. ArchiveWriter::finalizeArchive persists a .node file to the archive directory
//     This verifies that the T4 write path calls persistNode() as part of finalizeArchive()
TEST(MerkleDAGPersist, FinalizeArchivePersistsNodeFile) {
    const std::string subdir = kT4TestDir + "/merkle_via_archive";
    cleanDir(subdir);
    ensureDir(subdir);

    yuki::memory::ArchiveWriter writer(subdir);
    ASSERT_TRUE(writer.beginArchive("persist_check_epoch", cognitiveSchema()));
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "episode_id",  std::vector<int64_t>{77}   },
        { "timestamp",   std::vector<double>{777.0} },
        { "slot",        std::vector<int64_t>{7}    },
        { "free_energy", std::vector<double>{-0.07} },
        { "surprise",    std::vector<double>{0.77}  }
    };
    ASSERT_TRUE(writer.writeRowGroup(rg));
    std::string root;
    ASSERT_TRUE(writer.finalizeArchive(root));

    // A .node file named <root>.node must exist alongside the .yuk file
    std::string node_path = subdir + "/" + root + ".node";
    EXPECT_TRUE(fs::exists(node_path))
        << "finalizeArchive must call persistNode() — expected: " << node_path;

    // The .node file must be loadable and self-consistent
    MerkleDAG dag;
    std::string loaded_c, loaded_p;
    uint64_t loaded_ts = 0;
    ASSERT_TRUE(dag.loadNode(subdir, root, loaded_c, loaded_p, loaded_ts));
    EXPECT_TRUE(dag.verifyNode(loaded_c, loaded_p, root))
        << "Node file written by finalizeArchive must pass verifyNode()";
}


// ═══════════════════════════════════════════════════════════════════════════════
// TEST GROUP 3: SleepThread::archiveEpoch — Promotion Gate
//
// The gate has two observable layers:
//   Layer A (accumulating): dmc_outcome_count_ < min_stable_outcomes (10)
//              → prints "[SleepThread] T3→T4: ACCUMULATING", no .yuk written
//   Layer B (stability):   variance / stability check
//              → prints "[SleepThread] T3→T4: UNSTABLE" if fails, no .yuk written
//              → prints "[SleepThread] T3→T4: STABLE" if passes, writes .yuk
//
// Tests observe the gate via:
//   - Counting epoch_*.yuk files in data/archive after N epochs
//   - Confirming that < 10 epochs → 0 files; ≥ 10 stable epochs → ≥ 1 file
// ═══════════════════════════════════════════════════════════════════════════════

// 3a. archiveEpoch is a no-op when CMF is not wired (null guard)
TEST(ArchiveEpochGate, NullCMFSkipsArchive) {
    // Count existing epoch files before test
    size_t before_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            if (e.path().extension() == ".yuk") before_count++;
        }
    }

    yuki::sleep::SleepThread::Config cfg;
    cfg.idle_threshold = std::chrono::seconds(1);
    cfg.epoch_interval = std::chrono::seconds(1);
    cfg.poll_ms        = std::chrono::milliseconds(100);

    yuki::sleep::SleepThread st(cfg);
    // No setCMF() call → archiveEpoch() must return immediately

    st.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    EXPECT_TRUE(st.isIdle());
    st.stop();

    // No new .yuk files should have been created in data/archive
    size_t after_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            if (e.path().extension() == ".yuk") after_count++;
        }
    }
    EXPECT_EQ(before_count, after_count)
        << "archiveEpoch with null CMF must not write any .yuk files";
}

// 3b. Accumulating phase: < min_stable_outcomes (10) epochs → no .yuk files written
//     CMF is wired but only ~2 epochs run (well below the threshold of 10)
TEST(ArchiveEpochGate, AccumulatingPhaseBlocksWrite) {
    // CMF default paths: data/brain/cmf_episodes.db + data/brain/cmf_vectors*
    // Clean before test to ensure fresh HNSW (avoid capacity overflow from prior runs)
    fs::create_directories("data/brain");
    for (auto& e : fs::directory_iterator("data/brain")) {
        fs::remove(e.path());
    }

    // CMF uses its own internal paths (data/brain/cmf_episodes.db, data/brain/cmf_vectors)
    yuki::memory::CognitiveMemoryFabric cmf;
    ASSERT_TRUE(cmf.init()) << "CMF must initialize successfully";

    // Count existing epoch files before test
    size_t before_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            std::string name = e.path().filename().string();
            if (e.path().extension() == ".yuk" && name.rfind("epoch_", 0) == 0)
                before_count++;
        }
    }

    yuki::sleep::SleepThread::Config cfg;
    cfg.idle_threshold = std::chrono::seconds(1);
    cfg.epoch_interval = std::chrono::seconds(1);  // 1s epochs — fast enough for <10 check
    cfg.poll_ms        = std::chrono::milliseconds(100);

    yuki::sleep::SleepThread st(cfg);
    st.setEpisodicStore(cmf.episodicStore());
    st.setSemanticGraph(cmf.hdcSemanticGraph());
    st.setCMF(&cmf);
    st.start();

    // Run ~2 epochs (well below min_stable_outcomes = 10)
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
    EXPECT_TRUE(st.isIdle());
    st.stop();

    // Count epoch_*.yuk files added during this test
    size_t after_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            std::string name = e.path().filename().string();
            if (e.path().extension() == ".yuk" && name.rfind("epoch_", 0) == 0)
                after_count++;
        }
    }

    EXPECT_EQ(before_count, after_count)
        << "Accumulating phase (<10 outcomes) must not write any new epoch archives";
}

// 3c. Stable phase: ≥ min_stable_outcomes (10) epochs with near-zero variance
//     → promotion gate passes → at least one epoch_.yuk is written
//
// Gate logic (from archiveEpoch):
//   Layer A: dmc_outcome_count_ < 10 → ACCUMULATING (no write)
//   Layer B: variance / stability_threshold → must satisfy variance < threshold
//
// For the gate to pass we need:
//   1. ≥10 epochs accumulated
//   2. Non-zero but stable surprise: wire VSE so counterfactualReplay()
//      produces small non-zero avg_free_energy_delta → non-zero surprise
//   3. Consolidated episodes in the store so archiveEpoch actually writes
//
// At 1s idle_threshold + 1s epoch_interval = ~2s per cycle, 13s gives ~6 cycles.
// But each cycle can run multiple epochs in sequence if idle stays true.
// Using 15s to reliably cross 10 outcomes.
TEST(ArchiveEpochGate, StableOutcomesAllowArchiveWrite) {
    // Clean CMF default paths to avoid HNSW capacity overflow from prior runs
    // CMF uses: data/brain/cmf_episodes.db and data/brain/cmf_vectors* for HNSW
    fs::create_directories("data/brain");
    for (auto& e : fs::directory_iterator("data/brain")) {
        fs::remove(e.path());
    }

    yuki::memory::CognitiveMemoryFabric cmf;
    ASSERT_TRUE(cmf.init());

    // Pre-insert and immediately consolidate 5 episodes so archiveEpoch has rows to write
    auto* episodic = cmf.episodicStore();
    ASSERT_NE(episodic, nullptr);
    for (int i = 0; i < 5; ++i) {
        yuki::memory::EpisodeRecord rec;
        rec.timestamp_ms = static_cast<uint64_t>(1000 + i * 100);
        rec.source       = "gate_test";
        rec.text         = "stable_ep_" + std::to_string(i);
        rec.intent_label = "general";
        rec.confidence   = 0.8f;
        std::vector<float> vec(24, 0.5f);
        ASSERT_TRUE(episodic->insert(rec, vec));
        episodic->markConsolidated(i + 1);
    }

    size_t before_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            std::string name = e.path().filename().string();
            if (e.path().extension() == ".yuk" && name.rfind("epoch_", 0) == 0)
                before_count++;
        }
    }

    // Wire VSE so counterfactualReplay() evaluates 5 policies and produces
    // non-zero avg_free_energy_delta → non-zero surprise → gate stability check can pass
    yuki::inference::VariationalStateEstimator vse;

    // epoch_interval = 1s: with 1s idle threshold, each cycle is ~2s.
    // 15s test duration gives ~7 cycles. The SleepThread runs multiple
    // epochs per idle session, so 10+ outcomes will accumulate.
    yuki::sleep::SleepThread::Config cfg;
    cfg.idle_threshold = std::chrono::seconds(1);
    cfg.epoch_interval = std::chrono::seconds(1);
    cfg.poll_ms        = std::chrono::milliseconds(50);

    yuki::sleep::SleepThread st(cfg);
    st.setEpisodicStore(episodic);
    st.setSemanticGraph(cmf.hdcSemanticGraph());
    st.setVSE(&vse);         // VSE enables counterfactualReplay → non-zero surprise
    st.setCMF(&cmf);
    st.start();

    // 15s: idle fires at 1s, then each epoch runs + 1s inter-epoch pause.
    // With fast epochs (<100ms), 15s gives at minimum 7 epochs and typically 10+.
    // Once ≥10 outcomes are accumulated and variance is stable, gate passes.
    std::this_thread::sleep_for(std::chrono::milliseconds(15000));
    EXPECT_TRUE(st.isIdle());
    st.stop();

    size_t after_count = 0;
    if (fs::exists("data/archive")) {
        for (auto& e : fs::directory_iterator("data/archive")) {
            std::string name = e.path().filename().string();
            if (e.path().extension() == ".yuk" && name.rfind("epoch_", 0) == 0)
                after_count++;
        }
    }

    EXPECT_GT(after_count, before_count)
        << "After ≥10 stable outcomes with VSE wired, promotion gate must allow "
        << "at least one epoch archive write. before=" << before_count
        << " after=" << after_count;
}


// ═══════════════════════════════════════════════════════════════════════════════
// TEST GROUP 4: ArchiveWriter::queryBySurprise — API Contract
// The full read path is a stub (returns false). These tests verify:
//   a. It doesn't crash on empty/populated directories
//   b. It returns false + empty vector on every call (stub contract)
//   c. Related stubs (listEpochChain, readArchiveByMerkle) behave consistently
// ═══════════════════════════════════════════════════════════════════════════════

// 4a. queryBySurprise on empty directory: returns false, leaves vector empty
TEST(QueryBySurprise, EmptyDirectoryReturnsFalse) {
    const std::string subdir = kT4TestDir + "/qbs_empty";
    cleanDir(subdir);
    ensureDir(subdir);

    std::vector<std::string> roots;
    bool result = yuki::memory::ArchiveWriter::queryBySurprise(
        subdir, /*surprise_threshold=*/0.5, /*max_results=*/10, roots);

    EXPECT_FALSE(result)  << "queryBySurprise stub must return false";
    EXPECT_TRUE(roots.empty()) << "queryBySurprise must not populate out_merkle_roots on failure";
}

// 4b. queryBySurprise on a directory with .yuk + .node files: doesn't crash
TEST(QueryBySurprise, PopulatedDirectoryDoesNotCrash) {
    const std::string subdir = kT4TestDir + "/qbs_populated";
    cleanDir(subdir);
    ensureDir(subdir);

    // Write 3 chained epochs
    std::string prev_root;
    for (int i = 0; i < 3; ++i) {
        yuki::memory::ArchiveWriter writer(subdir);
        if (!prev_root.empty()) writer.setParentMerkleRoot(prev_root);
        ASSERT_TRUE(writer.beginArchive("qbs_epoch_" + std::to_string(i), cognitiveSchema()));
        std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
            { "episode_id",  std::vector<int64_t>{static_cast<int64_t>(i)}         },
            { "timestamp",   std::vector<double>{static_cast<double>(i + 1.0)}     },
            { "slot",        std::vector<int64_t>{static_cast<int64_t>(i)}         },
            { "free_energy", std::vector<double>{-0.1 * (i + 1)}                   },
            { "surprise",    std::vector<double>{0.2 * (i + 1)}                    }
        };
        ASSERT_TRUE(writer.writeRowGroup(rg));
        std::string root;
        ASSERT_TRUE(writer.finalizeArchive(root));
        prev_root = root;
    }

    std::vector<std::string> roots;
    EXPECT_NO_THROW({
        yuki::memory::ArchiveWriter::queryBySurprise(
            subdir, /*threshold=*/0.1, /*max_results=*/5, roots);
    }) << "queryBySurprise must not throw on a populated directory";
}

// 4c. queryBySurprise with threshold=0.0 (catch all) still returns false (stub)
TEST(QueryBySurprise, ZeroThresholdStillFalse) {
    const std::string subdir = kT4TestDir + "/qbs_zero_thresh";
    cleanDir(subdir);
    ensureDir(subdir);

    std::vector<std::string> roots;
    bool result = yuki::memory::ArchiveWriter::queryBySurprise(
        subdir, /*threshold=*/0.0, /*max_results=*/100, roots);

    EXPECT_FALSE(result);
    EXPECT_TRUE(roots.empty());
}

// 4d. listEpochChain stub: returns false, leaves vector empty
TEST(QueryBySurprise, ListEpochChainStubReturnsFalse) {
    const std::string subdir = kT4TestDir + "/list_chain";
    cleanDir(subdir);
    ensureDir(subdir);

    std::vector<std::string> roots;
    bool result = yuki::memory::ArchiveWriter::listEpochChain(subdir, roots);

    EXPECT_FALSE(result)        << "listEpochChain stub must return false";
    EXPECT_TRUE(roots.empty())  << "listEpochChain stub must leave out vector empty";
}

// 4e. readArchiveByMerkle returns false for an unknown (non-existent) Merkle root
TEST(QueryBySurprise, ReadArchiveByMerkleUnknownRootFails) {
    const std::string subdir = kT4TestDir + "/read_by_merkle_unknown";
    cleanDir(subdir);
    ensureDir(subdir);

    std::string fake_root(64, 'a');
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> cols;
    std::string schema_json;

    bool result = yuki::memory::ArchiveWriter::readArchiveByMerkle(
        subdir, fake_root, cols, schema_json);

    EXPECT_FALSE(result)
        << "readArchiveByMerkle with unknown Merkle root must return false";
}

// 4f. readArchiveByMerkle with a real Merkle root (known .node file):
//     must find the node and not crash, even if full data read is stubbed
TEST(QueryBySurprise, ReadArchiveByMerkleKnownRootDoesNotCrash) {
    const std::string subdir = kT4TestDir + "/read_by_merkle_valid";
    cleanDir(subdir);
    ensureDir(subdir);

    // Write one archive epoch → creates .yuk + .node file
    yuki::memory::ArchiveWriter writer(subdir);
    ASSERT_TRUE(writer.beginArchive("known_epoch", cognitiveSchema()));
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "episode_id",  std::vector<int64_t>{42}  },
        { "timestamp",   std::vector<double>{99.0} },
        { "slot",        std::vector<int64_t>{7}   },
        { "free_energy", std::vector<double>{-0.3} },
        { "surprise",    std::vector<double>{0.7}  }
    };
    ASSERT_TRUE(writer.writeRowGroup(rg));
    std::string root;
    ASSERT_TRUE(writer.finalizeArchive(root));
    ASSERT_EQ(root.size(), 64u);

    // readArchiveByMerkle must load the node metadata without crashing
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> cols;
    std::string schema_json;
    EXPECT_NO_THROW({
        yuki::memory::ArchiveWriter::readArchiveByMerkle(subdir, root, cols, schema_json);
    }) << "readArchiveByMerkle on a valid Merkle root must not throw";
}

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
