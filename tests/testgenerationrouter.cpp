#include "src/brain/language/GenerationRouter.h"
#include <cassert>
#include <iostream>

class MockBackend : public yuki::brain::language::IGenerationBackend {
public:
    MockBackend(yuki::brain::language::BackendKind kind, bool avail) : kind_(kind), avail_(avail) {}
    yuki::brain::language::GenerationResult generate(const yuki::brain::language::GenerationRequest&) override {
        yuki::brain::language::GenerationResult res;
        res.success = avail_;
        res.backend = kind_;
        res.text = "MockResult";
        return res;
    }
    bool available() const override { return avail_; }
    yuki::brain::language::BackendKind kind() const override { return kind_; }
    std::string name() const override { return "MockBackend"; }
    float estimateCost(const yuki::brain::language::GenerationRequest&) const override { return 0.0f; }
private:
    yuki::brain::language::BackendKind kind_;
    bool avail_;
};

int main() {
    std::cout << "Running testgenerationrouter...\n";
    using namespace yuki::brain::language;

    auto sycl = std::make_shared<MockBackend>(BackendKind::LOCAL_TRANSFORMER_SYCL, true);
    auto cpu = std::make_shared<MockBackend>(BackendKind::LOCAL_TRANSFORMER_CPU, false);

    GenerationRouter router(sycl, cpu, nullptr, nullptr);

    assert(router.isAvailable(BackendKind::LOCAL_TRANSFORMER_SYCL));
    assert(!router.isAvailable(BackendKind::EXTERNAL_LLM));

    GenerationRequest req;
    req.prompt = "Test prompt";

    auto resSycl = router.generate(BackendKind::LOCAL_TRANSFORMER_SYCL, req);
    assert(resSycl.success);
    assert(resSycl.text == "MockResult");

    auto resExt = router.generate(BackendKind::EXTERNAL_LLM, req);
    assert(!resExt.success);
    assert(!resExt.failureReason.empty());

    std::cout << "[PASS] testgenerationrouter completed cleanly.\n";
    return 0;
}
