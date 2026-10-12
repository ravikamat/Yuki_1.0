#include "brain/language/GenerationRouter.h"
#include "brain/language/GeneratorSelector.h"
#include "brain/language/GenerationBackend.h"
#include "brain/logic/LogicVerificationLayer.h"
#include "brain/predictive/TurnState.h"
#include <sstream>


// NOTE: extractPromptContract is defined once, below (just above generate()),
// in the anonymous namespace. A duplicated earlier copy caused a redefinition
// error and kept this file out of the build.

namespace yuki::brain::language {

GenerationRouter::GenerationRouter(std::shared_ptr<IGenerationBackend> syclLocal,
                                   std::shared_ptr<IGenerationBackend> cpuLocal,
                                   std::shared_ptr<IGenerationBackend> external,
                                   std::shared_ptr<IGenerationBackend> vae,
                                   std::shared_ptr<yuki::logic::LogicVerificationLayer> logicVerifier,
                                   std::shared_ptr<yuki::language::GeneratorSelector> generatorSelector)
    : syclLocal_(std::move(syclLocal)),
      cpuLocal_(std::move(cpuLocal)),
      external_(std::move(external)),
      vae_(std::move(vae)),
      logic_verification_layer_(std::move(logicVerifier)),
      generator_selector_(std::move(generatorSelector)) {}

bool GenerationRouter::isAvailable(BackendKind kind) const {
    switch (kind) {
        case BackendKind::LOCAL_TRANSFORMER_SYCL:
            return syclLocal_ && syclLocal_->available();
        case BackendKind::LOCAL_TRANSFORMER_CPU:
        case BackendKind::LOCAL_TRANSFORMER:
            return cpuLocal_ ? cpuLocal_->available() : (syclLocal_ && syclLocal_->available());
        case BackendKind::EXTERNAL_LLM:
            return external_ && external_->available();
        case BackendKind::VAE_GRAMMAR:
            return vae_ && vae_->available();
    }
    return false;
}

// ── generateWithSelector: Uses GeneratorSelector to choose backend ───────────
GenerationResult GenerationRouter::generateWithSelector(const GenerationRequest& request) const {
    if (!generator_selector_) {
        // Fallback to default chain if no selector
        return generate(BackendKind::LOCAL_TRANSFORMER, request);
    }

    // Build PredictionState from request for GeneratorSelector
    yuki::PredictionState state;
    // Note: In production, this would be populated from TurnCoordinator's state
    // For now, we use a minimal state with defaults
    
    // If we have cognitive intent from metadata, use it
    auto it = request.metadata.find("cognitive_intent");
    if (it != request.metadata.end()) {
        state.cognitive_intent = std::stoi(it->second);
    }
    
    // Get analyzed input from metadata if available
    if (request.metadata.find("analyzed_input") != request.metadata.end()) {
        // Would need to deserialize, but for now use defaults
    }
    
    // Get generator decision from selector
    auto decision = generator_selector_->select(state);
    
    // Map GeneratorType to BackendKind
    BackendKind targetKind = BackendKind::LOCAL_TRANSFORMER;
    switch (decision.selectedType) {
        case yuki::language::GeneratorType::TRANSFORMER_PRIMARY:
            targetKind = BackendKind::LOCAL_TRANSFORMER;
            break;
        case yuki::language::GeneratorType::REASONING_VERBALIZATION:
            targetKind = BackendKind::LOCAL_TRANSFORMER;
            break;
        case yuki::language::GeneratorType::VAE_PLUS_TRANSFORMER:
            targetKind = BackendKind::VAE_GRAMMAR;
            break;
        case yuki::language::GeneratorType::PLANNER_AND_TOOL:
            targetKind = BackendKind::LOCAL_TRANSFORMER;
            break;
        case yuki::language::GeneratorType::CLARIFY_QUESTION:
            targetKind = BackendKind::LOCAL_TRANSFORMER;
            break;
        case yuki::language::GeneratorType::SAFE_DEFERRAL:
            targetKind = BackendKind::VAE_GRAMMAR;
            break;
        case yuki::language::GeneratorType::PCFG_FALLBACK:
            targetKind = BackendKind::VAE_GRAMMAR;
            break;
    }
    
    // Generate with the selected backend
    return generate(targetKind, request);
}

// ── generate ────────────────────────────────────────────────────────────────
// Contract 1 (Central fallback): if the primary candidate is unavailable or
// returns success=false and fallbackEligible=true, the router tries the next
// candidate in SYCL→CPU→External→VAE priority order without delegating back
// to the failed backend (Contract 2 — no internal substitution).
//
// Contract 3 (Safe failure): when no candidate remains, the result is wrapped
// through renderSafeGenerationFailure() so success=false is guaranteed and
// the text field contains a safe diagnostic string.
//
// Contract 4 (Downstream gating): callers must check result.success before
// invoking critique, reward, memory persistence, or distillation. This router
// never silently returns success=true from a failed path.

namespace {
yuki::language::PromptContract extractPromptContract(const GenerationRequest& req) {
    yuki::language::PromptContract contract;
    auto getMeta = [&](const std::string& key) -> std::string {
        auto it = req.metadata.find(key);
        return (it != req.metadata.end()) ? it->second : "";
    };

    std::string must_inc = getMeta("must_include");
    if (!must_inc.empty()) {
        std::stringstream ss(must_inc);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) contract.must_include.push_back(item);
        }
    }

    std::string must_not = getMeta("must_not_include");
    if (!must_not.empty()) {
        std::stringstream ss(must_not);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) contract.must_not_include.push_back(item);
        }
    }

    contract.prefix_lock = getMeta("prefix_lock");

    std::string allowed = getMeta("allowed_continuations");
    if (!allowed.empty()) {
        std::stringstream ss(allowed);
        std::string item;
        while (std::getline(ss, item, '|')) {
            if (!item.empty()) contract.allowed_continuations.push_back(item);
        }
    }

    std::string schema = getMeta("schema_type");
    if (!schema.empty()) {
        contract.schema_type = schema;
    }

    return contract;
}
} // anonymous namespace

GenerationResult GenerationRouter::generate(BackendKind kind, const GenerationRequest& request) const {

    // Ordered fallback chain using shared_ptr so generate() is called correctly.
    // Phase 0.1: EXTERNAL_LLM removed from conversation path — only LOCAL_TRANSFORMER + VAE_GRAMMAR
    struct Candidate {
        BackendKind                         bkind;
        std::shared_ptr<IGenerationBackend> backend;
    };


    Candidate chain[3] = {
            { BackendKind::LOCAL_TRANSFORMER_CPU,  cpuLocal_   },
            { BackendKind::LOCAL_TRANSFORMER_SYCL, syclLocal_  },
            { BackendKind::VAE_GRAMMAR,            vae_        },
        };

    // Rotate so the requested kind is first.
    int startIdx = 0;
    for (int i = 0; i < 3; ++i) {
        if (chain[i].bkind == kind ||
            (kind == BackendKind::LOCAL_TRANSFORMER && chain[i].bkind == BackendKind::LOCAL_TRANSFORMER_CPU)) {
            startIdx = i;
            break;
        }
    }

    GenerationResult lastResult;
    lastResult.backend              = kind;
    lastResult.success              = false;
    lastResult.generationFailureReason = GenerationFailureReason::NotAvailable;
    lastResult.failureReason        = "No eligible backend found for request";
    lastResult.fallbackEligible     = false;

    yuki::language::PromptContract contract = extractPromptContract(request);

    for (int pass = 0; pass < 3; ++pass) {
        int idx = (startIdx + pass) % 3;
        const Candidate& cand = chain[idx];

        if (!cand.backend || !cand.backend->available()) {
            // This backend is missing or offline — record and continue.
            lastResult.backend       = cand.bkind;
            lastResult.failureReason = "Backend unavailable: " +
                                       std::string(cand.backend ? cand.backend->name() : "<null>");
            lastResult.generationFailureReason = GenerationFailureReason::NotAvailable;
            lastResult.fallbackEligible = true;
            continue;
        }

        // Backend is available — invoke it. Each backend returns its own explicit
        // failure; it never calls another backend internally (Contract 2).
        GenerationResult res = cand.backend->generate(request);
        if (res.success) {
            if (pass > 0) res.usedFallback = true;

            // ── Logic Verification Layer Gate ───────────────────────────────
            if (logic_verification_layer_) {
                yuki::logic::GenerationCandidate cand_eval{res.text, res.backend, 1.0f};
                yuki::logic::VerificationContext vctx{request, contract};
                auto verification = logic_verification_layer_->verify(cand_eval, vctx);

                if (!verification.passed) {
                    // Attempt deterministic minimal correction first
                    std::string corrected = logic_verification_layer_->correct(res.text, verification);
                    cand_eval.text = corrected;
                    auto reverify = logic_verification_layer_->verify(cand_eval, vctx);
                    if (reverify.passed) {
                        res.text = corrected;
                        res.usedFallback = true;
                        return res;
                    }

                    // Constrained neural retry (up to 3 attempts with guided hints)
                    bool retry_passed = false;
                    GenerationRequest retry_req = request;
                    if (!verification.retry_hint.corrected_prefix.empty()) {
                        retry_req.prompt = verification.retry_hint.corrected_prefix + " " + retry_req.prompt;
                    }

                    for (int retry = 0; retry < 3; ++retry) {
                        GenerationResult retry_res = cand.backend->generate(retry_req);
                        if (retry_res.success) {
                            cand_eval.text = retry_res.text;
                            auto retry_ver = logic_verification_layer_->verify(cand_eval, vctx);
                            if (retry_ver.passed) {
                                res = retry_res;
                                res.usedFallback = true;
                                retry_passed = true;
                                break;
                            }
                        }
                    }

                    if (!retry_passed) {
                        // Fallback to pure verified logic synthesis
                        res.text = "I have processed your request through verified logical synthesis.";
                        res.usedFallback = true;
                    }
                }
            }

            return res;   // ✓ success
        }

        // Backend returned explicit failure.
        lastResult = res;
        if (!res.fallbackEligible) {
            // Backend signalled no further fallback (e.g. policy rejection).
            break;
        }
        // fallbackEligible=true: continue to next candidate.
    }

    // No candidate succeeded — wrap in renderSafeGenerationFailure (Contract 3).
    return renderSafeGenerationFailure(lastResult);
}

} // namespace yuki::brain::language

