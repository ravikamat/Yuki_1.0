// ArchiveWriter.h — Yuki_1.0 T4: streaming .yuk archive writer
#pragma once
#include "brain/memory/ColumnarArchiveFormat.h"
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "brain/memory/MerkleDAG.h"

namespace yuki {
namespace memory {

class ArchiveWriter {
public:
    explicit ArchiveWriter(const std::string& directory);
    ~ArchiveWriter();

    // ── Write path ──
    bool beginArchive(const std::string& name,
                      const std::vector<ColumnarArchiveFormat::ColumnSchema>& schema);
    bool writeRowGroup(const std::vector<ColumnarArchiveFormat::ColumnData>& columns);
    bool finalizeArchive(std::string& out_merkle_root);

    // ── Read path ──
    static bool verifyArchive(const std::string& filepath);
    
    // Content-addressed retrieval by Merkle root
    static bool readArchiveByMerkle(const std::string& directory,
                                    const std::string& merkle_root,
                                    std::vector<ColumnarArchiveFormat::ColumnData>& out_columns,
                                    std::string& out_schema_json);

    // Information-gain query: epochs where surprise > threshold, sorted by free_energy desc
    static bool queryBySurprise(const std::string& directory,
                                double surprise_threshold,
                                size_t max_results,
                                std::vector<std::string>& out_merkle_roots);

    // List all epochs in chronological order via Merkle-DAG parent chain
    static bool listEpochChain(const std::string& directory,
                               std::vector<std::string>& out_merkle_roots);

    // ── Merkle-DAG chain ──
    void setParentMerkleRoot(const std::string& parent_root);
    std::string getCurrentMerkleRoot() const;

private:
    std::string  directory_;
    std::string  current_file_;
    std::unique_ptr<std::ofstream> stream_;
    std::vector<uint64_t>          row_group_offsets_;
    std::vector<std::string>       chunk_hashes_;
    uint64_t                       header_end_pos_ = 0;

    std::string parent_merkle_root_;  // std::string(64,'0') for genesis
    std::string current_merkle_root_;
    MerkleDAG    merkle_dag_;
};

} // namespace memory
} // namespace yuki
