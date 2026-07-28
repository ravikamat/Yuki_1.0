#include <iostream>
#include <cassert>
#include "src/brain/research/AlgorithmHarvestEngine.h"

int main() {
    using yuki::brain::research::AlgorithmHarvestEngine;

    AlgorithmHarvestEngine harvester;
    auto cand = harvester.harvestFromResearchOutput("Kahn's topological sort", "Efficient graph ordering using indegrees.");

    if (harvester.queuedCandidatesCount() != 1) {
        std::cerr << "[FAIL] testalgorithmharvest: expected 1 candidate queued\n";
        return 1;
    }

    std::cout << "[PASS] testalgorithmharvest\n";
    return 0;
}
