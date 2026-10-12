#include "GridworldSimulator.h"
#include <iostream>

int main() {
    std::cout << "Test 1: Creating Task..." << std::endl;
    yuki::test::cogni1::Task task;
    std::cout << "Test 1 passed" << std::endl;
    
    std::cout << "Test 2: Creating WorldState..." << std::endl;
    yuki::test::cogni1::WorldState ws;
    std::cout << "Test 2 passed" << std::endl;
    
    std::cout << "Test 3: Creating TaskGenerator..." << std::endl;
    yuki::test::cogni1::TaskGenerator::Config cfg;
    yuki::test::cogni1::TaskGenerator gen(cfg);
    std::cout << "Test 3 passed" << std::endl;
    
    std::cout << "Test 4: Generating 1 RETRIEVAL task..." << std::endl;
    auto tasks = gen.generateTasks(yuki::test::cogni1::TaskType::RETRIEVAL, 1);
    std::cout << "Test 4 passed, generated " << tasks.size() << " tasks" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}