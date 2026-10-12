#include "GridworldSimulator.h"
#include <iostream>
#include <string>
#include <vector>

struct TestWithVector {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
    std::vector<std::string> target_blocks;
};

int main() {
    std::cout << "Test With Vector..." << std::endl;
    TestWithVector t;
    std::cout << "Test With Vector passed" << std::endl;
    return 0;
}