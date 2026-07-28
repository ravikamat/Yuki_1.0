#include "brain/sleep/SleepThread.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/causality/CausalGraph.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] Enhanced CounterfactualReplayEngine..." << std::endl;

    yuki::memory::EpisodicStore episodic("test_cf_episodic.db");
    yuki::inference::VariationalStateEstimator vse;
    yuki::brain::sleep::CounterfactualReplayEngine replay(episodic, vse);

    yuki::causality::CausalGraph causal_graph;
    causal_graph.addNode("X");
    causal_graph.addNode("Y");
    causal_graph.addEdge(0, 1);

    replay.setCausalGraph(&causal_graph);
    assert(replay.causalGraph() == &causal_graph);

    size_t count = replay.generateCounterfactuals(5, 86400000);
    std::cout << "[TEST] Enhanced CounterfactualReplayEngine PASSED (generated=" << count << ")!" << std::endl;

    return 0;
}
