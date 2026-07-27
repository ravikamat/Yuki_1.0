#include "brain/memory/MultimodalEncoder.h"
#include "brain/learning/selfplay/SelfPlayEngine.h"
#include "brain/sleep/SleepThread.h"
#include "brain/language/VaeResponseGenerator.h"
#include "brain/world/PhysicsWorld.h"
#include "brain/world/WorldModelBridge.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/Word2Vec.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] P1 & P2 End-to-End Integration..." << std::endl;

    // 1. Multimodal + VAE Coexistence
    yuki::memory::BindingMatrix binding;
    yuki::memory::MultimodalEncoder encoder(&binding);
    std::vector<float> audio(40, 0.1f), visual(3780, 0.2f), text(300, 0.3f);
    yuki::memory::Hypervector fused = encoder.fuse(audio, visual, text);

    yuki::learning::generative::VAEConfig cfg;
    cfg.inputDim = 308;
    cfg.latentDim = 64;
    yuki::learning::generative::VariationalAutoencoder vae(cfg);

    yuki::language::Word2Vec w2v;
    yuki::language::GrammarEngine grammar(&w2v, nullptr);
    yuki::language::VaeResponseGenerator vae_gen(&vae, &grammar, &w2v);
    std::string resp = vae_gen.generateResponse("CREATIVE_BLEND", {});
    assert(!resp.empty());

    // 2. Self-Play Engine
    yuki::learning::selfplay::SelfPlayEngine selfplay(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    auto outcome = selfplay.runEpisode();
    assert(outcome.reward >= -1.0f && outcome.reward <= 1.0f);

    // 3. Physics + Causal Graph Integration
    yuki::world::PhysicsWorld physics;
    yuki::memory::HdcSemanticGraph graph;
    yuki::world::WorldModelBridge bridge(&physics, &graph);
    bridge.bindConcept(1, {0.0f, 0.0f}, 1.0f);
    bridge.bindConcept(2, {0.5f, 0.0f}, 1.0f);

    yuki::causality::CausalGraph causal_graph;
    bridge.syncCausalRulesToGraph(&causal_graph);

    std::cout << "[TEST] P1 & P2 End-to-End Integration PASSED!" << std::endl;
    return 0;
}
