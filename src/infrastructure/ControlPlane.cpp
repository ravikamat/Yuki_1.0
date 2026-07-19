#include "ControlPlane.h"
#include "CoreBus.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#include <chrono>
#include <string>

using namespace yuki::infra;
using yuki::gw::CoreBus;
using yuki::gw::Topic;
using yuki::gw::Message;

ControlPlane& ControlPlane::instance() {
    static ControlPlane cp;
    return cp;
}

void ControlPlane::init() {
    state_ = SystemState::BOOTING;
}

void ControlPlane::start() {
    if (running_.exchange(true)) return; // guard double-start
    monitor_thread_ = std::thread(&ControlPlane::monitorLoop, this);
}

void ControlPlane::stop() {
    running_ = false;
    if (monitor_thread_.joinable()) monitor_thread_.join();
}

void ControlPlane::transition(SystemState next) {
    SystemState prev = state_.exchange(next);
    if (prev == next) return;

    Message msg;
    msg.topic = Topic::SYSTEM_STATE;
    msg.source_module = "ControlPlane";
    msg.salience = 1.0f;
    msg.payload_json = "{\"from\":" + std::to_string(static_cast<int>(prev)) +
                       ",\"to\":"   + std::to_string(static_cast<int>(next)) + "}";
    CoreBus::instance().publish(msg);
}

bool ControlPlane::isActionAllowed(const std::string& action_type,
                                    const std::string& target) const {
    // Deny file operations targeting Windows system directories
    if (action_type == "file_delete" || action_type == "file_write") {
        if (target.find("Windows")  != std::string::npos ||
            target.find("System32") != std::string::npos) {
            return false;
        }
    }
    return true;
}

void ControlPlane::monitorLoop() {
    // Open PDH query for system-wide CPU usage
    PDH_HQUERY  cpuQuery   = nullptr;
    PDH_HCOUNTER cpuCounter = nullptr;
    bool pdh_ok = (PdhOpenQuery(nullptr, 0, &cpuQuery) == ERROR_SUCCESS);
    if (pdh_ok) {
        pdh_ok = (PdhAddCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time",
                                 0, &cpuCounter) == ERROR_SUCCESS);
        if (pdh_ok) PdhCollectQueryData(cpuQuery); // first collection sets baseline
    }

    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // --- CPU ---
        if (pdh_ok) {
            PdhCollectQueryData(cpuQuery);
            PDH_FMT_COUNTERVALUE cv;
            if (PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE,
                                            nullptr, &cv) == ERROR_SUCCESS) {
                cpu_percent_.store(static_cast<float>(cv.doubleValue));
            }
        }

        // --- Memory (this process working set) ---
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            memory_mb_.store(static_cast<size_t>(pmc.WorkingSetSize / (1024 * 1024)));
        }

        bool cpu_high = (cpu_percent_.load() > cpu_threshold_ * 100.0f);
        bool mem_high = (memory_mb_.load()   > mem_threshold_mb_);
        bool now_throttle = cpu_high || mem_high;

        // Publish alert on rising edge (stub→throttle transition)
        if (now_throttle && !throttle_.load()) {
            yuki::gw::Message msg;
            msg.topic         = yuki::gw::Topic::META_COGNITIVE;
            msg.source_module = "ControlPlane";
            msg.salience      = 0.9f;
            msg.payload_json  = "{\"alert\":\"throttle\""
                                ",\"cpu\":"    + std::to_string(cpu_percent_.load()) +
                                ",\"mem_mb\":" + std::to_string(memory_mb_.load()) + "}";
            yuki::gw::CoreBus::instance().publish(msg);
        }
        throttle_.store(now_throttle);
        ModuleRegistry::instance().heartbeat("ControlPlane");
    }

    if (pdh_ok && cpuQuery) PdhCloseQuery(cpuQuery);
}
