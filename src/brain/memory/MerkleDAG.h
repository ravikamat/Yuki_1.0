#pragma once
// MerkleDAG.h — Yuki_1.0
// Standalone content-addressed DAG with SHA-256 hashing.
// No external dependencies — pure C++17.

#include <string>
#include <cstdint>
#include <vector>

class MerkleDAG {
public:
    MerkleDAG()  = default;
    ~MerkleDAG() = default;

    // Hash arbitrary data → 64-char lowercase hex SHA-256
    std::string hashString(const std::string& data) const;

    // Create a DAG node: returns SHA-256(content_hash + parent_hash)
    // Use parent_hash = std::string(64,'0') for root nodes.
    std::string createNode(const std::string& content_hash,
                           const std::string& parent_hash) const;

    // Verify: returns true iff createNode(content_hash, parent_hash) == merkle_hash
    bool verifyNode(const std::string& content_hash,
                    const std::string& parent_hash,
                    const std::string& merkle_hash) const;

    // ── Node store (T4 persistence) ──
    // Persist a node to disk: directory/<merkle_hash>.node
    bool persistNode(const std::string& directory,
                     const std::string& merkle_hash,
                     const std::string& content_hash,
                     const std::string& parent_hash,
                     uint64_t timestamp) const;

    // Load node from disk
    bool loadNode(const std::string& directory,
                  const std::string& merkle_hash,
                  std::string& out_content_hash,
                  std::string& out_parent_hash,
                  uint64_t& out_timestamp) const;

    // Traverse from a node back to genesis via parent links
    bool traceToGenesis(const std::string& directory,
                        const std::string& start_merkle_hash,
                        std::vector<std::string>& out_chain) const;
};
