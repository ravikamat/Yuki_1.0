#include "brain/core/KnowledgeIngestionOrchestrator.h"
#include "brain/causality/CausalGraph.h"
#include <iostream>

namespace yuki::core {

KnowledgeIngestionOrchestrator::KnowledgeIngestionOrchestrator() = default;

bool KnowledgeIngestionOrchestrator::init(yuki::language::Word2Vec* w2v,
                                           yuki::memory::ConceptNetIngestor* cn_ingestor,
                                           yuki::causality::CausalGraph* causal_graph) {
    adapter_ = std::make_unique<yuki::knowledge::ConceptNetAdapter>("data/conceptnet_config.txt");
    filter_ = std::make_unique<yuki::knowledge::KnowledgeFilter>(w2v);
    grammar_extractor_ = std::make_unique<yuki::language::GrammarExtractor>();
    physics_kb_ = std::make_unique<yuki::knowledge::PhysicsKnowledgeBase>();
    constitution_ = std::make_unique<yuki::ethics::ValueConstitution>(w2v);
    hdc_encoder_ = std::make_unique<yuki::memory::HdcBatchEncoder>(w2v, 100000);

    constitution_->load("data/gita_constitution.jsonl");
    physics_kb_->load("data/physics_knowledge.jsonl");
    if (causal_graph) {
        physics_kb_->syncToCausalGraph(causal_graph);
    }

    autonomous_ingestor_ = std::make_unique<yuki::knowledge::AutonomousIngestor>(
        cn_ingestor, grammar_extractor_.get(), physics_kb_.get(), constitution_.get()
    );

    initialized_ = true;
    return true;
}

} // namespace yuki::core
