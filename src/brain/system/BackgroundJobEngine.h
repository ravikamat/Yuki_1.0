#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>

namespace yuki::system {

struct Job {
    uint64_t id = 0;
    enum class Type : uint8_t { RESEARCH, CONSOLIDATION, SYSTEM_MONITOR, CURRICULUM, WEB_SCRAPE };
    Type type = Type::RESEARCH;
    std::vector<uint8_t> payload;
    uint32_t priority = 10; // lower = higher priority
    uint32_t timeout_ms = 5000;
    enum class Status : uint8_t { PENDING, RUNNING, COMPLETED, FAILED, TIMEOUT };
    Status status = Status::PENDING;
    std::function<bool()> task_fn;
};

class BackgroundJobEngine {
public:
    explicit BackgroundJobEngine(size_t pool_size = 2);
    ~BackgroundJobEngine();

    uint64_t submitJob(Job::Type type, uint32_t priority, uint32_t timeout_ms, std::function<bool()> task = nullptr);
    Job::Status getJobStatus(uint64_t job_id) const;
    size_t pendingJobCount() const;
    size_t completedJobCount() const;
    void shutdown();

private:
    struct JobComparator {
        bool operator()(const Job& a, const Job& b) const {
            return a.priority > b.priority; // min-heap by priority
        }
    };

    std::priority_queue<Job, std::vector<Job>, JobComparator> queue_;
    std::vector<Job> history_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{true};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<uint64_t> next_job_id_{1};
    size_t completed_count_{0};

    void workerLoop();
};

} // namespace yuki::system
