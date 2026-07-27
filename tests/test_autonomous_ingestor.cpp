#include "brain/knowledge/AutonomousIngestor.h"
#include "brain/memory/ConceptNetIngestor.h"
#include "brain/language/GrammarExtractor.h"
#include "brain/knowledge/PhysicsKnowledgeBase.h"
#include "brain/ethics/ValueConstitution.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] AutonomousIngestor..." << std::endl;

    yuki::language::GrammarExtractor extractor;
    yuki::knowledge::PhysicsKnowledgeBase pkb;
    yuki::ethics::ValueConstitution constitution;

    yuki::knowledge::AutonomousIngestor ingestor(nullptr, &extractor, &pkb, &constitution);

    uint64_t job1 = ingestor.autoQueueForGap("physics", 2.0f);
    assert(job1 > 0);

    auto progress1 = ingestor.processJob(job1);
    assert(progress1.complete);
    assert(progress1.job_id == job1);

    uint64_t job2 = ingestor.autoQueueForGap("ethics", 1.5f);
    assert(job2 > 0);
    auto progress2 = ingestor.processJob(job2);
    assert(progress2.complete);

    std::cout << "[TEST] AutonomousIngestor PASSED!" << std::endl;
    return 0;
}
