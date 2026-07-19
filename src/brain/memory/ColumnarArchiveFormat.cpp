// ColumnarArchiveFormat.cpp — .yuk binary format implementation (pure C++17)
#include "brain/memory/ColumnarArchiveFormat.h"
#include "brain/memory/MerkleDAG.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <numeric>

namespace yuki {
namespace memory {

// ── Little-endian integer I/O ─────────────────────────────────────────────────
bool ColumnarArchiveFormat::write32(std::ostream& o, uint32_t v) {
    const char b[4] = { char(v), char(v>>8), char(v>>16), char(v>>24) };
    o.write(b, 4);
    return o.good();
}
bool ColumnarArchiveFormat::write64(std::ostream& o, uint64_t v) {
    const char b[8] = {
        char(v), char(v>>8), char(v>>16), char(v>>24),
        char(v>>32), char(v>>40), char(v>>48), char(v>>56) };
    o.write(b, 8);
    return o.good();
}
bool ColumnarArchiveFormat::read32(std::istream& i, uint32_t& v) {
    uint8_t b[4]; i.read(reinterpret_cast<char*>(b), 4);
    if (!i.good()) return false;
    v = uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
    return true;
}
bool ColumnarArchiveFormat::read64(std::istream& i, uint64_t& v) {
    uint8_t b[8]; i.read(reinterpret_cast<char*>(b), 8);
    if (!i.good()) return false;
    v = 0;
    for (int k = 0; k < 8; ++k) v |= (uint64_t(b[k]) << (8*k));
    return true;
}

// ── Serialisation helpers ─────────────────────────────────────────────────────
std::vector<uint8_t> ColumnarArchiveFormat::serializeInt64(const std::vector<int64_t>& v) {
    std::vector<uint8_t> out(v.size() * 8);
    for (size_t i = 0; i < v.size(); ++i) {
        uint64_t u;
        std::memcpy(&u, &v[i], 8);
        for (int k = 0; k < 8; ++k) out[i*8+k] = uint8_t(u >> (8*k));
    }
    return out;
}
std::vector<uint8_t> ColumnarArchiveFormat::serializeDouble(const std::vector<double>& v) {
    std::vector<uint8_t> out(v.size() * 8);
    for (size_t i = 0; i < v.size(); ++i) {
        uint64_t u;
        std::memcpy(&u, &v[i], 8);
        for (int k = 0; k < 8; ++k) out[i*8+k] = uint8_t(u >> (8*k));
    }
    return out;
}
std::vector<uint8_t> ColumnarArchiveFormat::serializeString(const std::vector<std::string>& v) {
    // Format: uint32_t count, then per-entry: uint32_t len + bytes
    std::vector<uint8_t> out;
    auto push32 = [&](uint32_t x) {
        out.push_back(uint8_t(x)); out.push_back(uint8_t(x>>8));
        out.push_back(uint8_t(x>>16)); out.push_back(uint8_t(x>>24));
    };
    push32(static_cast<uint32_t>(v.size()));
    for (const auto& s : v) {
        push32(static_cast<uint32_t>(s.size()));
        for (char c : s) out.push_back(static_cast<uint8_t>(c));
    }
    return out;
}
std::vector<uint8_t>
ColumnarArchiveFormat::serializeFloatArray(const std::vector<std::vector<float>>& v) {
    // Format: uint32_t outer_count, then per-array: uint32_t len + float bytes
    std::vector<uint8_t> out;
    auto push32 = [&](uint32_t x) {
        out.push_back(uint8_t(x)); out.push_back(uint8_t(x>>8));
        out.push_back(uint8_t(x>>16)); out.push_back(uint8_t(x>>24));
    };
    push32(static_cast<uint32_t>(v.size()));
    for (const auto& arr : v) {
        push32(static_cast<uint32_t>(arr.size()));
        for (float f : arr) {
            uint32_t u; std::memcpy(&u, &f, 4);
            out.push_back(uint8_t(u));      out.push_back(uint8_t(u>>8));
            out.push_back(uint8_t(u>>16));  out.push_back(uint8_t(u>>24));
        }
    }
    return out;
}

// ── Pure C++17 RLE compression ─────────────────────────────────────────────────
// Encodes as (value byte, count uint32_t) pairs. Only beneficial for repeated bytes.
std::vector<uint8_t> ColumnarArchiveFormat::rleCompress(const std::vector<uint8_t>& raw) {
    if (raw.empty()) return {};
    std::vector<uint8_t> out;
    out.reserve(raw.size());
    size_t i = 0;
    while (i < raw.size()) {
        uint8_t val = raw[i];
        uint32_t cnt = 1;
        while (i + cnt < raw.size() && raw[i + cnt] == val && cnt < 0xFFFFFFFFu) ++cnt;
        out.push_back(val);
        out.push_back(uint8_t(cnt));       out.push_back(uint8_t(cnt>>8));
        out.push_back(uint8_t(cnt>>16));   out.push_back(uint8_t(cnt>>24));
        i += cnt;
    }
    return out;
}

// ── Dictionary compression (for STRING) ───────────────────────────────────────
// Builds index from unique strings → uint16_t id.
// Format: dict_count (uint32) + dict_entries (serialized) + indices (uint16_t per row)
std::vector<uint8_t> ColumnarArchiveFormat::dictCompress(
    const std::vector<uint8_t>& /*raw*/,
    const std::vector<std::string>& strs)
{
    // Build dictionary
    std::vector<std::string> dict;
    std::vector<uint16_t> indices;
    indices.reserve(strs.size());
    for (const auto& s : strs) {
        auto it = std::find(dict.begin(), dict.end(), s);
        if (it == dict.end()) {
            indices.push_back(static_cast<uint16_t>(dict.size()));
            dict.push_back(s);
        } else {
            indices.push_back(static_cast<uint16_t>(it - dict.begin()));
        }
    }

    std::vector<uint8_t> out;
    auto push32 = [&](uint32_t x) {
        out.push_back(uint8_t(x)); out.push_back(uint8_t(x>>8));
        out.push_back(uint8_t(x>>16)); out.push_back(uint8_t(x>>24));
    };
    // Dict entries
    push32(static_cast<uint32_t>(dict.size()));
    for (const auto& s : dict) {
        push32(static_cast<uint32_t>(s.size()));
        for (char c : s) out.push_back(static_cast<uint8_t>(c));
    }
    // Indices (uint16_t each)
    push32(static_cast<uint32_t>(indices.size()));
    for (uint16_t idx : indices) {
        out.push_back(uint8_t(idx)); out.push_back(uint8_t(idx>>8));
    }
    return out;
}

// ── Hash helpers ──────────────────────────────────────────────────────────────
std::string ColumnarArchiveFormat::hashBytes(const std::vector<uint8_t>& data) {
    MerkleDAG dag;
    return dag.hashString(std::string(data.begin(), data.end()));
}
std::string ColumnarArchiveFormat::sha256Pair(const std::string& a, const std::string& b) {
    MerkleDAG dag;
    return dag.hashString(a + b);
}

// ── Merkle root (binary tree, pad odd leaves by duplicating last) ─────────────
std::string ColumnarArchiveFormat::merkleRoot(const std::vector<std::string>& leaf_hashes) {
    if (leaf_hashes.empty()) return std::string(64, '0');
    if (leaf_hashes.size() == 1) return leaf_hashes[0];

    std::vector<std::string> current = leaf_hashes;
    while (current.size() > 1) {
        if (current.size() & 1) current.push_back(current.back()); // pad odd
        std::vector<std::string> next;
        next.reserve(current.size() / 2);
        for (size_t i = 0; i < current.size(); i += 2)
            next.push_back(sha256Pair(current[i], current[i+1]));
        current = std::move(next);
    }
    return current[0];
}

// ── writeHeader ───────────────────────────────────────────────────────────────
bool ColumnarArchiveFormat::writeHeader(std::ostream& out,
                                         const std::vector<ColumnSchema>& schema) {
    // Build schema JSON manually (no external dep needed for simple schema)
    std::ostringstream js;
    js << "[";
    for (size_t i = 0; i < schema.size(); ++i) {
        if (i) js << ",";
        js << "{\"name\":\"" << schema[i].name << "\","
           << "\"type\":" << static_cast<uint32_t>(schema[i].type) << "}";
    }
    js << "]";
    std::string schema_json = js.str();

    if (!write32(out, kMagic))   return false;
    if (!write32(out, kVersion)) return false;
    if (!write64(out, static_cast<uint64_t>(schema_json.size()))) return false;
    out.write(schema_json.c_str(), static_cast<std::streamsize>(schema_json.size()));
    return out.good();
}

// ── writeRowGroup ─────────────────────────────────────────────────────────────
bool ColumnarArchiveFormat::writeRowGroup(std::ostream&                  out,
                                           const std::vector<ColumnData>& columns,
                                           std::vector<std::string>&      out_chunk_hashes) {
    // Infer row_count from first column
    uint64_t row_count = 0;
    for (const auto& col : columns) {
        std::visit([&](const auto& v){ row_count = v.size(); }, col.data);
        break;
    }
    if (!write64(out, row_count)) return false;
    if (!write32(out, static_cast<uint32_t>(columns.size()))) return false;

    for (const auto& col : columns) {
        // Determine type and serialise
        uint32_t type_enum = 0;
        std::vector<uint8_t> raw_bytes;
        std::vector<std::string> str_data;  // kept for dict compression

        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                type_enum = 0; raw_bytes = serializeInt64(v);
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                type_enum = 1; raw_bytes = serializeDouble(v);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                type_enum = 2; str_data = v; raw_bytes = serializeString(v);
            } else {
                type_enum = 3; raw_bytes = serializeFloatArray(v);
            }
        }, col.data);

        uint64_t uncompressed_len = raw_bytes.size();

        // Compress based on type
        std::vector<uint8_t> compressed;
        if (type_enum == 0 || type_enum == 1) {
            compressed = rleCompress(raw_bytes);
        } else if (type_enum == 2 && !str_data.empty()) {
            compressed = dictCompress(raw_bytes, str_data);
        } else {
            compressed = raw_bytes;  // raw (FLOAT_ARRAY or empty)
        }

        // If compression didn't help, store raw
        if (compressed.size() >= raw_bytes.size()) compressed = raw_bytes;
        uint64_t compressed_len = compressed.size();

        // Hash this chunk for the Merkle tree
        out_chunk_hashes.push_back(hashBytes(compressed));

        // Write chunk header + data
        if (!write32(out, type_enum)) return false;
        if (!write64(out, uncompressed_len)) return false;
        if (!write64(out, compressed_len)) return false;
        out.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        if (!out.good()) return false;
    }
    return true;
}

// ── writeFooter ───────────────────────────────────────────────────────────────
bool ColumnarArchiveFormat::writeFooter(std::ostream&                   out,
                                         const std::vector<uint64_t>&    offsets,
                                         const std::vector<std::string>& chunk_hashes,
                                         std::string&                    out_merkle_root) {
    uint64_t footer_start = static_cast<uint64_t>(out.tellp());

    // row_group_offsets array
    if (!write32(out, static_cast<uint32_t>(offsets.size()))) return false;
    for (uint64_t off : offsets)
        if (!write64(out, off)) return false;

    // footer_offset = where footer starts
    if (!write64(out, footer_start)) return false;

    // Merkle root
    out_merkle_root = merkleRoot(chunk_hashes);
    uint64_t mr_len = out_merkle_root.size();
    if (!write64(out, mr_len)) return false;
    out.write(out_merkle_root.c_str(), static_cast<std::streamsize>(mr_len));
    return out.good();
}

// ── readFooter ────────────────────────────────────────────────────────────────
bool ColumnarArchiveFormat::readFooter(std::istream& in, Footer& out) {
    // Seek to end to find footer_offset position: last 8 bytes before Merkle root len.
    // Actually: read footer_offset from 8 bytes before end of footer.
    // Structure at footer: [uint32_t rg_count][rg_offsets...][uint64_t footer_offset][uint64_t mr_len][mr]
    // We need to find footer_offset. It's written as part of the footer itself.
    // Strategy: scan from end. Last write: mr_len(8) + mr(mr_len) + footer_start(8) before that.
    // We need to try to find it. Since mr_len is the last fixed structure before the footer ends,
    // we can: seek to end, go back 8 bytes for mr_len, read it, then go back (8 + mr_len + 8) for footer_start.

    in.seekg(0, std::ios::end);
    std::streampos file_end = in.tellg();

    // Need at least 8 (mr_len) + 64 (typical sha256 hex len) + 8 (footer_start) = 80 bytes from footer start
    // Read footer_start by scanning backward
    // footer_offset is at position: footer_start (inside footer content)
    // Since we know the merkle root is hex (64 chars for sha256):
    constexpr int64_t min_footer_size = 4 + 8 + 8 + 8 + 64; // approx
    if (static_cast<int64_t>(file_end) < min_footer_size) return false;

    // Read last 8 bytes as mr_len
    in.seekg(-8 - 8, std::ios::end);  // 8 for footer_offset placeholder, but we need mr_len first
    // Try: go 8 bytes before end to read mr_len (this is the last uint64 in the footer before mr)
    // Actually the layout is: [rg_offsets...][footer_start:8][mr_len:8][mr:mr_len]
    // The mr starts at end - mr_len; mr_len is at end - mr_len - 8.
    // We don't know mr_len yet. Try default 64 (SHA-256 hex len).
    constexpr int64_t sha256_hex_len = 64;
    if (static_cast<int64_t>(file_end) < sha256_hex_len + 8 + 8) return false;

    in.seekg(-(sha256_hex_len + 8 + 8), std::ios::end);  // skip mr + mr_len + footer_offset
    uint64_t footer_offset = 0;
    if (!read64(in, footer_offset)) return false;
    uint64_t mr_len = 0;
    if (!read64(in, mr_len)) return false;
    if (mr_len > 128) return false;  // sanity
    std::string mr(mr_len, '\0');
    in.read(mr.data(), static_cast<std::streamsize>(mr_len));

    out.footer_offset = footer_offset;
    out.merkle_root   = mr;

    // Read row group offsets from footer_start
    in.seekg(static_cast<std::streamoff>(footer_offset), std::ios::beg);
    uint32_t rg_count = 0;
    if (!read32(in, rg_count)) return false;
    out.row_group_offsets.resize(rg_count);
    for (auto& off : out.row_group_offsets)
        if (!read64(in, off)) return false;

    return true;
}

// ── readHeader ────────────────────────────────────────────────────────────────
// Read magic + version + schema JSON from an open seekable stream.
bool ColumnarArchiveFormat::readHeader(std::istream& in, std::string& out_schema_json) {
    in.seekg(0, std::ios::beg);
    uint32_t magic = 0, version = 0;
    if (!read32(in, magic)) return false;
    if (!read32(in, version)) return false;
    if (magic != kMagic || version != kVersion) return false;
    uint64_t schema_len = 0;
    if (!read64(in, schema_len)) return false;
    if (schema_len > 65536) return false;
    out_schema_json.resize(static_cast<size_t>(schema_len));
    in.read(out_schema_json.data(), static_cast<std::streamsize>(schema_len));
    return in.good();
}

// ── Decompress helpers ──────────────────────────────────────────────────────────
// Decompress RLE: each run is (value:1, count:4) → value repeated count times
static std::vector<uint8_t> rleDecompress(const std::vector<uint8_t>& compressed, uint64_t expected_uncompressed) {
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(expected_uncompressed));
    size_t i = 0;
    while (i < compressed.size()) {
        uint8_t val = compressed[i++];
        if (i + 4 > compressed.size()) break;
        uint32_t cnt = uint32_t(compressed[i]) | (uint32_t(compressed[i+1])<<8)
                     | (uint32_t(compressed[i+2])<<16) | (uint32_t(compressed[i+3])<<24);
        i += 4;
        for (uint32_t j = 0; j < cnt; ++j) out.push_back(val);
    }
    return out;
}

// Decompress dictionary: indices array → original strings via dictionary
static std::vector<std::string> dictDecompress(const std::vector<uint8_t>& compressed) {
    std::vector<std::string> out;
    size_t pos = 0;
    auto read32_at = [&]() -> uint32_t {
        if (pos + 4 > compressed.size()) return 0;
        uint32_t v = uint32_t(compressed[pos]) | (uint32_t(compressed[pos+1])<<8)
                   | (uint32_t(compressed[pos+2])<<16) | (uint32_t(compressed[pos+3])<<24);
        pos += 4;
        return v;
    };
    // Read dictionary
    uint32_t dict_count = read32_at();
    std::vector<std::string> dict;
    dict.reserve(dict_count);
    for (uint32_t d = 0; d < dict_count; ++d) {
        uint32_t slen = read32_at();
        if (pos + slen > compressed.size()) return out;
        dict.emplace_back(reinterpret_cast<const char*>(compressed.data() + pos), slen);
        pos += slen;
    }
    // Read indices
    uint32_t idx_count = read32_at();
    out.reserve(idx_count);
    for (uint32_t i = 0; i < idx_count; ++i) {
        if (pos + 2 > compressed.size()) break;
        uint16_t idx = uint16_t(compressed[pos]) | (uint16_t(compressed[pos+1])<<8);
        pos += 2;
        out.push_back(idx < dict.size() ? dict[idx] : "");
    }
    return out;
}

// ── readRowGroup ───────────────────────────────────────────────────────────────
// Read one row group at the given offset. Decompresses all column chunks.
bool ColumnarArchiveFormat::readRowGroup(std::istream& in, uint64_t offset,
                                          std::vector<ColumnData>& out_columns) {
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    uint64_t row_count = 0;
    uint32_t col_count = 0;
    if (!read64(in, row_count)) return false;
    if (!read32(in, col_count)) return false;

    out_columns.clear();
    for (uint32_t c = 0; c < col_count; ++c) {
        uint32_t type_enum = 0;
        uint64_t uncompressed_len = 0, compressed_len = 0;
        if (!read32(in, type_enum)) return false;
        if (!read64(in, uncompressed_len)) return false;
        if (!read64(in, compressed_len)) return false;

        std::vector<uint8_t> chunk_data(static_cast<size_t>(compressed_len));
        in.read(reinterpret_cast<char*>(chunk_data.data()),
                static_cast<std::streamsize>(compressed_len));
        if (!in.good()) return false;

        ColumnData col;
        col.name = std::to_string(c);  // placeholder; proper name from schema

        switch (static_cast<ColumnSchema::Type>(type_enum)) {
        case ColumnSchema::Type::INT64: {
            std::vector<uint8_t> raw = rleDecompress(chunk_data, uncompressed_len);
            std::vector<int64_t> vals(raw.size() / 8);
            for (size_t i = 0; i < vals.size(); ++i) {
                uint64_t u = 0;
                for (int k = 0; k < 8; ++k)
                    if (i*8+k < raw.size()) u |= (uint64_t(raw[i*8+k]) << (8*k));
                std::memcpy(&vals[i], &u, 8);
            }
            col.data = vals;
            break;
        }
        case ColumnSchema::Type::DOUBLE: {
            std::vector<uint8_t> raw = rleDecompress(chunk_data, uncompressed_len);
            std::vector<double> vals(raw.size() / 8);
            for (size_t i = 0; i < vals.size(); ++i) {
                uint64_t u = 0;
                for (int k = 0; k < 8; ++k)
                    if (i*8+k < raw.size()) u |= (uint64_t(raw[i*8+k]) << (8*k));
                std::memcpy(&vals[i], &u, 8);
            }
            col.data = vals;
            break;
        }
        case ColumnSchema::Type::STRING: {
            auto strs = dictDecompress(chunk_data);
            col.data = strs;
            break;
        }
        case ColumnSchema::Type::FLOAT_ARRAY: {
            // Raw format (no compression for float arrays)
            size_t pos = 0;
            auto read32_at = [&]() -> uint32_t {
                if (pos + 4 > chunk_data.size()) return 0;
                uint32_t v = uint32_t(chunk_data[pos]) | (uint32_t(chunk_data[pos+1])<<8)
                           | (uint32_t(chunk_data[pos+2])<<16) | (uint32_t(chunk_data[pos+3])<<24);
                pos += 4;
                return v;
            };
            uint32_t outer_count = read32_at();
            std::vector<std::vector<float>> arrays;
            arrays.reserve(outer_count);
            for (uint32_t a = 0; a < outer_count; ++a) {
                uint32_t inner_len = read32_at();
                std::vector<float> arr(inner_len);
                for (uint32_t f = 0; f < inner_len; ++f) {
                    if (pos + 4 > chunk_data.size()) break;
                    uint32_t u = uint32_t(chunk_data[pos]) | (uint32_t(chunk_data[pos+1])<<8)
                               | (uint32_t(chunk_data[pos+2])<<16) | (uint32_t(chunk_data[pos+3])<<24);
                    pos += 4;
                    std::memcpy(&arr[f], &u, 4);
                }
                arrays.push_back(std::move(arr));
            }
            col.data = arrays;
            break;
        }
        default:
            return false;
        }

        out_columns.push_back(std::move(col));
    }
    return true;
}

// ── verifyArchive ─────────────────────────────────────────────────────────────
bool ColumnarArchiveFormat::verifyArchive(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in) return false;

    Footer footer;
    if (!readFooter(in, footer)) return false;

    // Replay all row groups: re-read every column chunk, hash it, collect leaf hashes.
    std::vector<std::string> chunk_hashes;

    for (uint64_t rg_offset : footer.row_group_offsets) {
        in.seekg(static_cast<std::streamoff>(rg_offset), std::ios::beg);
        uint64_t row_count = 0;
        uint32_t col_count = 0;
        if (!read64(in, row_count)) return false;
        if (!read32(in, col_count)) return false;

        for (uint32_t c = 0; c < col_count; ++c) {
            uint32_t type_enum = 0;
            uint64_t uncompressed_len = 0, compressed_len = 0;
            if (!read32(in, type_enum)) return false;
            if (!read64(in, uncompressed_len)) return false;
            if (!read64(in, compressed_len)) return false;

            std::vector<uint8_t> chunk_data(compressed_len);
            in.read(reinterpret_cast<char*>(chunk_data.data()),
                    static_cast<std::streamsize>(compressed_len));
            if (!in.good()) return false;

            chunk_hashes.push_back(hashBytes(chunk_data));
        }
    }

    std::string computed_root = merkleRoot(chunk_hashes);
    return computed_root == footer.merkle_root;
}

} // namespace memory
} // namespace yuki
