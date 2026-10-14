// main_cogni1.cpp — COGNI-1 Integrated System Demo
// Runs full cognitive cycle with all phases

#include "cogni1/Cogni1System.h"
#include "GridworldSimulator.h"
#include "TaskGenerator.h"
#include "MetricsLogger.h"
#include <iostream>
#include <memory>

using namespace yuki::brain::cogni1;
using namespace yuki::test::cogni1;

int main() {
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  COGNI-1 Integrated Cognitive System Demo\n";
    std::cout << "═══════════════════════════════════════════════════════════\n\n";

    // Create system for Phase 8 (full)
    auto system = Cogni1Factory::createForPhase(8);
    
    // Create simulator and task generator
    GridworldSimulator::Config sim_cfg;
    sim_cfg.grid_size = 5;
    sim_cfg.num_blocks = 3;
    sim_cfg.max_steps = 50;
    sim_cfg.seed = 42;
    
    TaskGenerator::Config gen_cfg;
    gen_cfg.min_blocks = 3;
    gen_cfg.max_blocks = 3;
    gen_cfg.num_tasks_per_type = 5;
    gen_cfg.seed = 123;
    
    GridworldSimulator sim(sim_cfg);
    TaskGenerator generator(gen_cfg);
    
    // Generate tasks
    auto tasks = generator.generateAllTasks(5);
    std::cout << "Generated " << tasks.size() << " tasks\n\n";
    
    // Metrics logger
    MetricsLogger::Config logger_cfg;
    logger_cfg.output_dir = "logs/cogni1_demo";
    logger_cfg.run_id = "demo";
    MetricsLogger logger(logger_cfg);
    logger.startRun("demo_001", 42);
    
    // Run each task through cognitive cycle
    int passed = 0;
    int total = 0;
    
    for (size_t i = 0; i < tasks.size() && i < 10; ++i) { // Limit for demo
        const auto& task = tasks[i];
        ++total;
        
        std::cout << "Task " << i << " [" << TASK_TYPE_NAMES[static_cast<int>(task.type)] << "]\n";
        
        // Reset simulator
        sim.reset(task.initial_state);
        
        // Run cognitive cycle
        auto obs = sim.getSymbolicObservation();
        auto result = system->cycle(obs, "", reasoning::ReasoningMode::PLANNING);
        
        std::cout << "  Success: " << (result.success ? "YES" : "NO") << "\n";
        std::cout << "  Confidence: " << result.confidence << "\n";
        std::cout << "  Total time: " << result.total_time.count() << " us\n";
        std::cout << "  Reasoning: " << result.reasoning_time.count() << " us\n";
        std::cout << "  Verification: " << result.verification_time.count() << " us\n";
        if (!result.verbalized_output.empty()) {
            std::cout << "  Output: " << result.verbalized_output.substr(0, 100) << "...\n";
        }
        std::cout << "\n";
        
        if (result.success) ++passed;
        
        // Log
        MetricsLogger::TaskStartEvent tse;
        tse.task_id = std::to_string(i);
        tse.task_type = TASK_TYPE_NAMES[static_cast<int>(task.type)];
        tse.phase = "demo";
        logger.log(tse);
        
        MetricsLogger::TaskEndEvent tee;
        tee.task_id = std::to_string(i);
        tee.success = result.success;
        tee.steps_taken = result.reasoning_result.plan_actions.size();
        tee.total_reward = result.success ? 1.0f : -1.0f;
        tee.wall_time_ms = result.total_time.count() / 1000.0f;
        logger.log(tee);
    }
    
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  Results: " << passed << "/" << total << " passed\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    
    // System stats
    auto stats = system->getStats();
    std::cout << "\nSystem Stats:\n";
    std::cout << "  Workspace occupancy: " << stats.workspace_occupancy << "/7\n";
    std::cout << "  Knowledge facts: " << stats.knowledge_facts << "\n";
    std::cout << "  Active rules: " << stats.active_rules << "\n";
    std::cout << "  Shadow rules: " << stats.shadow_rules << "\n";
    std::cout << "  ItemMemory roles: " << stats.item_memory_roles << "\n";
    std::cout << "  ItemMemory fillers: " << stats.item_memory_fillers << "\n";
    std::cout << "  Current tick: " << stats.current_tick << "\n";
    
    logger.flush();
    logger.endRun();
    
    return passed == total ? 0 : 1;
}