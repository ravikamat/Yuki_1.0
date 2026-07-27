#include "brain/knowledge/ConceptNetAdapter.h"
#include <iostream>
#include <fstream>
#include <cassert>

int main() {
    std::cout << "[TEST] ConceptNetAdapter..." << std::endl;

    // Create a temporary test assertions CSV
    std::string test_csv = "test_assertions.csv";
    std::ofstream out(test_csv);
    out << "/r/IsA\t/c/en/dog\t/c/en/animal\t/d/conceptnet\t3.0\n";
    out << "/r/IsA\t/c/en/cat\t/c/en/animal\t/d/conceptnet\t2.5\n";
    out << "/r/IsA\t/c/en/dog\t/c/en/animal\t/d/conceptnet\t3.0\n"; // duplicate
    out << "/r/IsA\t/c/fr/chien\t/c/fr/animal\t/d/conceptnet\t4.0\n"; // non-English
    out << "/r/IsA\t/c/en/bug\t/c/en/thing\t/d/conceptnet\t0.5\n"; // low weight
    out.close();

    yuki::knowledge::ConceptNetAdapter adapter("data/conceptnet_config.txt");

    size_t count = 0;
    auto stats = adapter.parseStream(test_csv, [&](const yuki::knowledge::ConceptNetAssertion& a) {
        std::cout << "Callback assertion: start='" << a.start_concept << "' rel='" << a.relation << "' end='" << a.end_concept << "' weight=" << a.weight << std::endl;
        assert(a.start_concept == "dog" || a.start_concept == "cat");
        count++;
        return true;
    });

    std::cout << "stats.accepted=" << stats.accepted << " (expected 2)" << std::endl;
    std::cout << "stats.deduped=" << stats.deduped << " (expected >=1)" << std::endl;
    std::cout << "stats.filtered_lang=" << stats.filtered_lang << " (expected >=1)" << std::endl;
    std::cout << "stats.filtered_weight=" << stats.filtered_weight << " (expected >=1)" << std::endl;
    std::cout << "count=" << count << " (expected 2)" << std::endl;

    assert(stats.accepted == 2);
    assert(stats.deduped >= 1);
    assert(stats.filtered_lang >= 1);
    assert(stats.filtered_weight >= 1);
    assert(count == 2);

    auto est = adapter.estimate(test_csv);
    assert(est.total >= 0);

    std::remove(test_csv.c_str());

    std::cout << "[TEST] ConceptNetAdapter PASSED!" << std::endl;
    return 0;
}
