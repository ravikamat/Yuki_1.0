#pragma once
// Portable shared_mutex implementation for environments where std::shared_mutex
// is unavailable or buggy (old MSVC STL pre-19.38). Uses std::mutex +
// std::condition_variable with a correct readers-writer protocol.
//
// Writer priority: pending writers block new readers so writers are not starved.

#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace yuki {
namespace concurrency {

class shared_mutex {
public:
    shared_mutex() = default;
    ~shared_mutex() = default;
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;

    // ── Exclusive (writer) ────────────────────────────────────────────────────

    void lock() {
        std::unique_lock<std::mutex> lk(mtx_);
        // Announce intent so new readers are blocked (writer priority).
        ++pending_writers_;
        // Wait until no active readers and no other active writer.
        cv_.wait(lk, [this] { return readers_ == 0 && !writing_; });
        --pending_writers_;
        writing_ = true;
    }

    bool try_lock() {
        std::unique_lock<std::mutex> lk(mtx_, std::try_to_lock);
        if (!lk.owns_lock()) return false;
        if (readers_ > 0 || writing_) return false;
        writing_ = true;
        return true;
    }

    void unlock() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            writing_ = false;
        }
        cv_.notify_all();
    }

    // ── Shared (reader) ───────────────────────────────────────────────────────

    void lock_shared() {
        std::unique_lock<std::mutex> lk(mtx_);
        // Block while a writer is active OR writers are waiting (priority).
        cv_.wait(lk, [this] { return !writing_ && pending_writers_ == 0; });
        ++readers_;
    }

    bool try_lock_shared() {
        std::unique_lock<std::mutex> lk(mtx_, std::try_to_lock);
        if (!lk.owns_lock()) return false;
        if (writing_ || pending_writers_ > 0) return false;
        ++readers_;
        return true;
    }

    void unlock_shared() {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            notify = (--readers_ == 0);
        }
        if (notify) cv_.notify_all();
    }

    // ── RAII wrappers ─────────────────────────────────────────────────────────

    // Exclusive (write) lock guard.
    class unique_lock {
    public:
        explicit unique_lock(shared_mutex& m) : mx_(m) { mx_.lock(); }
        unique_lock(shared_mutex& m, std::try_to_lock_t) : mx_(m), owns_(mx_.try_lock()) {}
        unique_lock(shared_mutex& m, std::defer_lock_t) noexcept : mx_(m), owns_(false) {}
        ~unique_lock() { if (owns_) mx_.unlock(); }

        void lock()   { mx_.lock();   owns_ = true; }
        bool try_lock()               { return (owns_ = mx_.try_lock()); }
        void unlock() { if (owns_) { mx_.unlock(); owns_ = false; } }
        bool owns_lock() const noexcept { return owns_; }

        unique_lock(const unique_lock&) = delete;
        unique_lock& operator=(const unique_lock&) = delete;

    private:
        shared_mutex& mx_;
        bool          owns_ = true;
    };

    // Shared (read) lock guard.
    class shared_lock {
    public:
        explicit shared_lock(shared_mutex& m) : mx_(m) { mx_.lock_shared(); }
        shared_lock(shared_mutex& m, std::try_to_lock_t) : mx_(m), owns_(mx_.try_lock_shared()) {}
        shared_lock(shared_mutex& m, std::defer_lock_t) noexcept : mx_(m), owns_(false) {}
        ~shared_lock() { if (owns_) mx_.unlock_shared(); }

        void lock()   { mx_.lock_shared();   owns_ = true; }
        bool try_lock()                      { return (owns_ = mx_.try_lock_shared()); }
        void unlock() { if (owns_) { mx_.unlock_shared(); owns_ = false; } }
        bool owns_lock() const noexcept { return owns_; }

        shared_lock(const shared_lock&) = delete;
        shared_lock& operator=(const shared_lock&) = delete;

    private:
        shared_mutex& mx_;
        bool          owns_ = true;
    };

private:
    std::mutex              mtx_;
    std::condition_variable cv_;
    std::size_t             readers_         = 0;
    std::size_t             pending_writers_ = 0;
    bool                    writing_         = false;
};

} // namespace concurrency
} // namespace yuki