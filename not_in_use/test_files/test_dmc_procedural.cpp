#include <gtest/gtest.h>
#include <filesystem>
#include <cstdio>
#include <vector>
#include <array>
#include "brain/memory/TinyMLP.h"
#include "brain/memory/ProceduralStore.h"
#include "brain/memory/DifferentialMemoryController.h"

using namespace yuki::memory;
namespace fs = std::filesystem;

// ── 1. TinyMLP forward pass is deterministic ────────────────────────────────────
TEST(DMC, TinyMLPForwardPassIsDeterministic) {
    TinyMLP mlp1(12345ULL);
    TinyMLP mlp2(12345ULL);

    std::array<float, 48> input;
    input.fill(0.5f);

    auto out1 = mlp1.forward(input);
    auto out2 = mlp2.forward(input);

    ASSERT_EQ(out1.size(), out2.size());
    for (size_t i = 0; i < out1.size(); ++i) {
        EXPECT_FLOAT_EQ(out1[i], out2[i]) << "Deterministic mismatch at index " << i;
    }
}

// ── 2. ProceduralStore roundtrip and filesystem fallback ───────────────────────
TEST(DMC, ProceduralStoreRoundtripAndFallback) {
    std::string db  = (fs::temp_directory_path() / "test_proc.db").string();
    std::string dir = (fs::temp_directory_path() / "test_proc_fs").string() + "/";

    std::remove(db.c_str());
    fs::remove_all(dir);

    ProceduralStore store;
    ASSERT_TRUE(store.init(db, dir));

    // Test DB storage (small blob)
    std::vector<uint8_t> small_blob = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_TRUE(store.store("small_key", ProceduralStore::BlobType::DMC_WEIGHTS, small_blob));
    EXPECT_TRUE(store.exists("small_key"));

    auto retrieved_small = store.retrieve("small_key");
    ASSERT_TRUE(retrieved_small.has_value());
    EXPECT_EQ(*retrieved_small, small_blob);

    // Test FS storage fallback (large blob > 64KB)
    std::vector<uint8_t> large_blob(70000, 0xAA);
    EXPECT_TRUE(store.store("large_key", ProceduralStore::BlobType::SESSION_CHECKPOINT, large_blob));
    EXPECT_TRUE(store.exists("large_key"));

    // Verify FS binary file exists
    std::string fs_file_path = dir + "large_key.bin";
    EXPECT_TRUE(fs::exists(fs_file_path));

    auto retrieved_large = store.retrieve("large_key");
    ASSERT_TRUE(retrieved_large.has_value());
    EXPECT_EQ(*retrieved_large, large_blob);

    // Verify integrity checks
    EXPECT_TRUE(store.verifyIntegrity("small_key"));
    EXPECT_TRUE(store.verifyIntegrity("large_key"));

    // Clean up
    store.close();
    std::remove(db.c_str());
    fs::remove_all(dir);
}

// ── 3. DMC promotion and safety envelope overrides ──────────────────────────────
TEST(DMC, DmcPromotionSafetyEnvelopeOverride) {
    std::string db  = (fs::temp_directory_path() / "test_dmc_promo.db").string();
    std::string dir = (fs::temp_directory_path() / "test_dmc_promo_fs").string() + "/";

    std::remove(db.c_str());
    fs::remove_all(dir);

    ProceduralStore store;
    ASSERT_TRUE(store.init(db, dir));

    DifferentialMemoryController dmc;
    ASSERT_TRUE(dmc.init(&store, "weights_key"));

    // Set safety config
    DifferentialMemoryController::SafetyConfig cfg;
    cfg.min_confidence_threshold = 0.5f;
    cfg.max_entropy_for_write = 1.0f; // low entropy limit
    cfg.min_outcomes_before_learning = 5;
    dmc.setSafetyConfig(cfg);

    // Evaluate with high entropy (uniform vse posterior → high uncertainty)
    std::array<float, 24> high_entropy_posterior;
    high_entropy_posterior.fill(1.0f / 24.0f); // high uncertainty

    std::array<float, 24> context;
    context.fill(0.5f);

    auto [decision1, token1] = dmc.evaluate(high_entropy_posterior, context);
    
    // Safety envelope C3 constraint: high VSE entropy must override PROMOTE or WRITE to READ
    if (decision1.action == DMCDecision::Action::PROMOTE || decision1.action == DMCDecision::Action::WRITE) {
        EXPECT_TRUE(decision1.safety_override);
        EXPECT_EQ(decision1.action, DMCDecision::Action::READ);
    }

    // Clean up
    store.close();
    std::remove(db.c_str());
    fs::remove_all(dir);
}

// ── 4. SleepThread DMC weight learning consolidation ────────────────────────────
TEST(DMC, SleepThreadConsolidationLearnsWeights) {
    std::string db  = (fs::temp_directory_path() / "test_sleep_learn.db").string();
    std::string dir = (fs::temp_directory_path() / "test_sleep_learn_fs").string() + "/";

    std::remove(db.c_str());
    fs::remove_all(dir);

    ProceduralStore store;
    ASSERT_TRUE(store.init(db, dir));

    DifferentialMemoryController dmc;
    ASSERT_TRUE(dmc.init(&store, "learning_weights"));

    // Configure safety thresholds for quick training
    DifferentialMemoryController::SafetyConfig cfg;
    cfg.min_confidence_threshold = -1e9f; // disable C1 override for testing
    cfg.max_entropy_for_write = 1e9f;      // disable C3 override for testing
    cfg.min_outcomes_before_learning = 5;  // consolidate after 5 turns
    dmc.setSafetyConfig(cfg);

    // Save initial weights
    ASSERT_TRUE(dmc.saveWeights());
    auto initial_blob = store.retrieve("learning_weights");
    ASSERT_TRUE(initial_blob.has_value());

    // Generate 5 outcomes with success rewards
    std::array<float, 24> posterior{};
    posterior[0] = 1.0f; // low entropy

    std::array<float, 24> context{};
    context.fill(0.5f);

    for (int i = 0; i < 5; ++i) {
        auto [decision, token] = dmc.evaluate(posterior, context);
        dmc.recordOutcome(token, true, 1.0f); // reinforce success
    }

    // Consolidate (SleepThread task 7)
    EXPECT_TRUE(dmc.consolidate());

    // Verify weights changed after training
    ASSERT_TRUE(dmc.saveWeights());
    auto trained_blob = store.retrieve("learning_weights");
    ASSERT_TRUE(trained_blob.has_value());

    EXPECT_NE(*initial_blob, *trained_blob) << "Weights did not change after consolidation learning!";
    EXPECT_GT(dmc.getUpdateCount(), 0);

    // Clean up
    store.close();
    std::remove(db.c_str());
    fs::remove_all(dir);
}
