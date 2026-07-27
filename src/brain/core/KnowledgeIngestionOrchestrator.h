#pragma once
#include "brain/knowledge/ConceptNetAdapter.h"
#include "brain/knowledge/KnowledgeFilter.h"
#include "brain/language/GrammarExtractor.h"
#include "brain/knowledge/PhysicsKnowledgeBase.h"
#include "brain/ethics/ValueConstitution.h"
#include "brain/memory/HdcBatchEncoder.h"
#include "brain/knowledge/AutonomousIngestor.h"
#include <memory>

namespace yuki::core {

class KnowledgeIngestionOrchestrator {
public:
    KnowledgeIngestionOrchestrator();

    bool init(yuki::language::Word2Vec* w2v,
              yuki::memory::ConceptNetIngestor* cn_ingestor,
              yuki::causality::CausalGraph* causal_graph);

    yuki::knowledge::ConceptNetAdapter* conceptNetAdapter() { return adapter_.get(); }
    yuki::knowledge::KnowledgeFilter* knowledgeFilter() { return filter_.get(); }
    yuki::language::GrammarExtractor* grammarExtractor() { return grammar_extractor_.get(); }
    yuki::knowledge::PhysicsKnowledgeBase* physicsKnowledgeBase() { return physics_kb_.get(); }
    yuki::ethics::ValueConstitution* valueConstitution() { return constitution_.get(); }
    yuki::memory::HdcBatchEncoder* hdcBatchEncoder() { return hdc_encoder_.get(); }
    yuki::knowledge::AutonomousIngestor* autonomousIngestor() { return autonomous_ingestor_.get(); }

    bool isInitialized() const { return initialized_; }

private:
    std::unique_ptr<yuki::knowledge::ConceptNetAdapter> adapter_;
    std::unique_ptr<yuki::knowledge::KnowledgeFilter> filter_;
    std::unique_ptr<yuki::language::GrammarExtractor> grammar_extractor_;
    std::unique_ptr<yuki::knowledge::PhysicsKnowledgeBase> physics_kb_;
    std::unique_ptr<yuki::ethics::ValueConstitution> constitution_;
    std::unique_ptr<yuki::memory::HdcBatchEncoder> hdc_encoder_;
    std::unique_ptr<yuki::knowledge::AutonomousIngestor> autonomous_ingestor_;

    bool initialized_ = false;
};

} // namespace yuki::core
