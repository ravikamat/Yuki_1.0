// TranscriptMirror.cpp — Append-only local transcript log with atomic writes

#include "TranscriptMirror.h"
#include "brain/core/Logger.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <random>
#include <thread>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/file.h>
#endif

namespace yuki {
namespace memory {

TranscriptMirror::TranscriptMirror(const Config& cfg)
    : config_(cfg) {
}

TranscriptMirror::~TranscriptMirror() {
    shutdown();
}

bool TranscriptMirror::initialize() {
    if (initialized_.exchange(true)) {
        return true;
    }

    // Ensure directory exists
    std::filesystem::create_directories(config_.file_path.parent_path());

    // Load existing transcript
    if (!load_from_file()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TranscriptMirror",
            "No existing transcript file or empty, starting fresh");
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TranscriptMirror",
        "Initialized: path=" + config_.file_path.string() +
        ", max_entries=" + std::to_string(config_.max_entries) +
        ", loaded_entries=" + std::to_string(entries_.size()));

    return true;
}

void TranscriptMirror::shutdown() {
    if (!initialized_.exchange(false)) {
        return;
    }

    flush();
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TranscriptMirror",
        "Shutdown complete");
}

bool TranscriptMirror::append(TurnSource source, const std::string& text, const std::string& metadata_json) {
    if (!initialized_.load()) return false;

    uint64_t seq = next_sequence_id_.fetch_add(1, std::memory_order_relaxed);
    uint64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    TranscriptEntry entry(seq, ts, source, text, metadata_json);

    return append_entry(entry);
}

bool TranscriptMirror::append_entry(const TranscriptEntry& entry) {
    if (!initialized_.load()) return false;

    std::lock_guard<std::mutex> lock(mutex_);

    entries_.push_back(entry);
    total_appended_.fetch_add(1, std::memory_order_relaxed);
    last_append_time_us_.store(entry.timestamp_us, std::memory_order_relaxed);

    // Trim if over limit
    if (entries_.size() > config_.max_entries) {
        size_t to_remove = entries_.size() - config_.max_entries;
        entries_.erase(entries_.begin(), entries_.begin() + to_remove);
        total_dropped_.fetch_add(to_remove, std::memory_order_relaxed);
    }

    // Atomic write if enabled
    if (config_.flush_on_append) {
        if (!atomic_write(entries_)) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
                "Atomic write failed for entry " + std::to_string(entry.sequence_id));
            return false;
        }
    }

    return true;
}

std::vector<TranscriptEntry> TranscriptMirror::get_recent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.empty()) return {};

    size_t start = (entries_.size() > count) ? entries_.size() - count : 0;
    return std::vector<TranscriptEntry>(entries_.begin() + start, entries_.end());
}

std::vector<TranscriptEntry> TranscriptMirror::get_range(uint64_t start_us, uint64_t end_us) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TranscriptEntry> result;

    auto it_start = std::lower_bound(entries_.begin(), entries_.end(), start_us,
        [](const TranscriptEntry& e, uint64_t ts) { return e.timestamp_us < ts; });
    auto it_end = std::upper_bound(entries_.begin(), entries_.end(), end_us,
        [](uint64_t ts, const TranscriptEntry& e) { return ts < e.timestamp_us; });

    result.assign(it_start, it_end);
    return result;
}

std::vector<TranscriptEntry> TranscriptMirror::get_by_source(TurnSource source, size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TranscriptEntry> result;

    for (auto it = entries_.rbegin(); it != entries_.rend() && result.size() < limit; ++it) {
        if (it->source == source) {
            result.push_back(*it);
        }
    }

    return result;
}

size_t TranscriptMirror::total_entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

TranscriptMirror::Stats TranscriptMirror::get_stats() const {
    Stats s;
    s.total_appended = total_appended_.load(std::memory_order_relaxed);
    s.total_dropped = total_dropped_.load(std::memory_order_relaxed);
    s.last_append_time_us = last_append_time_us_.load(std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        s.current_size = entries_.size();
    }
    s.file_path = config_.file_path;
    return s;
}

bool TranscriptMirror::flush() {
    if (!initialized_.load()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    return atomic_write(entries_);
}

void TranscriptMirror::trim_to_limit() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.size() > config_.max_entries) {
        size_t to_remove = entries_.size() - config_.max_entries;
        entries_.erase(entries_.begin(), entries_.begin() + to_remove);
        total_dropped_.fetch_add(to_remove, std::memory_order_relaxed);
    }
}

bool TranscriptMirror::atomic_write(const std::vector<TranscriptEntry>& data) {
    namespace fs = std::filesystem;

    // Serialize to JSON
    nlohmann::json j;
    j["schemaVersion"] = TRANSCRIPT_MIRROR_SCHEMA_VERSION;
    j["entries"] = nlohmann::json::array();

    for (const auto& entry : data) {
        nlohmann::json e;
        e["seq"] = entry.sequence_id;
        e["ts"] = entry.timestamp_us;
        e["src"] = static_cast<uint8_t>(entry.source);
        e["text"] = entry.text;
        if (!entry.metadata_json.empty()) {
            try {
                e["meta"] = nlohmann::json::parse(entry.metadata_json);
            } catch (...) {
                e["meta_raw"] = entry.metadata_json;
            }
        }
        j["entries"].push_back(std::move(e));
    }

    // Write to temp file
    auto tmp = temp_path();
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Failed to open temp file: " + tmp.string());
        return false;
    }

    ofs << j.dump();
    if (!ofs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Failed to write temp file");
        return false;
    }

    ofs.flush();
    if (!ofs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Failed to flush temp file");
        return false;
    }

    // fsync for durability
    ofs.close();

    // Platform-specific fsync
    int fd = -1;
#ifdef _WIN32
    // Windows: use _commit on file descriptor
    fd = _open(tmp.string().c_str(), _O_RDONLY | _O_BINARY);
    if (fd >= 0) {
        _commit(fd);
        _close(fd);
    }
#else
    // POSIX: fsync
    fd = open(tmp.string().c_str(), O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
#endif

    // Atomic rename
    try {
        fs::rename(tmp, config_.file_path);
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Atomic rename failed: " + std::string(e.what()));
        return false;
    }

    return true;
}

bool TranscriptMirror::load_from_file() {
    namespace fs = std::filesystem;

    if (!fs::exists(config_.file_path)) {
        return true; // No file yet, that's OK
    }

    std::ifstream ifs(config_.file_path);
    if (!ifs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Failed to open transcript file: " + config_.file_path.string());
        return false;
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "JSON parse error: " + std::string(e.what()));
        return false;
    }

    // Validate schema version
    if (!j.contains("schemaVersion") || !j["schemaVersion"].is_number_unsigned()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Missing or invalid schemaVersion");
        return false;
    }

    uint32_t found_version = j["schemaVersion"].get<uint32_t>();
    if (!validate_schema(found_version)) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Schema version mismatch: expected " + std::to_string(TRANSCRIPT_MIRROR_SCHEMA_VERSION) +
            ", found " + std::to_string(found_version));
        return false;
    }

    // Load entries
    if (!j.contains("entries") || !j["entries"].is_array()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TranscriptMirror",
            "Missing or invalid entries array");
        return false;
    }

    entries_.clear();
    uint64_t max_seq = 0;

    for (const auto& e : j["entries"]) {
        try {
            TranscriptEntry entry;
            entry.sequence_id = e.value("seq", 0);
            entry.timestamp_us = e.value("ts", 0);
            entry.source = static_cast<TurnSource>(e.value("src", 0));
            entry.text = e.value("text", "");

            if (e.contains("meta")) {
                entry.metadata_json = e["meta"].dump();
            } else if (e.contains("meta_raw")) {
                entry.metadata_json = e["meta_raw"].get<std::string>();
            }

            if (entry.sequence_id > max_seq) {
                max_seq = entry.sequence_id;
            }

            entries_.push_back(std::move(entry));
        } catch (const std::exception& e) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "TranscriptMirror",
                "Skipping malformed entry: " + std::string(e.what()));
        }
    }

    // Update sequence counter
    next_sequence_id_.store(max_seq + 1, std::memory_order_relaxed);

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TranscriptMirror",
        "Loaded " + std::to_string(entries_.size()) + " entries from " + config_.file_path.string());

    return true;
}

bool TranscriptMirror::validate_schema(uint32_t found_version) const {
    // Strict: refuse unknown versions (K3 discipline: refuse-don't-guess)
    return found_version == TRANSCRIPT_MIRROR_SCHEMA_VERSION;
}

std::filesystem::path TranscriptMirror::temp_path() const {
    namespace fs = std::filesystem;
    auto parent = config_.file_path.parent_path();
    auto stem = config_.file_path.stem().string();

    // Generate unique temp name: .stem.pid.timestamp.tmp
    static std::atomic<uint64_t> counter{0};
    uint64_t seq = counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::string tmp_name = "." + stem + "." +
        std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "." +
        std::to_string(ts) + "." + std::to_string(seq) + ".tmp";

    return parent / tmp_name;
}

} // namespace memory
} // namespace yuki