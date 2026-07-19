#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <mutex>
#include <sqlite3.h>

namespace yuki::memory {

class ProceduralStore {
public:
    enum class BlobType { DMC_WEIGHTS, SKILL_COMPILED, SESSION_CHECKPOINT, GENERATIVE_MODEL };
    static std::string typeToString(BlobType t);
    static BlobType stringToType(const std::string& s);

    ProceduralStore();
    ~ProceduralStore();

    bool init(const std::string& dbPath, const std::string& fsPath);
    void close();

    // Store: UPSERT semantics. Returns false on failure.
    bool store(const std::string& id, BlobType type, const std::vector<uint8_t>& blob);

    // Retrieve: returns empty if not found or hash mismatch (corruption detected).
    std::optional<std::vector<uint8_t>> retrieve(const std::string& id);

    std::vector<std::string> list(BlobType type) const;
    bool erase(const std::string& id);
    bool exists(const std::string& id) const;

    struct Metadata {
        std::string id;
        BlobType type;
        std::string hash;
        int64_t created_ts;
        int access_count;
        size_t size_bytes;
    };
    std::optional<Metadata> getMetadata(const std::string& id) const;

    size_t totalBlobCount() const;
    size_t totalByteSize() const;
    bool verifyIntegrity(const std::string& id) const;

private:
    sqlite3* db_ = nullptr;
    std::string db_path_;
    std::string fs_path_;
    mutable std::mutex mu_;

    bool ensureSchema();
    std::string computeHash(const std::vector<uint8_t>& data) const;

    std::optional<std::vector<uint8_t>> retrieveInternal(const std::string& id) const;

    // Filesystem fallback for blobs > 64KB
    bool writeFsBlob(const std::string& id, const std::vector<uint8_t>& data);
    std::optional<std::vector<uint8_t>> readFsBlob(const std::string& id) const;
    bool deleteFsBlob(const std::string& id);

    static constexpr int MAX_RETRIES = 3;
    static constexpr int RETRY_MS = 10;
};

} // namespace yuki::memory
