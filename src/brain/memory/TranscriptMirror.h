#pragma once
// TranscriptMirror — Append-only local transcript log with atomic writes
// Grok Bot pattern: host-event-bus.ts transcript mirror with schema versioning

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <random>

#include "brain/core/Logger.h"

namespace yuki {
namespace memory {

// Schema version for forward/backward compatibility
static constexpr uint32_t TRANSCRIPT_MIRROR_SCHEMA_VERSION = 2;

// Turn source types
enum class TurnSource : uint8_t {
    USER_TYPED       = 0,
    USER_VOICE       = 1,
    YUKI_RESPONSE    = 2,
    YUKI_PROACTIVE   = 3,
    TOOL_CALL        = 4,
    TOOL_RESULT      = 5,
    SYSTEM_EVENT     = 6
};

// Single turn entry in the transcript
struct TranscriptEntry {
    uint64_t sequence_id = 0;
    uint64_t timestamp_us = 0;
    TurnSource source = TurnSource::USER_TYPED;
    std::string text;
    std::string metadata_json; // Extensible: tool name, confidence, cognitive state, etc.

    TranscriptEntry() = default;

    TranscriptEntry(uint64_t seq, uint64_t ts, TurnSource src, std::string txt, std::string meta = "")
        : sequence_id(seq), timestamp_us(ts), source(src), text(std::move(txt)), metadata_json(std::move(meta)) {}
};

// TranscriptMirror — Thread-safe, crash-safe append-only log
class TranscriptMirror {
public:
    struct Config {
        std::filesystem::path file_path;
        size_t max_entries;
        bool flush_on_append;
        bool enable_compression;
        
        Config()
            : file_path("data/brain/transcript_mirror.json")
            , max_entries(10000)
            , flush_on_append(true)
            , enable_compression(false) {}
    };

    explicit TranscriptMirror(const Config& cfg = Config());
    ~TranscriptMirror();

    // Non-copyable, non-movable
    TranscriptMirror(const TranscriptMirror&) = delete;
    TranscriptMirror& operator=(const TranscriptMirror&) = delete;
    TranscriptMirror(TranscriptMirror&&) = delete;
    TranscriptMirror& operator=(TranscriptMirror&&) = delete;

    // Initialize: load existing transcript, validate schema
    bool initialize();

    // Shutdown: flush and close
    void shutdown();

    // Append a turn (thread-safe, atomic write)
    bool append(TurnSource source, const std::string& text, const std::string& metadata_json = "");

    // Append with full entry (for migration/replay)
    bool append_entry(const TranscriptEntry& entry);

    // Query recent entries
    std::vector<TranscriptEntry> get_recent(size_t count) const;

    // Query by time range
    std::vector<TranscriptEntry> get_range(uint64_t start_us, uint64_t end_us) const;

    // Query by source
    std::vector<TranscriptEntry> get_by_source(TurnSource source, size_t limit) const;

    // Get total entry count
    size_t total_entries() const;

    // Get stats
    struct Stats {
        uint64_t total_appended = 0;
        uint64_t total_dropped = 0;
        uint64_t last_append_time_us = 0;
        size_t current_size = 0;
        std::filesystem::path file_path;
    };
    Stats get_stats() const;

    // Force flush to disk
    bool flush();

    // Trim to max_entries (called automatically on append)
    void trim_to_limit();

private:
    Config config_;
    mutable std::mutex mutex_;
    std::vector<TranscriptEntry> entries_;
    std::atomic<uint64_t> next_sequence_id_{1};
    std::atomic<bool> initialized_{false};
    std::atomic<uint64_t> total_appended_{0};
    std::atomic<uint64_t> total_dropped_{0};
    std::atomic<uint64_t> last_append_time_us_{0};

    // Atomic write: write to temp file, fsync, rename
    bool atomic_write(const std::vector<TranscriptEntry>& data);

    // Load from file with schema validation
    bool load_from_file();

    // Validate schema version
    bool validate_schema(uint32_t found_version) const;

    // Generate temp file path
    std::filesystem::path temp_path() const;
};

} // namespace memory
} // namespace yuki