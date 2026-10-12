#include "Phase0_Gate0.h"
#include <algorithm>

namespace yuki {
namespace test {
namespace cogni1 {

Phase0Gate0::Phase0Gate0(const Config& cfg) : config_(cfg) {
    std::cout << "Phase0Gate0: Creating MetricsLogger..." << std::endl << std::flush;
    logger_ = std::make_unique<MetricsLogger>(config_.logger_cfg);
    std::cout << "Phase0Gate0: MetricsLogger created, logger_ = " << (logger_ ? "valid" : "null") << std::endl << std::flush;
    
    // Generate run ID
    std::cout << "Phase0Gate0: Getting current time..." << std::endl << std::flush;
    auto now = std::chrono::system_clock::now();
    std::cout << "Phase0Gate0: Got time, converting..." << std::endl << std::flush;
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "Phase0Gate0: time_t = " << time_t << std::endl << std::flush;
    char buf[64];
    snprintf(buf, sizeof(buf), "gate0_%lld", (long long)time_t);
    std::string run_id = buf;
    std::cout << "Phase0Gate0: run_id = " << run_id << std::endl << std::flush;
    
    std::cout << "Phase0Gate0: Calling startRun..." << std::endl << std::flush;
    logger_->startRun(run_id, config_.gen_cfg.seed);
    std::cout << "Phase0Gate0: startRun done" << std::endl << std::flush;
}

Gate0Result Phase0Gate0::run() {
    Gate0Result result;
    
    if (config_.verbose) {
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "  COGNI-1 Phase 0 - Gate 0: Oracle Validation + Baselines\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
    }
    
    // Generate tasks
    std::cout << "Creating TaskGenerator..." << std::endl << std::flush;
    TaskGenerator generator(config_.gen_cfg);
    std::cout << "TaskGenerator created" << std::endl << std::flush;
    config_.gen_cfg.num_tasks_per_type = config_.tasks_per_type;
    std::cout << "Generating tasks..." << std::endl << std::flush;
    std::vector<Task> tasks = generator.generateAllTasks(config_.tasks_per_type);
    std::cout << "Generated " << tasks.size() << " tasks" << std::endl;
    
    if (config_.verbose) {
        std::cout << "\nGenerated " << tasks.size() << " tasks:\n";
        for (int t = 0; t < 6; ++t) {
            int count = 0;
            for (const auto& task : tasks) {
                if (task.type == static_cast<TaskType>(t)) count++;
            }
            std::cout << "  " << TASK_TYPE_NAMES[t] << ": " << count << "\n";
        }
    }
    
    std::cout << "About to create Oracle" << std::endl;
    
    // Initialize oracle
    std::cout << "Creating BruteForceOracle..." << std::endl;
    std::cout << "  sim_cfg.grid_size=" << config_.sim_cfg.grid_size << std::endl;
    std::cout << "  sim_cfg.num_blocks=" << config_.sim_cfg.num_blocks << std::endl;
    std::cout << "  sim_cfg.max_steps=" << config_.sim_cfg.max_steps << std::endl;
    std::cout << "  sim_cfg.seed=" << config_.sim_cfg.seed << std::endl;
    BruteForceOracle oracle(config_.sim_cfg, config_.oracle_cfg);
    std::cout << "BruteForceOracle created!" << std::endl;
    std::cout << "About to start Oracle loop" << std::endl;
    
    // Initialize baseline runner
    std::cout << "Creating BaselineRunner..." << std::endl;
    BaselineRunner runner(config_.sim_cfg, config_.runner_cfg);
    std::cout << "BaselineRunner created!" << std::endl;
    
    // Test oracle on all tasks
    if (config_.verbose) {
        std::cout << "\n--- Running Brute-Force Oracle ---\n";
    }
    
    int oracle_solved = 0;
    float total_oracle_time = 0.0f;
    
    std::cout << "Starting Oracle loop with " << tasks.size() << " tasks" << std::endl;
    
    for (size_t i = 0; i < tasks.size(); ++i) {
        const Task& task = tasks[i];
        
        std::cout << "  Task " << i << ": Logging task start..." << std::endl;
        
        // Log task start
        TaskStartEvent tse;
        tse.task_id = std::to_string(i);
        tse.task_type = TASK_TYPE_NAMES[static_cast<int>(task.type)];
        tse.description = "";
        tse.phase = "phase_0";
        std::cout << "  Task " << i << ": Calling logger_->log..." << std::endl;
        logger_->log(tse);
        std::cout << "  Task " << i << ": logger_->log returned" << std::endl;
        std::cout << "  Task " << i << ": Task start logged, calling oracle..." << std::endl;
        
        // Run oracle
        std::cout << "  Task " << i << ": Calling oracle.solve..." << std::endl;
        auto oracle_result = oracle.solve(task);
        std::cout << "  Task " << i << ": Oracle done (found=" << oracle_result.found << ", status=" << oracle_result.status << ")" << std::endl;
        total_oracle_time += oracle_result.solve_time_ms;
        
        if (oracle_result.found) oracle_solved++;
        
        // Log oracle result
        OracleResultEvent ore;
        ore.task_id = tse.task_id;
        ore.success = oracle_result.found;
        ore.optimal_steps = static_cast<int>(oracle_result.optimal_actions.size());
        ore.optimal_actions.reserve(oracle_result.optimal_actions.size());
        for (Action a : oracle_result.optimal_actions) {
            ore.optimal_actions.push_back(ACTION_NAMES[static_cast<int>(a)]);
        }
        ore.compute_time_ms = oracle_result.solve_time_ms;
        logger_->log(ore);
        
        // Log task end
        TaskEndEvent tee;
        tee.task_id = tse.task_id;
        tee.success = oracle_result.found;
        tee.steps_taken = ore.optimal_steps;
        tee.total_reward = oracle_result.found ? 1.0f : -1.0f;
        tee.wall_time_ms = oracle_result.solve_time_ms;
        logger_->log(tee);
        
        if (config_.verbose && i < 5) {  // Print first few
            printTask(task, static_cast<int>(i));
            printOracleResult(task, oracle_result);
        }
    }
    
    result.oracle_solved = oracle_solved;
    result.oracle_total = static_cast<int>(tasks.size());
    result.oracle_avg_time_ms = total_oracle_time / tasks.size();
    result.oracle_perfect = (oracle_solved == static_cast<int>(tasks.size()));
    
    if (config_.verbose) {
        std::cout << "\nOracle Results:\n";
        std::cout << "  Solved: " << oracle_solved << "/" << tasks.size() 
                  << " (" << (100.0 * oracle_solved / tasks.size()) << "%)\n";
        std::cout << "  Avg solve time: " << result.oracle_avg_time_ms << " ms\n";
    }
    
    // Run baselines
    if (config_.verbose) {
        std::cout << "\n--- Running Baselines ---\n";
    }
    
    // Log phase transition
    PhaseEvent pe;
    pe.from_phase = 0;
    pe.to_phase = 1;
    pe.reason = "Gate 0 complete, moving to Phase 1";
    pe.phase = "phase_0";
    logger_->log(pe);
    
    auto baseline_summaries = runner.runAll(tasks, logger_.get());
    result.baseline_summaries = baseline_summaries;
    
    if (config_.verbose) {
        runner.printSummary();
    }
    
    // Gate 0 passes if oracle solves 100%
    result.gate_passed = result.oracle_perfect;
    result.notes = result.oracle_perfect 
        ? "Oracle solves 100% of tasks. Baselines recorded." 
        : "Oracle failed on some tasks - check task generation or oracle implementation";
    
    if (config_.verbose) {
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "  Gate 0: " << (result.gate_passed ? "PASSED ✓" : "FAILED ✗") << "\n";
        std::cout << "  " << result.notes << "\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
    }
    
    logger_->flush();
    logger_->endRun();
    
    return result;
}

void Phase0Gate0::printTask(const Task& task, int idx) const {
    std::cout << "\nTask " << idx << " [" << TASK_TYPE_NAMES[static_cast<int>(task.type)] << "]\n";
    std::cout << "  Initial:\n" << task.initial_state.toString() << "\n";
}

void Phase0Gate0::printOracleResult(const Task& task, const OracleSolution& sol) const {
    std::cout << "  Oracle: " << (sol.found ? "SOLVED" : "FAILED") 
              << " in " << sol.optimal_actions.size() << " steps, " 
              << sol.nodes_explored << " nodes, " << sol.solve_time_ms << "ms\n";
    if (sol.found && !sol.optimal_actions.empty()) {
        std::cout << "  Actions: ";
        for (size_t i = 0; i < sol.optimal_actions.size(); ++i) {
            std::cout << ACTION_NAMES[static_cast<int>(sol.optimal_actions[i])];
            if (i + 1 < sol.optimal_actions.size()) std::cout << " → ";
        }
        std::cout << "\n";
    }
}

void Phase0Gate0::printBaselineResults(const std::unordered_map<std::string, SimResult>& results) const {
    for (const auto& [name, result] : results) {
        std::cout << "  " << name << ": " << (result.success ? "✓" : "✗") 
                  << " (" << result.steps_taken << " steps, " << result.total_reward << " reward)\n";
    }
}

} // namespace cogni1
} // namespace test
} // namespace yuki