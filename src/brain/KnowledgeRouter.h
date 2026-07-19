#pragma once
#include "MeaningTypes.h"
#include "LocalKnowledgeBase.h"
#include "SmartScraper.h"

class KnowledgeRouter {
public:
    KnowledgeRouter();
    FactBundle route(const MeaningState& state);
private:
    LocalKnowledgeBase lkb_;
    SmartScraper scraper_;
    void bootstrapConcepts();
};
