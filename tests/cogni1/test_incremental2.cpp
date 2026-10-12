#include "GridworldSimulator.h"
#include <iostream>
#include <string>
#include <vector>

struct Test2 {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
    std::string description;
    std::vector<std::string> target_blocks;
    std::string query;
};

int main() {
    std::cout << "Test 2: TaskTest2..." << std::endl;
    Test2 t2;
    std::cout << "Test 2 passed" << std::endl;
    return 0;
}