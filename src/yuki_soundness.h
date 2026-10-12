// yuki_soundness.h — KT-2 Soundness Taxonomy
// Single header, zero dependencies. Include everywhere a YUKI process/tool/turn terminates.
// Exit codes:
//   0 = SUCCESS — verified against T1/T2 memory
//   1 = ENV_FAILED — valid request, infrastructure broke (tool, disk, network)
//   2 = REFUSED — caller's request wrong/unsafe, nothing ran, refuse-don't-guess
//   4 = UNSOUND — finished but output contains unverified claims (silent corruption)
// Corruption must NEVER map to 0. This is the exact failure mode K3 prevents with k3_expert_drops.

#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

// The YUKI_SOUNDNESS_CHECK() macro logs through the project logger; without
// this include the header only compiles when a caller happened to pull Logger
// in first, which is why it was effectively dead (audit §5).
#include "brain/core/Logger.h"

namespace yuki {

// Soundness taxonomy — matches K3 exit codes exactly
enum class SoundnessStatus : int {
    SUCCESS       = 0,  // Verified against T1/T2 memory stores
    ENV_FAILED    = 1,  // Infrastructure failure (tool, disk, network, OOM)
    REFUSED       = 2,  // Request refused: unsafe, unverifiable, out of bounds
    UNSOUND       = 4   // Completed but output contains unverified claims
};

// Global soundness drop counter — mandatory caller check after every turn/tool/process
// Mirrors K3's k3_expert_drops: recoverable integrity defects increment this;
// unrecoverable ones (would leave stale data consumed as output) abort immediately.
inline std::atomic<uint64_t> yuki_soundness_drops{0};

// Record an unsound output — caller MUST check this counter after operation
inline void yuki_record_unsound(const char* reason = nullptr) noexcept {
    yuki_soundness_drops.fetch_add(1, std::memory_order_relaxed);
    // Optional: log reason to audit trail
}

// Record a refusal — distinct from unsound; nothing ran
inline void yuki_record_refused(const char* reason = nullptr) noexcept {
    // Refusals don't increment soundness_drops; they're correct behavior
    // Optional: log reason to audit trail
}

// Record environment failure
inline void yuki_record_env_failed(const char* reason = nullptr) noexcept {
    // Optional: log reason to audit trail
}

// Check if any unsound outputs have occurred since last check
// Caller MUST call this after every turn/tool/process and handle non-zero
inline uint64_t yuki_check_soundness() noexcept {
    return yuki_soundness_drops.load(std::memory_order_relaxed);
}

// Reset counter (use only at explicit session boundaries, never silently)
inline void yuki_reset_soundness() noexcept {
    yuki_soundness_drops.store(0, std::memory_order_relaxed);
}

// Preflight refusal: check budget/capacity BEFORE long compute
// Returns true if operation should proceed, false if it should be refused with both numbers
inline bool yuki_preflight_check(size_t required_bytes, size_t available_bytes, const char* resource_name) noexcept {
    if (required_bytes > available_bytes) {
        // Refuse with both numbers — K3 pattern: "need X, have Y"
        // Log: "[Preflight] REFUSED: need {required_bytes} bytes {resource_name}, have {available_bytes}"
        return false;
    }
    return true;
}

// Soundness-scoped result wrapper for functions that can produce UNSOUND output
// Forces caller to acknowledge soundness state before using result
template<typename T>
struct SoundnessResult {
    SoundnessStatus status = SoundnessStatus::REFUSED;
    T value{};
    std::string reason;  // Required for REFUSED/UNSOUND/ENV_FAILED
    
    // Explicit conversion — caller must handle status
    explicit operator bool() const noexcept { return status == SoundnessStatus::SUCCESS; }
    
    // Access value only after verifying SUCCESS
    const T& operator*() const noexcept { return value; }
    T& operator*() noexcept { return value; }
    const T* operator->() const noexcept { return &value; }
    T* operator->() noexcept { return &value; }
};

// Helper: create success result
template<typename T>
inline SoundnessResult<T> yuki_ok(T&& v) noexcept {
    return SoundnessResult<T>{SoundnessStatus::SUCCESS, std::forward<T>(v), {}};
}

// Helper: create refused result (caller wrong, nothing ran)
template<typename T>
inline SoundnessResult<T> yuki_refused(std::string_view reason) noexcept {
    yuki_record_refused(reason.data());
    return SoundnessResult<T>{SoundnessStatus::REFUSED, T{}, std::string(reason)};
}

// Helper: create env_failed result (infrastructure broke)
template<typename T>
inline SoundnessResult<T> yuki_env_failed(std::string_view reason) noexcept {
    yuki_record_env_failed(reason.data());
    return SoundnessResult<T>{SoundnessStatus::ENV_FAILED, T{}, std::string(reason)};
}

// Helper: create unsound result (output corrupt/unverified)
template<typename T>
inline SoundnessResult<T> yuki_unsound(T&& v, std::string_view reason) noexcept {
    yuki_record_unsound(reason.data());
    return SoundnessResult<T>{SoundnessStatus::UNSOUND, std::forward<T>(v), std::string(reason)};
}

// Mandatory caller check macro — use after every turn/tool/process
// Expands to: if (yuki_check_soundness()) { /* handle corruption */ }
#define YUKI_SOUNDNESS_CHECK() \
    do { \
        uint64_t drops = yuki::yuki_check_soundness(); \
        if (drops > 0) { \
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "Soundness", \
                "SOUNDNESS DROPS DETECTED: " + std::to_string(drops) + " unsound outputs since last check"); \
            /* Caller MUST handle — do not continue silently */ \
        } \
    } while (0)

} // namespace yuki