#include "brain/creativity/ConceptBlender.h"
#include "brain/creativity/CreativeSearch.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/sleep/SleepThread.h"
#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/EmotionSystem.h"
#include "brain/organism/ConfidenceCalibrator.h"

#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::creativity;
    using namespace yuki::learning::generative;
    using namespace yuki::self;
    using namespace yuki::sleep;
    using namespace yuki::emotion;
    using namespace yuki::organism;

    std::cout << "[TEST] M10 Full Integration starting..." << std::endl;

    // 1. ConceptBlender
    ConceptBlender blender(8);
    std::vector<double> a = {1,0,0,0,0,0,0,0};
    std::vector<double> b = {0,1,0,0,0,0,0,0};
    auto blendRes = blender.blend(a, b, BlendMode::CONVEX, 0.5);
    assert(blendRes.blendVector.size() == 8);

    // 2. CreativeSearch
    CreativeSearch searcher(8);
    searcher.setKnownConcepts({a, b});
    auto searchRes = searcher.search(blendRes.blendVector, SearchMode::CONVERGENT, 2);
    assert(searchRes.conceptVector.size() == 8);

    // 3. VAE
    VAEConfig cfg;
    cfg.inputDim = 8;
    cfg.latentDim = 2;
    VariationalAutoencoder vae(cfg);
    auto rec = vae.forward(searchRes.conceptVector);
    assert(rec.size() == 8);

    // 4. IdentityPersistence
    IdentityPersistence pers("data/brain/test_m10_identity.db");
    SelfModel self; TheoryOfMind tom; ValenceArousalModel emo; ConfidenceCalibrator cal;
    bool saved = pers.saveIdentity(self, tom, emo, cal, "1.0.0");
    assert(saved);

    // 5. DreamEngine
    DreamEngine dreamEngine;
    dreamEngine.setVAE(&vae);
    auto dreams = dreamEngine.generateDreamCycle();
    assert(!dreams.empty());

    std::cout << "[TEST] M10 Full Integration PASSED!" << std::endl;
    return 0;
}
