#pragma once
// Phase0_Gate0.cpp — Phase 0 Gate 0: Oracle validation + Yuki baseline
// Runs brute-force oracle on all tasks, records Yuki baseline performance

#include "GridworldSimulator.h"
#include "BruteForceOracle.h"
#include "BaselineRunner.h"
#include "MetricsLogger.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

namespace yuki {
namespace test {
namespace cogni1 {

struct Gate0Result {
    bool oracle_perfect = false;
    int oracle_solved = 0;
    int oracle_total = 0;
    float oracle_avg_time_ms = 0.0f;
    
    std::unordered_map<std::string, BaselineSummary> baseline_summaries;
    bool gate_passed = false;
    std::string notes;
};

class Phase0Gate0 {
public:
    struct Config {
        GridworldSimulator::Config sim_cfg;
        TaskGenerator::Config gen_cfg;
        BruteForceOracle::Config oracle_cfg;
        BaselineRunner::Config runner_cfg;
        MetricsLogger::Config logger_cfg;
        int tasks_per_type;
        bool verbose;
        
        Config() : sim_cfg(), gen_cfg(), oracle_cfg(), runner_cfg(), logger_cfg(),
                   tasks_per_type(20), verbose(true) {}
    };
    
    explicit Phase0Gate0(const Config& cfg = Config());
    
    Gate0Result run();
    
private:
    Config config_;
    std::unique_ptr<MetricsLogger> logger_;
    
    void printTask(const Task& task, int idx) const;
    void printOracleResult(const Task& task, const OracleSolution& sol) const;
    void printBaselineResults(const std::unordered_map<std::string, SimResult>& results) const;
};

} // namespace cogni1
} // namespace test
} // namespace yuki