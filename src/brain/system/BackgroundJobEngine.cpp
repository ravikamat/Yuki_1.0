#include "brain/system/BackgroundJobEngine.h"
#include <chrono>

namespace yuki::system {

BackgroundJobEngine::BackgroundJobEngine(size_t pool_size) {
    if (pool_size == 0) pool_size = 1;
    for (size_t i = 0; i < pool_size; ++i) {
        workers_.emplace_back(&BackgroundJobEngine::workerLoop, this);
    }
}

BackgroundJobEngine::~BackgroundJobEngine() {
    shutdown();
}

void BackgroundJobEngine::shutdown() {
    if (shutdown_requested_.exchange(true)) return;
    running_.store(false);
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
}

uint64_t BackgroundJobEngine::submitJob(Job::Type type, uint32_t priority, uint32_t timeout_ms, std::function<bool()> task) {
    uint64_t id = next_job_id_.fetch_add(1);
    Job job;
    job.id = id;
    job.type = type;
    job.priority = priority;
    job.timeout_ms = timeout_ms;
    job.status = Job::Status::PENDING;
    job.task_fn = task;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(job);
    }
    cv_.notify_one();
    return id;
}

Job::Status BackgroundJobEngine::getJobStatus(uint64_t job_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& j : history_) {
        if (j.id == job_id) return j.status;
    }
    return Job::Status::PENDING;
}

size_t BackgroundJobEngine::pendingJobCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

size_t BackgroundJobEngine::completedJobCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_count_;
}

void BackgroundJobEngine::workerLoop() {
    while (running_.load()) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                return !running_.load() || !queue_.empty();
            });
            if (!running_.load() && queue_.empty()) break;
            if (queue_.empty()) continue;

            job = queue_.top();
            queue_.pop();
        }

        job.status = Job::Status::RUNNING;
        bool ok = true;
        if (job.task_fn) {
            auto start = std::chrono::steady_clock::now();
            ok = job.task_fn();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > job.timeout_ms) {
                job.status = Job::Status::TIMEOUT;
            } else {
                job.status = ok ? Job::Status::COMPLETED : Job::Status::FAILED;
            }
        } else {
            job.status = Job::Status::COMPLETED;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            history_.push_back(job);
            if (job.status == Job::Status::COMPLETED) {
                ++completed_count_;
            }
        }
    }
}

} // namespace yuki::system
