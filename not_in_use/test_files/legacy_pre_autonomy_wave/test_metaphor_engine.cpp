#include "brain/language/MetaphorEngine.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::language;

    std::cout << "[TEST] MetaphorEngine starting..." << std::endl;

    MetaphorEngine engine;
    engine.loadTemplates("data/metaphor_templates.txt");

    auto met = engine.generateMetaphor("Memory", "Ocean");
    assert(!met.expression.empty());
    assert(met.aptness > 0.0);
    assert(!met.isSimile);

    auto sim = engine.generateSimile("Thought", "Spark");
    assert(!sim.expression.empty());
    assert(sim.isSimile);

    // Test serialization
    auto bytes = engine.serialize();
    assert(!bytes.empty());

    MetaphorEngine engine2;
    bool ok = engine2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] MetaphorEngine PASSED!" << std::endl;
    return 0;
}
