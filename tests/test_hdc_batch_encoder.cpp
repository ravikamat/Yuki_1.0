#include "brain/memory/HdcBatchEncoder.h"
#include "brain/language/Word2Vec.h"
#include "brain/database/DatabaseManager.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] HdcBatchEncoder..." << std::endl;

    yuki::language::Word2Vec w2v;
    yuki::memory::HdcBatchEncoder encoder(&w2v, 100);

    yuki::memory::Hypervector hv1 = encoder.getOrEncode("dog");
    yuki::memory::Hypervector hv2 = encoder.getOrEncode("dog");

    // Determinism test
    assert(hv1.cosineSimilarity(hv2) > 0.99f);

    // Bloom filter test
    assert(encoder.mightContain("dog"));

    // Batch encode parallel test
    std::vector<std::string> concepts = {"cat", "animal", "pet", "wolf", "fox"};
    auto batch_hvs = encoder.batchEncode(concepts);
    assert(batch_hvs.size() == 5);

    auto stats = encoder.getStats();
    assert(stats.hits >= 1);
    assert(stats.warm_stored > 0);

    std::cout << "[TEST] HdcBatchEncoder PASSED!" << std::endl;
    return 0;
}
