#include "GridworldSimulator.h"
#include <iostream>
#include <string>

struct Test2Simple {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
    std::string description;
    std::string query;
};

int main() {
    std::cout << "Test 2Simple: TaskTest2Simple..." << std::endl;
    Test2Simple t2;
    std::cout << "Test 2Simple passed" << std::endl;
    return 0;
}