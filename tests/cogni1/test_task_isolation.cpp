#include "GridworldSimulator.h"
#include <iostream>

int main() {
    std::cout << "Test 1: Creating empty Task..." << std::endl;
    yuki::test::cogni1::Task task;
    std::cout << "Test 1 passed" << std::endl;
    
    std::cout << "Test 2: Creating Task with pre-made WorldState..." << std::endl;
    yuki::test::cogni1::WorldState ws;
    yuki::test::cogni1::Task task2;
    task2.initial_state = ws;
    std::cout << "Test 2 passed" << std::endl;
    
    std::cout << "Test 3: Creating Task with goal_states..." << std::endl;
    yuki::test::cogni1::Task task3;
    task3.goal_states.push_back(ws);
    std::cout << "Test 3 passed" << std::endl;
    
    std::cout << "Test 4: Creating Task with metadata..." << std::endl;
    yuki::test::cogni1::Task task4;
    task4.metadata["key"] = "value";
    std::cout << "Test 4 passed" << std::endl;
    
    std::cout << "Test 5: Creating Task with goal_states vector reserve..." << std::endl;
    yuki::test::cogni1::Task task5;
    task5.goal_states.reserve(2);
    task5.goal_states.push_back(ws);
    task5.goal_states.push_back(ws);
    std::cout << "Test 5 passed" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}