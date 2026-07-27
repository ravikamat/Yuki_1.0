#include "brain/creativity/CreativeSearch.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::creativity;

    std::cout << "[TEST] CreativeSearch starting..." << std::endl;

    CreativeSearch searcher(4);
    assert(searcher.getEmbeddingDim() == 4);

    std::vector<std::vector<double>> known = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0}
    };
    searcher.setKnownConcepts(known);

    std::vector<double> goal = {0.5, 0.5, 0.5, 0.5};
    searcher.setGoalVector(goal);

    std::vector<double> start = {0.1, 0.1, 0.1, 0.1};

    // Test DIVERGENT search
    SearchResult resDiv = searcher.search(start, SearchMode::DIVERGENT, 3);
    assert(resDiv.conceptVector.size() == 4);
    assert(resDiv.iterations > 0);

    // Test CONVERGENT search
    SearchResult resConv = searcher.search(start, SearchMode::CONVERGENT, 3);
    assert(resConv.conceptVector.size() == 4);
    assert(resConv.iterations > 0);

    // Evaluate functions
    double nov = searcher.evaluateNovelty(start);
    double util = searcher.evaluateUtility(start);
    double coh = searcher.evaluateCoherence(start);
    double val = searcher.evaluateValue(start);

    assert(nov >= 0.0);
    assert(util >= 0.0);
    assert(coh >= 0.0);
    assert(val >= 0.0);

    // Test serialization
    auto bytes = searcher.serialize();
    assert(!bytes.empty());

    CreativeSearch searcher2(4);
    bool ok = searcher2.deserialize(bytes);
    assert(ok);
    assert(searcher2.getEmbeddingDim() == 4);

    std::cout << "[TEST] CreativeSearch PASSED!" << std::endl;
    return 0;
}
