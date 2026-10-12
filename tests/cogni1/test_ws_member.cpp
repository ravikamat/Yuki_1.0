#include "GridworldSimulator.h"
#include <iostream>

struct TestWithWorldState {
    yuki::test::cogni1::WorldState ws;
    int x = 0;
};

int main() {
    std::cout << "Test: Creating struct with WorldState..." << std::endl;
    TestWithWorldState t;
    std::cout << "Struct with WorldState created successfully" << std::endl;
    return 0;
}