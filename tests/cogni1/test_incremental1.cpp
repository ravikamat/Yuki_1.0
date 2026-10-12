#include "GridworldSimulator.h"
#include <iostream>
#include <vector>

struct Test1 {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
    std::string description;
    std::vector<std::string> target_blocks;
};

int main() {
    std::cout << "Test 1: TaskTest1..." << std::endl;
    Test1 t1;
    std::cout << "Test 1 passed" << std::endl;
    return 0;
}