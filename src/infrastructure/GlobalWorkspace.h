#pragma once
#include "CoreBus.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

namespace yuki::gw {

struct Coalition {
    std::string module_id;
    Topic topic = Topic::NONE;
    float salience = 0.0f;
    Message message;
};

class GlobalWorkspace {
public:
    static GlobalWorkspace& instance();

    void init(float threshold = 0.25f, uint32_t broadcast_interval_ms = 10);

    // Modules submit salience-competing messages here instead of direct CoreBus publish
    void compete(const Coalition& coalition);

    // Background thread: every 10ms picks winner and broadcasts to CoreBus
    void start();
    void stop();

    // Current winning coalition (for introspection)
    Coalition currentWinner() const;

private:
    GlobalWorkspace() = default;
    void broadcastLoop();

    std::vector<Coalition> buffer_;
    mutable std::mutex buf_mtx_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    float threshold_ = 0.25f;
    uint32_t interval_ms_ = 10;
    Coalition winner_;
    mutable std::mutex winner_mtx_;
};

} // namespace yuki::gw
