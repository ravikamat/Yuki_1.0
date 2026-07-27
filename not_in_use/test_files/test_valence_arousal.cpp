#include "brain/emotion/ValenceArousalModel.h"
#include <cassert>
#include <cmath>

using namespace yuki::emotion;

int main() {
    ValenceArousalModel vam;

    // 1. positive reward increases valence
    vam.update(1.0f, 1.0f, 0.5f, 0.0f);
    assert(vam.valence() > 0.0f);

    // 2. surprise increases arousal
    ValenceArousalModel vam2;
    vam2.update(0.0f, 0.0f, 0.5f, 1.0f);
    assert(vam2.arousal() > 0.5f);

    // 3. high arousal raises threshold
    float base = 0.5f;
    float mod_high = vam2.modulateThreshold(base);
    assert(mod_high > base);

    // 4. positive valence lowers threshold
    float mod_pos = vam.modulateThreshold(base);
    assert(mod_pos < base);

    // 5. decay reduces arousal
    float before = vam2.arousal();
    vam2.decay();
    assert(vam2.arousal() < before);

    // 6. serialize/deserialize
    auto data = vam.serialize();
    ValenceArousalModel vam3;
    assert(vam3.deserialize(data));
    assert(std::abs(vam3.valence() - vam.valence()) < 1e-5f);
    return 0;
}
