#include "GridworldSimulator.h"
#include <iostream>

int main() {
    std::cout << "Test 1: Creating Task (empty)..." << std::endl;
    yuki::test::cogni1::Task task;
    std::cout << "Test 1 passed" << std::endl;
    
    std::cout << "Test 2: Creating Task with description..." << std::endl;
    task.description = "test";
    std::cout << "Test 2 passed" << std::endl;
    
    std::cout << "Test 3: Adding to target_blocks..." << std::endl;
    task.target_blocks.push_back("block_1");
    std::cout << "Test 3 passed" << std::endl;
    
    std::cout << "Test 4: Adding to metadata..." << std::endl;
    task.metadata["key"] = "value";
    std::cout << "Test 4 passed" << std::endl;
    
    std::cout << "Test 5: Adding initial_state..." << std::endl;
    task.initial_state = yuki::test::cogni1::WorldState();
    std::cout << "Test 5 passed" << std::endl;
    
    std::cout << "Test 6: Adding to goal_states..." << std::endl;
    task.goal_states.push_back(yuki::test::cogni1::WorldState());
    std::cout << "Test 6 passed" << std::endl;
    
    std::cout << "Test 7: Adding expected_answer..." << std::endl;
    task.expected_answer = "test";
    std::cout << "Test 7 passed" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}