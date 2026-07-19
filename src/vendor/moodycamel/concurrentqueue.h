// =============================================================================
// yuki/vendor/moodycamel/concurrentqueue.h
// Minimal stand-in for moodycamel::ConcurrentQueue.
// Implements the same enqueue / try_dequeue interface using a mutex-protected
// std::queue so that the rest of the engine compiles and tests correctly.
// Replace with the real single-header library from
//   https://github.com/cameron314/concurrentqueue
// for production lock-free performance.
// =============================================================================

#pragma once

#include <mutex>
#include <queue>

namespace moodycamel {

template<typename T>
class ConcurrentQueue {
public:
    ConcurrentQueue() = default;

    // Enqueue a copy.  Always succeeds.  Thread-safe.
    bool enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        q_.push(item);
        return true;
    }

    // Move-enqueue.  Thread-safe.
    bool enqueue(T&& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        q_.push(std::move(item));
        return true;
    }

    // Non-blocking dequeue.  Returns false if queue is empty.  Thread-safe.
    bool try_dequeue(T& out) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }

    // Approximate size (may be stale by the time the caller uses it).
    std::size_t size_approx() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return q_.size();
    }

private:
    mutable std::mutex mtx_;
    std::queue<T> q_;
};

} // namespace moodycamel
