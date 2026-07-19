#pragma once
#include <thread>
#include <atomic>

class RuntimeWorkerBase {
protected:
    std::thread worker_;
    std::atomic<bool> stop_{false};

public:
    virtual ~RuntimeWorkerBase() {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }
};
