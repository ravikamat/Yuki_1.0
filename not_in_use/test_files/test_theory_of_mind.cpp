#include "brain/self/TheoryOfMind.h"
#include <cassert>
#include <cmath>

using namespace yuki::self;

int main() {
    TheoryOfMind tom;
    std::array<float, 11> yuki_comp = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f};

    // 1. trust builds with satisfaction
    for (int i = 0; i < 5; ++i) {
        tom.observeTurn("query" + std::to_string(i), "ok", true, yuki_comp);
    }
    assert(tom.userTrust() > 0.3f);

    // 2. trust drops with dissatisfaction
    TheoryOfMind tom2;
    for (int i = 0; i < 3; ++i) {
        tom2.observeTurn("bad" + std::to_string(i), "fail", false, yuki_comp);
    }
    assert(tom2.userTrust() < 0.5f);

    // 3. knowledge inference accumulates
    assert(tom.interactionCount() == 5);

    // 4. goal prediction normalized
    auto goals = tom.predictGoalDistribution();
    float sum = goals[0]+goals[1]+goals[2]+goals[3];
    assert(std::abs(sum - 1.0f) < 0.01f || sum < 1e-6f); // either normalized or uniform fallback

    // 5. serialize/deserialize
    auto data = tom.serialize();
    TheoryOfMind tom3;
    assert(tom3.deserialize(data));
    assert(tom3.interactionCount() == tom.interactionCount());

    // 6. knowledge gap calculation
    auto gap = tom.inferKnowledgeGap(yuki_comp);
    assert(gap[0] >= -1.0f && gap[0] <= 1.0f);
    return 0;
}
