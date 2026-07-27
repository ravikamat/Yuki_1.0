#include "brain/creativity/ConceptBlender.h"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    using namespace yuki::creativity;

    std::cout << "[TEST] ConceptBlender starting..." << std::endl;

    ConceptBlender blender(4);
    assert(blender.getEmbeddingDim() == 4);
    assert(blender.getLibrarySize() == 0);

    std::vector<std::vector<double>> library = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0}
    };
    blender.setConceptLibrary(library);
    assert(blender.getLibrarySize() == 2);

    std::vector<double> a = {1.0, 0.0, 0.0, 0.0};
    std::vector<double> b = {0.0, 1.0, 0.0, 0.0};

    // Test CONVEX blend
    BlendResult resConvex = blender.blend(a, b, BlendMode::CONVEX, 0.5);
    assert(resConvex.blendVector.size() == 4);
    assert(std::abs(resConvex.blendVector[0] - 0.5) < 1e-6);
    assert(std::abs(resConvex.blendVector[1] - 0.5) < 1e-6);
    assert(resConvex.novelty >= 0.0);

    // Test MULTIPLICATIVE blend
    BlendResult resMult = blender.blend(a, b, BlendMode::MULTIPLICATIVE, 0.5);
    assert(resMult.blendVector.size() == 4);

    // Test blendSeries
    auto series = blender.blendSeries(a, b, BlendMode::CONVEX, 5);
    assert(series.size() == 5);
    assert(std::abs(series[0].alpha - 0.0) < 1e-6);
    assert(std::abs(series[4].alpha - 1.0) < 1e-6);

    // Test selection
    BlendResult mostNovel = blender.selectMostNovel(series);
    BlendResult mostDiv = blender.selectMostDivergent(series);
    assert(mostNovel.blendVector.size() == 4);
    assert(mostDiv.blendVector.size() == 4);

    // Test serialization
    auto bytes = blender.serialize();
    assert(!bytes.empty());

    ConceptBlender blender2(4);
    bool ok = blender2.deserialize(bytes);
    assert(ok);
    assert(blender2.getEmbeddingDim() == 4);
    assert(blender2.getLibrarySize() == 2);

    std::cout << "[TEST] ConceptBlender PASSED!" << std::endl;
    return 0;
}
