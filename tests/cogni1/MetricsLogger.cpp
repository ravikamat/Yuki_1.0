#include "MetricsLogger.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace yuki {
namespace test {
namespace cogni1 {

MetricsLogger::MetricsLogger(const Config& cfg) : config_(cfg) {
    std::filesystem::create_directories(config_.output_dir);
}

MetricsLogger::~MetricsLogger() {
    flush();
    if (file_.is_open()) file_.close();
}

void MetricsLogger::startRun(const std::string& run_id, int seed) {
    std::cout << "[MetricsLogger] startRun: acquiring lock..." << std::endl << std::flush;
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[MetricsLogger] startRun: lock acquired" << std::endl << std::flush;
    flushUnlocked();
    std::cout << "[MetricsLogger] startRun: flush done" << std::endl << std::flush;
    if (file_.is_open()) file_.close();
    
    current_run_id_ = run_id.empty() ? 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) : run_id;
    current_seed_ = seed;
    event_counter_ = 0;
    buffer_.clear();
    buffer_.reserve(config_.buffer_size);
    
    std::cout << "[MetricsLogger] startRun: calling makeFileName..." << std::endl << std::flush;
    std::string filename = makeFileName();
    std::cout << "[MetricsLogger] startRun: filename = " << filename << std::endl << std::flush;
    std::cout << "[MetricsLogger] startRun: opening file..." << std::endl << std::flush;
    file_.open(filename, std::ios::out | std::ios::app);
    std::cout << "[MetricsLogger] startRun: file opened: " << file_.is_open() << std::endl << std::flush;
}

void MetricsLogger::endRun() {
    flush();
    if (file_.is_open()) file_.close();
    current_run_id_.clear();
}

void MetricsLogger::log(const MetricEvent& event) {
    json j = event.toJson();
    j["event_id"] = ++event_counter_;
    j["run_id"] = current_run_id_;
    j["seed"] = current_seed_;
    writeEvent(j);
}

void MetricsLogger::log(const TaskStartEvent& event) {
    std::cout << "[MetricsLogger] log(TaskStartEvent) start" << std::endl << std::flush;
    TaskStartEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    std::cout << "[MetricsLogger] calling toJson" << std::endl << std::flush;
    json j = e.toJson();
    std::cout << "[MetricsLogger] toJson done, calling writeEvent" << std::endl << std::flush;
    writeEvent(j);
    std::cout << "[MetricsLogger] writeEvent done" << std::endl << std::flush;
}

void MetricsLogger::log(const TaskEndEvent& event) {
    TaskEndEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const ActionEvent& event) {
    ActionEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const ReasoningEvent& event) {
    ReasoningEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const VerifierEvent& event) {
    VerifierEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const BaselineResultEvent& event) {
    BaselineResultEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const OracleResultEvent& event) {
    OracleResultEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const GateEvent& event) {
    GateEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::log(const PhaseEvent& event) {
    PhaseEvent e = event;
    e.event_id = ++event_counter_;
    e.run_id = current_run_id_;
    e.seed = current_seed_;
    writeEvent(e.toJson());
}

void MetricsLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    flushUnlocked();
}

void MetricsLogger::flushUnlocked() {
    for (const auto& j : buffer_) {
        if (file_.is_open()) {
            file_ << j.dump() << '\n';
        }
    }
    buffer_.clear();
    if (file_.is_open()) file_.flush();
}

void MetricsLogger::writeEvent(const json& j) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(j);
    }
    if (config_.flush_every || buffer_.size() >= config_.buffer_size) {
        flush();
    }
}

std::string MetricsLogger::makeFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s/metrics_%lld_%s.jsonl", 
             config_.output_dir.c_str(), (long long)time_t, current_run_id_.c_str());
    return std::string(buf);
}

} // namespace cogni1
} // namespace test
} // namespace yuki