// main_gate0.cpp — Entry point for Phase 0 Gate 0

#include "cogni1/Phase0_Gate0.h"
#include <iostream>

int main() {
    using namespace yuki::test::cogni1;
    
    std::cout << "Starting Gate 0..." << std::endl;
    
    Phase0Gate0::Config cfg;
    
    // Simulator config
    cfg.sim_cfg.grid_size = 5;
    cfg.sim_cfg.num_blocks = 4;
    cfg.sim_cfg.max_steps = 100;
    cfg.sim_cfg.seed = 42;
    
    // Generator config
    cfg.gen_cfg.min_blocks = 3;
    cfg.gen_cfg.max_blocks = 5;
    cfg.gen_cfg.num_tasks_per_type = 15;
    cfg.gen_cfg.seed = 123;
    
    // Oracle config
    cfg.oracle_cfg.max_nodes = 5000;
    cfg.oracle_cfg.timeout_ms = 1000.0f;
    cfg.oracle_cfg.use_astar = true;
    cfg.oracle_cfg.verbose = true;
    
    // Runner config
    cfg.runner_cfg.enabled_baselines = {
        "structured_fact_store", "dense_embedding", "random_policy", "heuristic_policy"
    };
    cfg.runner_cfg.max_steps_per_task = 100;
    cfg.runner_cfg.log_details = true;
    
    // Logger config
    cfg.logger_cfg.output_dir = "logs/cogni1";
    cfg.logger_cfg.flush_every = true;
    cfg.logger_cfg.buffer_size = 50;
    
    cfg.tasks_per_type = 3;  // Reduced for testing
    cfg.verbose = true;
    
    std::cout << "Creating Phase0Gate0..." << std::endl << std::flush;
    try {
        std::cout << "About to call Phase0Gate0 constructor..." << std::endl << std::flush;
        Phase0Gate0 gate0(cfg);
        std::cout << "Phase0Gate0 constructor returned" << std::endl << std::flush;
        std::cout << "Phase0Gate0 created, running Gate 0..." << std::endl << std::flush;
        auto result = gate0.run();
        std::cout << "Gate 0 completed: " << (result.gate_passed ? "PASSED" : "FAILED") << std::endl;
        return result.gate_passed ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }
}