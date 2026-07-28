#include "brain/knowledge/KnowledgeFilter.h"
#include "brain/language/Word2Vec.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] KnowledgeFilter..." << std::endl;

    yuki::language::Word2Vec w2v;
    yuki::knowledge::KnowledgeFilter filter(&w2v);

    yuki::knowledge::ConceptNetAssertion a1;
    a1.relation = "IsA";
    a1.start_concept = "dog";
    a1.end_concept = "animal";
    a1.weight = 3.0f;

    auto ft1 = filter.filter(a1);
    assert(ft1 != nullptr);
    assert(ft1->start == "dog");
    assert(ft1->end == "animal");
    assert(ft1->hash != 0);

    std::vector<yuki::knowledge::ConceptNetAssertion> batch = {a1};
    auto batch_res = filter.filterBatch(batch);
    assert(batch_res.size() == 1);

    auto stats = filter.getCoverageStats();
    assert(stats.start_avg >= 0.0f);

    std::cout << "[TEST] KnowledgeFilter PASSED!" << std::endl;
    return 0;
}
