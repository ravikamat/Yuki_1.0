#pragma once

#include "brain/language/GenerationBackend.h"
#include <memory>
#include <atomic>

namespace yuki::logic { class LogicVerificationLayer; }
namespace yuki::language { class GeneratorSelector; }

namespace yuki::brain::language {

class GenerationRouter {
public:
    // Phase 0.1: Generation metrics for LLM-free gate verification
    struct GenerationStats {
        std::atomic<uint64_t> totalGenerations{0};
        std::atomic<uint64_t> localTransformerSuccesses{0};
        std::atomic<uint64_t> vaeSuccesses{0};
        std::atomic<uint64_t> externalFallbacks{0};  // Must be 0 for LLM-free gate
        std::atomic<uint64_t> totalFailures{0};
    };

    GenerationRouter() = default;
    GenerationRouter(std::shared_ptr<IGenerationBackend> syclLocal,
                     std::shared_ptr<IGenerationBackend> cpuLocal,
                     std::shared_ptr<IGenerationBackend> external,
                     std::shared_ptr<IGenerationBackend> vae,
                     std::shared_ptr<yuki::logic::LogicVerificationLayer> logicVerifier = nullptr,
                     std::shared_ptr<yuki::language::GeneratorSelector> generatorSelector = nullptr);

    void setLogicVerificationLayer(std::shared_ptr<yuki::logic::LogicVerificationLayer> verifier) noexcept {
        logic_verification_layer_ = std::move(verifier);
    }

    void setGeneratorSelector(std::shared_ptr<yuki::language::GeneratorSelector> selector) noexcept {
        generator_selector_ = std::move(selector);
    }

    GenerationResult generate(BackendKind kind,
                              const GenerationRequest& request) const;

    GenerationResult generateWithSelector(const GenerationRequest& request) const;

    bool isAvailable(BackendKind kind) const;

    const GenerationStats& getStats() const { return stats_; }

private:
    std::shared_ptr<IGenerationBackend> syclLocal_;
    std::shared_ptr<IGenerationBackend> cpuLocal_;
    std::shared_ptr<IGenerationBackend> external_;
    std::shared_ptr<IGenerationBackend> vae_;
    std::shared_ptr<yuki::logic::LogicVerificationLayer> logic_verification_layer_{nullptr};
    std::shared_ptr<yuki::language::GeneratorSelector> generator_selector_{nullptr};
    mutable GenerationStats stats_;
};

} // namespace yuki::brain::language

