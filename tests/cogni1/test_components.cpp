#include "GridworldSimulator.h"
#include <iostream>

int main() {
    std::cout << "Test 1: Creating WorldState..." << std::endl;
    yuki::test::cogni1::WorldState ws;
    std::cout << "Test 1 passed" << std::endl;
    
    std::cout << "Test 2: Creating Position..." << std::endl;
    yuki::test::cogni1::Position pos{1, 2};
    std::cout << "Test 2 passed" << std::endl;
    
    std::cout << "Test 3: Creating Block..." << std::endl;
    yuki::test::cogni1::Block block;
    block.name = "test";
    block.pos = pos;
    std::cout << "Test 3 passed" << std::endl;
    
    std::cout << "Test 4: Creating Gripper..." << std::endl;
    yuki::test::cogni1::Gripper gripper;
    std::cout << "Test 4 passed" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}