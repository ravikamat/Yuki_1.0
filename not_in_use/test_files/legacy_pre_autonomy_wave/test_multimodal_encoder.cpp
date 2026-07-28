#include "brain/memory/MultimodalEncoder.h"
#include <iostream>
#include <cassert>
#include <vector>

int main() {
    std::cout << "[TEST] MultimodalEncoder & BindingMatrix..." << std::endl;

    using namespace yuki::memory;
    BindingMatrix binding;

    std::vector<float> audio(40, 0.5f);
    std::vector<float> visual(3780, 0.2f);
    std::vector<float> text(300, 0.8f);

    // 1. Projection determinism
    Hypervector h1 = binding.project(audio, ModalityType::AUDIO);
    Hypervector h2 = binding.project(audio, ModalityType::AUDIO);
    assert(BindingMatrix::similarity(h1, h2) > 0.999f);

    // 2. Multimodal Encoder Fusion
    MultimodalEncoder encoder(&binding);
    Hypervector fused1 = encoder.fuse(audio, visual, text);
    Hypervector fused2 = encoder.fuse(audio, visual, text);
    assert(BindingMatrix::similarity(fused1, fused2) > 0.999f);

    // 3. Permutation non-commutativity
    Hypervector perm1 = MultimodalEncoder::permute(h1, 1);
    Hypervector perm2 = MultimodalEncoder::permute(h1, 100);
    assert(BindingMatrix::similarity(perm1, perm2) < 0.9f);

    // 4. Contrastive training step
    ModalitySample anc{ModalityType::TEXT, text, h1};
    ModalitySample pos{ModalityType::VISUAL, visual, h2};
    std::vector<ModalitySample> negs;
    for (int i = 0; i < 4; ++i) {
        std::vector<float> neg_v(3780, 0.1f * i);
        negs.push_back(ModalitySample{ModalityType::VISUAL, neg_v, binding.project(neg_v, ModalityType::VISUAL)});
    }
    binding.trainStep(anc, pos, negs, 0.01f);

    std::cout << "[TEST] MultimodalEncoder PASSED!" << std::endl;
    return 0;
}
