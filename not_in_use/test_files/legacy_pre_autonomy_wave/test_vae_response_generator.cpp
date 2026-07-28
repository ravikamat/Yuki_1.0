#include "brain/language/VaeResponseGenerator.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/Word2Vec.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[TEST] VaeResponseGenerator..." << std::endl;

    yuki::learning::generative::VAEConfig cfg;
    cfg.inputDim = 308;
    cfg.latentDim = 64;
    yuki::learning::generative::VariationalAutoencoder vae(cfg);

    yuki::language::Word2Vec w2v;
    yuki::language::GrammarEngine grammar(&w2v, nullptr);

    yuki::language::VaeResponseGenerator generator(&vae, &grammar, &w2v);
    assert(!generator.isTrained());

    std::string resp = generator.generateResponse("GREETING", {});
    assert(!resp.empty());

    std::cout << "[TEST] VaeResponseGenerator PASSED!" << std::endl;
    return 0;
}
