// ColumnarArchiveFormat.h — Yuki_1.0 T4 binary columnar archive (.yuk format)
// Header: magic + version + schema_json
// Row groups: row_count + column_chunks (type + uncompressed_len + compressed_len + data)
// Footer: row_group_offsets + footer_offset + merkle_root
// Compression: RLE for INT64/DOUBLE, dictionary for STRING, raw for FLOAT_ARRAY (pure C++17)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <iosfwd>

namespace yuki {
namespace memory {

struct ColumnSchema {
    std::string name;
    enum class Type : uint32_t { INT64 = 0, DOUBLE = 1, STRING = 2, FLOAT_ARRAY = 3, VECTOR_F32 = 4, JSON_BLOB = 5 } type;
    size_t max_string_len = 256;
};

struct ColumnData {
    std::string name;
    std::variant<
        std::vector<int64_t>,           // INT64
        std::vector<double>,            // DOUBLE
        std::vector<std::string>,       // STRING
        std::vector<std::vector<float>> // FLOAT_ARRAY
    > data;
};

class ColumnarArchiveFormat {
public:
    using ColumnSchema = yuki::memory::ColumnSchema;
    using ColumnData   = yuki::memory::ColumnData;

    static constexpr uint32_t kMagic   = 0x59554B01u;
    static constexpr uint32_t kVersion = 1u;

    struct Footer {
        std::vector<uint64_t> row_group_offsets;
        uint64_t              footer_offset  = 0;
        std::string           merkle_root;     // hex SHA-256
    };

    // Write magic + version + schema JSON to stream. Returns bytes written.
    static bool writeHeader(std::ostream& out,
                            const std::vector<ColumnSchema>& schema);

    // Serialise one row group. Updates out_chunk_hashes with sha256 per column chunk.
    // Returns false on error.
    static bool writeRowGroup(std::ostream&                    out,
                              const std::vector<ColumnData>&   columns,
                              std::vector<std::string>&        out_chunk_hashes);

    // Write footer (offsets array + footer_offset + merkle_root).
    static bool writeFooter(std::ostream&                    out,
                            const std::vector<uint64_t>&     offsets,
                            const std::vector<std::string>&  chunk_hashes,
                            std::string&                     out_merkle_root);

    // Read footer from an open seekable istream.
    static bool readFooter(std::istream& in, Footer& out);

    // Read header from an open seekable istream. Returns schema JSON.
    static bool readHeader(std::istream& in, std::string& out_schema_json);

    // Read one row group from an open seekable istream at the given offset.
    // Returns decompressed column data.
    static bool readRowGroup(std::istream& in, uint64_t offset,
                             std::vector<ColumnData>& out_columns);

    // Full file integrity verification: recompute Merkle root and compare.
    static bool verifyArchive(const std::string& filepath);

private:
    // Low-level integer serialisation (little-endian)
    static bool write32(std::ostream& o, uint32_t v);
    static bool write64(std::ostream& o, uint64_t v);
    static bool read32 (std::istream& i, uint32_t& v);
    static bool read64 (std::istream& i, uint64_t& v);

    // Serialise column data to raw bytes
    static std::vector<uint8_t> serializeInt64 (const std::vector<int64_t>&          v);
    static std::vector<uint8_t> serializeDouble(const std::vector<double>&            v);
    static std::vector<uint8_t> serializeString(const std::vector<std::string>&       v);
    static std::vector<uint8_t> serializeFloatArray(const std::vector<std::vector<float>>& v);

    // Pure C++17 compression (no zlib)
    static std::vector<uint8_t> rleCompress  (const std::vector<uint8_t>& raw);
    static std::vector<uint8_t> dictCompress (const std::vector<uint8_t>& raw,
                                              const std::vector<std::string>& strs);

    // Merkle tree over leaf hashes (pairwise SHA-256, pad odd leaves by duplication)
    static std::string merkleRoot(const std::vector<std::string>& leaf_hashes);
    static std::string sha256Pair(const std::string& a, const std::string& b);

    // MerkleDAG::hashString wrapper (declared here, defined in .cpp)
    static std::string hashBytes(const std::vector<uint8_t>& data);
};

} // namespace memory
} // namespace yuki
