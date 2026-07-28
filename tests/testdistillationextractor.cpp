#include <iostream>
#include <cassert>
#include "src/brain/language/DistillationExtractor.h"
#include "src/brain/memory/MemoryFabric.h"

int main() {
    using yuki::memory::MemoryFabric;
    using yuki::brain::language::DistillationExtractor;
    using yuki::brain::learning::LearningEpisode;

    MemoryFabric memoryFabric;
    LearningEpisode ep;
    ep.episodeId = "ep_001";
    ep.userInput = "Hello YUKI";
    ep.finalOutput = "Hello Owner";
    ep.acceptedByOwner = true;
    ep.safe = true;

    memoryFabric.storeLearningEpisode(ep);

    DistillationExtractor extractor(memoryFabric);
    std::size_t exported = extractor.exportEligibleEpisodes("data/brain/test_distill.jsonl");
    if (exported != 1) {
        std::cerr << "[FAIL] testdistillationextractor: expected 1 exported episode\n";
        return 1;
    }

    std::cout << "[PASS] testdistillationextractor\n";
    return 0;
}
