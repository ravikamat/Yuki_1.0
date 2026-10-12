#include "GridworldSimulator.h"
#include <iostream>

struct TaskMinimal {
    yuki::test::cogni1::TaskType type;
    yuki::test::cogni1::WorldState initial_state;
};

int main() {
    std::cout << "Test: Creating TaskMinimal..." << std::endl;
    TaskMinimal t;
    std::cout << "TaskMinimal created successfully" << std::endl;
    return 0;
}