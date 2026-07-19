#include "GlobalWorkspace.h"
#include <algorithm>
#include <chrono>

using namespace yuki::gw;

GlobalWorkspace& GlobalWorkspace::instance() {
    static GlobalWorkspace gw;
    return gw;
}

void GlobalWorkspace::init(float threshold, uint32_t broadcast_interval_ms) {
    threshold_ = threshold;
    interval_ms_ = broadcast_interval_ms;
}

void GlobalWorkspace::compete(const Coalition& coalition) {
    std::lock_guard lock(buf_mtx_);
    buffer_.push_back(coalition);
}

void GlobalWorkspace::start() {
    if (running_.exchange(true)) return; // already running
    thread_ = std::thread(&GlobalWorkspace::broadcastLoop, this);
}

void GlobalWorkspace::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

Coalition GlobalWorkspace::currentWinner() const {
    std::lock_guard lock(winner_mtx_);
    return winner_;
}

void GlobalWorkspace::broadcastLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));

        std::vector<Coalition> batch;
        {
            std::lock_guard lock(buf_mtx_);
            batch = std::move(buffer_);
            buffer_.clear();
        }

        if (batch.empty()) continue;

        // Winner-take-all by salience
        auto winner_it = std::max_element(batch.begin(), batch.end(),
            [](const Coalition& a, const Coalition& b) {
                return a.salience < b.salience;
            });

        if (winner_it != batch.end() && winner_it->salience >= threshold_) {
            {
                std::lock_guard lock(winner_mtx_);
                winner_ = *winner_it;
            }
            // Broadcast winning message to CoreBus
            CoreBus::instance().publish(winner_it->message);
        }
    }
}
