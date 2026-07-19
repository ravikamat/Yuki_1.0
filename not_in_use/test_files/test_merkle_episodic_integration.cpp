// test_merkle_episodic_integration.cpp — Yuki_1.0
// Tests: MerkleDAG standalone (SHA-256) + EpisodicStore Merkle chain integration.

#include <gtest/gtest.h>
#include "brain/memory/MerkleDAG.h"
#include "brain/memory/EpisodicStore.h"
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string tmpDb(const std::string& tag) {
    return (fs::temp_directory_path() / ("yuki_test_" + tag + ".db")).string();
}
static std::string tmpIdx(const std::string& tag) {
    return (fs::temp_directory_path() / ("yuki_test_" + tag + ".idx")).string();
}
static void rmTmp(const std::string& tag) {
    fs::remove(tmpDb(tag));
    fs::remove(tmpIdx(tag));
    // also remove any HNSW sidecar files
    for (auto& p : fs::directory_iterator(fs::temp_directory_path())) {
        auto name = p.path().filename().string();
        if (name.rfind("yuki_test_" + tag, 0) == 0) fs::remove(p);
    }
}

static yuki::memory::EpisodeRecord makeRecord(const std::string& text,
                                               uint64_t ts_ms = 1000) {
    yuki::memory::EpisodeRecord r;
    r.timestamp_ms = ts_ms;
    r.source       = "test";
    r.text         = text;
    r.topic_tag    = "unit";
    r.confidence   = 0.9f;
    r.intent_label = "general";
    return r;
}

// ── MerkleDAG unit tests ──────────────────────────────────────────────────────

TEST(MerkleDAG, HashStringIs64Chars) {
    MerkleDAG dag;
    EXPECT_EQ(dag.hashString("hello").size(), 64u);
}

TEST(MerkleDAG, HashStringIsDeterministic) {
    MerkleDAG dag;
    EXPECT_EQ(dag.hashString("yuki"), dag.hashString("yuki"));
}

TEST(MerkleDAG, HashStringDiffers) {
    MerkleDAG dag;
    EXPECT_NE(dag.hashString("abc"), dag.hashString("xyz"));
}

TEST(MerkleDAG, KnownSHA256Empty) {
    // SHA-256("") known vector
    MerkleDAG dag;
    EXPECT_EQ(dag.hashString(""),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(MerkleDAG, CreateNodeHexLen) {
    MerkleDAG dag;
    std::string m = dag.createNode(dag.hashString("content"), std::string(64,'0'));
    EXPECT_EQ(m.size(), 64u);
}

TEST(MerkleDAG, CreateNodeChainDiffers) {
    MerkleDAG dag;
    std::string root(64, '0');
    std::string m1 = dag.createNode(dag.hashString("ep1"), root);
    std::string m2 = dag.createNode(dag.hashString("ep2"), m1);
    EXPECT_NE(m1, m2);
}

TEST(MerkleDAG, VerifyNodePass) {
    MerkleDAG dag;
    std::string parent(64, '0');
    std::string content = dag.hashString("data");
    std::string merkle  = dag.createNode(content, parent);
    EXPECT_TRUE(dag.verifyNode(content, parent, merkle));
}

TEST(MerkleDAG, VerifyNodeFailOnTamper) {
    MerkleDAG dag;
    std::string parent(64, '0');
    std::string content = dag.hashString("data");
    std::string merkle  = dag.createNode(content, parent);
    merkle[0] = (merkle[0] == 'a') ? 'b' : 'a';
    EXPECT_FALSE(dag.verifyNode(content, parent, merkle));
}

// ── EpisodicStore + Merkle chain integration ──────────────────────────────────

TEST(MerkleEpisodic, GetMerkleRootEmptyBeforeInsert) {
    const std::string tag = "empty";
    rmTmp(tag);
    {
        yuki::memory::EpisodicStore store(tmpDb(tag), tmpIdx(tag));
        ASSERT_TRUE(store.init());
        EXPECT_TRUE(store.getMerkleRoot(0).empty());
    }
    rmTmp(tag);
}

TEST(MerkleEpisodic, RootIs64CharsAfterInsert) {
    const std::string tag = "root64";
    rmTmp(tag);
    {
        yuki::memory::EpisodicStore store(tmpDb(tag), tmpIdx(tag));
        ASSERT_TRUE(store.init());
        std::vector<float> vec(24, 0.5f);
        EXPECT_TRUE(store.insert(makeRecord("hello"), vec));
        std::string root = store.getMerkleRoot(0);
        EXPECT_EQ(root.size(), 64u);
    }
    rmTmp(tag);
}

TEST(MerkleEpisodic, ChainGrowsWithDifferentRoots) {
    const std::string tag = "chain3";
    rmTmp(tag);
    {
        yuki::memory::EpisodicStore store(tmpDb(tag), tmpIdx(tag));
        ASSERT_TRUE(store.init());
        std::vector<float> vec(24, 0.1f);
        store.insert(makeRecord("ep1", 1000), vec);
        std::string r1 = store.getMerkleRoot(0);
        store.insert(makeRecord("ep2", 2000), vec);
        std::string r2 = store.getMerkleRoot(0);
        store.insert(makeRecord("ep3", 3000), vec);
        std::string r3 = store.getMerkleRoot(0);
        EXPECT_EQ(r1.size(), 64u);
        EXPECT_EQ(r2.size(), 64u);
        EXPECT_EQ(r3.size(), 64u);
        EXPECT_NE(r1, r2);
        EXPECT_NE(r2, r3);
    }
    rmTmp(tag);
}

TEST(MerkleEpisodic, VerifyChainValidOnCleanData) {
    const std::string tag = "verify";
    rmTmp(tag);
    {
        yuki::memory::EpisodicStore store(tmpDb(tag), tmpIdx(tag));
        ASSERT_TRUE(store.init());
        std::vector<float> vec(24, 0.2f);
        for (int i = 0; i < 3; ++i)
            store.insert(makeRecord("ep" + std::to_string(i), 1000 + i * 100), vec);
        auto result = store.verifyChain(0);
        EXPECT_EQ(result.first_broken_id, -1);
    }
    rmTmp(tag);
}

TEST(MerkleEpisodic, MerkleRootDiffersAcrossStores) {
    const std::string tag1 = "storeA", tag2 = "storeB";
    rmTmp(tag1); rmTmp(tag2);
    {
        yuki::memory::EpisodicStore s1(tmpDb(tag1), tmpIdx(tag1));
        yuki::memory::EpisodicStore s2(tmpDb(tag2), tmpIdx(tag2));
        ASSERT_TRUE(s1.init()); ASSERT_TRUE(s2.init());
        std::vector<float> v(24, 0.3f);
        s1.insert(makeRecord("data-A", 1000), v);
        s2.insert(makeRecord("data-B", 9999), v);   // different timestamp → different hash
        EXPECT_NE(s1.getMerkleRoot(0), s2.getMerkleRoot(0));
    }
    rmTmp(tag1); rmTmp(tag2);
}
