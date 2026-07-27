#include "brain/core/KnowledgeIngestionOrchestrator.h"
#include "brain/language/Word2Vec.h"
#include "brain/causality/CausalGraph.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] Knowledge Ingestion Pipeline End-to-End Integration..." << std::endl;

    yuki::language::Word2Vec w2v;
    yuki::causality::CausalGraph causal_graph;

    yuki::core::KnowledgeIngestionOrchestrator orchestrator;
    bool ok = orchestrator.init(&w2v, nullptr, &causal_graph);
    assert(ok);
    assert(orchestrator.isInitialized());

    // 1. ConceptNet & Knowledge Filter
    assert(orchestrator.conceptNetAdapter() != nullptr);
    assert(orchestrator.knowledgeFilter() != nullptr);

    // 2. Physics & Causal Graph Sync
    assert(orchestrator.physicsKnowledgeBase() != nullptr);
    const auto* water = orchestrator.physicsKnowledgeBase()->getMaterial("water");
    assert(water != nullptr);
    assert(!causal_graph.nodes.empty());

    // 3. Gita Value Constitution & Policy Selector Integration
    assert(orchestrator.valueConstitution() != nullptr);
    yuki::policy::PolicySelector policy_selector;
    policy_selector.setValueConstitution(orchestrator.valueConstitution());

    auto report = orchestrator.valueConstitution()->evaluate("perform duty without attachment to results", {"action", "duty"});
    assert(report.alignment_score >= -1.0f && report.alignment_score <= 1.0f);

    // 4. HDC Encoder & Autonomous Ingestor
    assert(orchestrator.hdcBatchEncoder() != nullptr);
    assert(orchestrator.autonomousIngestor() != nullptr);

    uint64_t job_id = orchestrator.autonomousIngestor()->autoQueueForGap("physics", 1.0f);
    assert(job_id > 0);

    auto progress = orchestrator.autonomousIngestor()->processJob(job_id);
    assert(progress.complete);

    std::cout << "[TEST] Knowledge Ingestion Pipeline End-to-End Integration PASSED!" << std::endl;
    return 0;
}
