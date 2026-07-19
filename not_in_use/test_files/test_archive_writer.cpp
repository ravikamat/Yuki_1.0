// tests/test_archive_writer.cpp — Phase C: ArchiveWriter / ColumnarArchiveFormat tests
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "brain/memory/ColumnarArchiveFormat.h"
#include "brain/memory/ArchiveWriter.h"

namespace fs = std::filesystem;

static const std::string kTestDir = "data/test_archive";

// ── Helper: build a simple schema ─────────────────────────────────────────────
static std::vector<yuki::memory::ColumnarArchiveFormat::ColumnSchema> makeSchema() {
    return {
        { "id",        yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64   },
        { "score",     yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE  },
        { "label",     yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::STRING  }
    };
}

// ── TEST 1: Header writes magic bytes correctly ───────────────────────────────
TEST(Archive, HeaderRoundtrip) {
    // Verify magic constant value
    EXPECT_EQ(yuki::memory::ColumnarArchiveFormat::kMagic,   0x59554B01u);
    EXPECT_EQ(yuki::memory::ColumnarArchiveFormat::kVersion, 1u);

    yuki::memory::ArchiveWriter writer(kTestDir);
    std::string merkle;
    bool ok = writer.beginArchive("header_test", makeSchema());
    ASSERT_TRUE(ok) << "beginArchive must succeed";

    // Write empty row group so footer can be finalised
    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> empty_rg = {
        { "id",    std::vector<int64_t>{} },
        { "score", std::vector<double>{}  },
        { "label", std::vector<std::string>{} }
    };
    writer.writeRowGroup(empty_rg);
    ok = writer.finalizeArchive(merkle);
    ASSERT_TRUE(ok) << "finalizeArchive must succeed";

    // Read back first 4 bytes and verify magic
    std::string filepath = kTestDir + "/header_test.yuk";
    std::ifstream in(filepath, std::ios::binary);
    ASSERT_TRUE(in.is_open()) << "Archive file must exist";
    uint32_t magic = 0;
    uint8_t b[4];
    in.read(reinterpret_cast<char*>(b), 4);
    magic = uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
    EXPECT_EQ(magic, yuki::memory::ColumnarArchiveFormat::kMagic);
}

// ── TEST 2: Row group integrity via Merkle root ───────────────────────────────
TEST(Archive, RowGroupIntegrity) {
    yuki::memory::ArchiveWriter writer(kTestDir);

    auto schema = makeSchema();
    ASSERT_TRUE(writer.beginArchive("integrity_test", schema));

    // Build 100-row dataset
    std::vector<int64_t>     ids(100);
    std::vector<double>      scores(100);
    std::vector<std::string> labels(100);
    for (int i = 0; i < 100; ++i) {
        ids[i]    = static_cast<int64_t>(i);
        scores[i] = static_cast<double>(i) * 0.01;
        labels[i] = "label_" + std::to_string(i);
    }

    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "id",    ids    },
        { "score", scores },
        { "label", labels }
    };
    ASSERT_TRUE(writer.writeRowGroup(rg));

    std::string merkle_root;
    ASSERT_TRUE(writer.finalizeArchive(merkle_root));

    // Merkle root must be non-empty
    EXPECT_FALSE(merkle_root.empty()) << "Merkle root must be set";
    // Verify: open file and check Merkle root is consistent
    std::string filepath = kTestDir + "/integrity_test.yuk";
    EXPECT_TRUE(yuki::memory::ArchiveWriter::verifyArchive(filepath))
        << "Archive integrity verification must pass";
}

// ── TEST 3: Tampered file fails verification ──────────────────────────────────
TEST(Archive, VerifyTamperedFile) {
    yuki::memory::ArchiveWriter writer(kTestDir);
    ASSERT_TRUE(writer.beginArchive("tamper_test", makeSchema()));

    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "id",    std::vector<int64_t>{1, 2, 3}           },
        { "score", std::vector<double>{0.1, 0.2, 0.3}      },
        { "label", std::vector<std::string>{"a", "b", "c"} }
    };
    ASSERT_TRUE(writer.writeRowGroup(rg));

    std::string merkle;
    ASSERT_TRUE(writer.finalizeArchive(merkle));

    std::string filepath = kTestDir + "/tamper_test.yuk";

    // Tamper: flip one byte in the middle of the file (within row group data)
    {
        auto sz = std::filesystem::file_size(filepath);
        std::fstream f(filepath, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(f.is_open());
        auto pos = sz / 2;
        f.seekg(pos, std::ios::beg);
        char c = 0;
        f.read(&c, 1);
        c ^= 0xFF;
        f.seekp(pos, std::ios::beg);
        f.write(&c, 1);
    }

    // Verify should now FAIL (Merkle root mismatch)
    bool verified = yuki::memory::ArchiveWriter::verifyArchive(filepath);
    EXPECT_FALSE(verified) << "Tampered archive must fail verification";
}

// ── TEST 4: Sleep integration — ArchiveWriter is accessible via CMF ───────────
TEST(Archive, SleepIntegration) {
    // Verify ArchiveWriter can be constructed and begin/finalize works
    yuki::memory::ArchiveWriter writer(kTestDir);
    static const std::vector<yuki::memory::ColumnarArchiveFormat::ColumnSchema> kSchema = {
        { "episode_id", yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64  },
        { "timestamp",  yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::DOUBLE },
        { "slot",       yuki::memory::ColumnarArchiveFormat::ColumnSchema::Type::INT64  }
    };

    ASSERT_TRUE(writer.beginArchive("sleep_epoch_test", kSchema));

    std::vector<yuki::memory::ColumnarArchiveFormat::ColumnData> rg = {
        { "episode_id", std::vector<int64_t>{100, 101, 102}  },
        { "timestamp",  std::vector<double>{1.0, 2.0, 3.0}   },
        { "slot",       std::vector<int64_t>{0, 1, 2}        }
    };
    ASSERT_TRUE(writer.writeRowGroup(rg));

    std::string merkle;
    ASSERT_TRUE(writer.finalizeArchive(merkle));

    // File must exist
    std::string filepath = kTestDir + "/sleep_epoch_test.yuk";
    EXPECT_TRUE(fs::exists(filepath)) << "Archive file must be created";
    EXPECT_GT(fs::file_size(filepath), 0u) << "Archive file must be non-empty";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
