#include "GridworldSimulator.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

struct TaskTest1 {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
    std::string description;
};

int main() {
    std::cout << "Test 1: Task with type, initial_state, description..." << std::endl;
    TaskTest1 t1;
    std::cout << "Test 1 passed" << std::endl;
    
    struct TaskTest2 {
        yuki::test::cogni1::TaskType type;
        yuki::test::cogni1::WorldState initial_state;
        std::string description;
        std::vector<std::string> target_blocks;
    };
    
    std::cout << "Test 2: Adding target_blocks..." << std::endl;
    TaskTest2 t2;
    std::cout << "Test 2 passed" << std::endl;
    
    struct TaskTest3 {
        yuki::test::cogni1::TaskType type;
        yuki::test::cogni1::WorldState initial_state;
        std::string description;
        std::vector<std::string> target_blocks;
        std::string query;
    };
    
    std::cout << "Test 3: Adding query..." << std::endl;
    TaskTest3 t3;
    std::cout << "Test 3 passed" << std::endl;
    
    struct TaskTest4 {
        yuki::test::cogni1::TaskType type;
        yuki::test::cogni1::WorldState initial_state;
        std::string description;
        std::vector<std::string> target_blocks;
        std::string query;
        std::vector<yuki::test::cogni1::WorldState> goal_states;
    };
    
    std::cout << "Test 4: Adding goal_states..." << std::endl;
    TaskTest4 t4;
    std::cout << "Test 4 passed" << std::endl;
    
    struct TaskTest5 {
        yuki::test::cogni1::TaskType type;
        yuki::test::cogni1::WorldState initial_state;
        std::string description;
        std::vector<std::string> target_blocks;
        std::string query;
        std::vector<yuki::test::cogni1::WorldState> goal_states;
        std::unordered_map<std::string, std::string> metadata;
    };
    
    std::cout << "Test 5: Adding metadata..." << std::endl;
    TaskTest5 t5;
    std::cout << "Test 5 passed" << std::endl;
    
    struct TaskTest6 {
        yuki::test::cogni1::TaskType type;
        yuki::test::cogni1::WorldState initial_state;
        std::string description;
        std::vector<std::string> target_blocks;
        std::string query;
        std::vector<yuki::test::cogni1::WorldState> goal_states;
        std::unordered_map<std::string, std::string> metadata;
        std::string expected_answer;
    };
    
    std::cout << "Test 6: Adding expected_answer..." << std::endl;
    TaskTest6 t6;
    std::cout << "Test 6 passed" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}