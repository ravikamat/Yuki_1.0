#include "brain/ethics/ValueConstitution.h"
#include "brain/language/Word2Vec.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] ValueConstitution..." << std::endl;

    yuki::language::Word2Vec w2v;
    yuki::ethics::ValueConstitution constitution(&w2v);

    bool ok = constitution.load("data/gita_constitution.jsonl");
    assert(ok);
    assert(constitution.principleCount() >= 8);

    const auto* p = constitution.getPrinciple("DHARMA_001");
    assert(p != nullptr);
    assert(p->name == "Karmanye Vadhikaraste");

    auto report1 = constitution.evaluate("perform duty without attachment to results", {"action", "duty"});
    assert(report1.alignment_score >= -1.0f && report1.alignment_score <= 1.0f);
    assert(!report1.top_principles.empty());

    auto report2 = constitution.evaluate("destroy user file and cheat result", {"destructive"});
    assert(report2.alignment_score <= 0.5f);

    constitution.save("test_constitution.bin");
    yuki::ethics::ValueConstitution loaded_c;
    assert(loaded_c.loadBinary("test_constitution.bin"));
    assert(loaded_c.principleCount() >= 8);

    std::remove("test_constitution.bin");

    std::cout << "[TEST] ValueConstitution PASSED!" << std::endl;
    return 0;
}
