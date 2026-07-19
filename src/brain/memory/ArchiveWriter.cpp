// ArchiveWriter.cpp — T4 streaming archive writer implementation
#include "brain/memory/ArchiveWriter.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace yuki {
namespace memory {

ArchiveWriter::ArchiveWriter(const std::string& directory)
    : directory_(directory) {}

ArchiveWriter::~ArchiveWriter() {
    if (stream_ && stream_->is_open()) {
        std::string dummy;
        finalizeArchive(dummy);
    }
}

bool ArchiveWriter::beginArchive(
        const std::string& name,
        const std::vector<ColumnarArchiveFormat::ColumnSchema>& schema) {
    // Ensure directory exists
    try { fs::create_directories(directory_); }
    catch (const std::exception& e) {
        std::cerr << "[ArchiveWriter] Cannot create directory " << directory_
                  << ": " << e.what() << "\n";
        return false;
    }

    current_file_ = directory_ + "/" + name + ".yuk";
    stream_ = std::make_unique<std::ofstream>(current_file_, std::ios::binary | std::ios::trunc);
    if (!stream_->is_open()) {
        std::cerr << "[ArchiveWriter] Cannot open " << current_file_ << "\n";
        return false;
    }

    row_group_offsets_.clear();
    chunk_hashes_.clear();

    if (!ColumnarArchiveFormat::writeHeader(*stream_, schema)) return false;
    header_end_pos_ = static_cast<uint64_t>(stream_->tellp());
    return true;
}

bool ArchiveWriter::writeRowGroup(
        const std::vector<ColumnarArchiveFormat::ColumnData>& columns) {
    if (!stream_ || !stream_->is_open()) return false;

    uint64_t offset = static_cast<uint64_t>(stream_->tellp());
    row_group_offsets_.push_back(offset);

    return ColumnarArchiveFormat::writeRowGroup(*stream_, columns, chunk_hashes_);
}

bool ArchiveWriter::finalizeArchive(std::string& out_merkle_root) {
    if (!stream_ || !stream_->is_open()) return false;

    std::string content_hash;
    bool ok = ColumnarArchiveFormat::writeFooter(
        *stream_, row_group_offsets_, chunk_hashes_, content_hash);

    stream_->flush();
    stream_->close();
    stream_.reset();

    if (ok) {
        if (parent_merkle_root_.empty()) {
            parent_merkle_root_ = std::string(64, '0');
        }
        current_merkle_root_ = merkle_dag_.createNode(content_hash, parent_merkle_root_);
        uint64_t ts = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        merkle_dag_.persistNode(directory_, current_merkle_root_, content_hash, parent_merkle_root_, ts);
        out_merkle_root = current_merkle_root_;
        
        std::cout << "[ArchiveWriter] Finalized " << current_file_
                  << " content_hash=" << content_hash.substr(0,8) 
                  << " DAG_root=" << out_merkle_root.substr(0,16) << "...\n";
    }
    return ok;
}

void ArchiveWriter::setParentMerkleRoot(const std::string& parent_root) {
    parent_merkle_root_ = parent_root.empty() ? std::string(64, '0') : parent_root;
}

std::string ArchiveWriter::getCurrentMerkleRoot() const {
    return current_merkle_root_;
}

// ── Read path implementations ──

bool ArchiveWriter::readArchiveByMerkle(const std::string& directory,
                                        const std::string& merkle_root,
                                        std::vector<ColumnarArchiveFormat::ColumnData>& out_columns,
                                        std::string& out_schema_json) {
    // 1. Resolve DAG node to get content_hash
    MerkleDAG dag;
    std::string content_hash, parent_hash;
    uint64_t ts = 0;
    if (!dag.loadNode(directory, merkle_root, content_hash, parent_hash, ts)) {
        std::cerr << "[ArchiveWriter] Node missing for " << merkle_root << "\n";
        return false;
    }

    // 2. Find the .yuk file whose footer merkle_root matches
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() != ".yuk") continue;
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) continue;

        ColumnarArchiveFormat::Footer footer;
        if (!ColumnarArchiveFormat::readFooter(in, footer)) continue;

        if (footer.merkle_root == merkle_root) {
            // 3. Read header for schema
            if (!ColumnarArchiveFormat::readHeader(in, out_schema_json)) {
                std::cerr << "[ArchiveWriter] Failed to read header from " << entry.path() << "\n";
                return false;
            }

            // 4. Read all row groups
            out_columns.clear();
            for (uint64_t rg_offset : footer.row_group_offsets) {
                std::vector<ColumnarArchiveFormat::ColumnData> rg_columns;
                if (!ColumnarArchiveFormat::readRowGroup(in, rg_offset, rg_columns)) {
                    std::cerr << "[ArchiveWriter] Failed to read row group at offset " << rg_offset << "\n";
                    return false;
                }
                // Merge into flat column list (append)
                for (auto& col : rg_columns) {
                    out_columns.push_back(std::move(col));
                }
            }

            std::cout << "[ArchiveWriter] Read archive by Merkle root " << merkle_root.substr(0,16)
                      << " from " << entry.path() << " (" << footer.row_group_offsets.size()
                      << " row groups, " << out_columns.size() << " columns)\n";
            return true;
        }
    }

    std::cerr << "[ArchiveWriter] No .yuk file found with Merkle root " << merkle_root << "\n";
    return false;
}

bool ArchiveWriter::queryBySurprise(const std::string& directory,
                                    double surprise_threshold,
                                    size_t max_results,
                                    std::vector<std::string>& out_merkle_roots) {
    out_merkle_roots.clear();

    // Collect all .yuk files with their footer merkle roots and surprise values
    std::vector<std::pair<double, std::string>> surprise_roots;  // (surprise, merkle_root)

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() != ".yuk") continue;
        if (out_merkle_roots.size() >= max_results) break;

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) continue;

        ColumnarArchiveFormat::Footer footer;
        if (!ColumnarArchiveFormat::readFooter(in, footer)) continue;

        // Read row groups looking for the "surprise" column
        // Surprise is column index 4 in the schema: [id, ts, slot, fe, surprise]
        double total_surprise = 0.0;
        size_t surprise_count = 0;

        for (uint64_t rg_offset : footer.row_group_offsets) {
            std::vector<ColumnarArchiveFormat::ColumnData> rg_columns;
            if (!ColumnarArchiveFormat::readRowGroup(in, rg_offset, rg_columns)) continue;

            // Column 4 (0-indexed) = surprise
            if (rg_columns.size() > 4) {
                const auto& col = rg_columns[4];
                if (std::holds_alternative<std::vector<double>>(col.data)) {
                    const auto& vals = std::get<std::vector<double>>(col.data);
                    for (double v : vals) {
                        total_surprise += v;
                        surprise_count++;
                    }
                }
            }
        }

        if (surprise_count > 0) {
            double avg_surprise = total_surprise / static_cast<double>(surprise_count);
            if (avg_surprise >= surprise_threshold) {
                surprise_roots.emplace_back(avg_surprise, footer.merkle_root);
            }
        }
    }

    // Sort by surprise descending
    std::sort(surprise_roots.begin(), surprise_roots.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < std::min(max_results, surprise_roots.size()); ++i) {
        out_merkle_roots.push_back(surprise_roots[i].second);
    }

    std::cout << "[ArchiveWriter] queryBySurprise: " << out_merkle_roots.size()
              << " epochs above threshold " << surprise_threshold << "\n";
    return !out_merkle_roots.empty();
}

bool ArchiveWriter::listEpochChain(const std::string& directory,
                                   std::vector<std::string>& out_merkle_roots) {
    out_merkle_roots.clear();

    // Scan .node files to find the head (highest timestamp)
    std::string head_hash;
    uint64_t head_ts = 0;
    MerkleDAG dag;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() != ".node") continue;
        std::string fname = entry.path().stem().string();  // merkle_hash.node
        std::string content, parent;
        uint64_t ts = 0;
        if (dag.loadNode(directory, fname, content, parent, ts)) {
            if (ts > head_ts) {
                head_ts = ts;
                head_hash = fname;
            }
        }
    }

    if (head_hash.empty()) {
        std::cerr << "[ArchiveWriter] No .node files found in " << directory << "\n";
        return false;
    }

    // Trace from head to genesis
    if (!dag.traceToGenesis(directory, head_hash, out_merkle_roots)) {
        std::cerr << "[ArchiveWriter] Chain traversal failed starting from " << head_hash << "\n";
        return false;
    }

    // out_merkle_roots is currently [head → ... → genesis], reverse to chronological
    std::reverse(out_merkle_roots.begin(), out_merkle_roots.end());

    std::cout << "[ArchiveWriter] listEpochChain: " << out_merkle_roots.size()
              << " epochs (head=" << head_hash.substr(0,16) << "...)\n";
    return true;
}



bool ArchiveWriter::verifyArchive(const std::string& filepath) {
    return ColumnarArchiveFormat::verifyArchive(filepath);
}

} // namespace memory
} // namespace yuki
