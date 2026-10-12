#include "brain/language/NativeGenerationBackend.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/GrammarEngine.h"

#include <cctype>
#include <chrono>
#include <sstream>
#include <vector>

namespace yuki::brain::language {

namespace {

// Cheap tokenizer: lowercase alphanumeric words, length >= 3, deduped. The
// frame only needs a handful of grounded concepts to be synthesizable.
std::vector<std::string> keywords(const std::string& text, size_t max_words = 4) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        if (cur.size() >= 3) {
            for (char& c : cur) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            bool dup = false;
            for (const auto& w : out) if (w == cur) { dup = true; break; }
            if (!dup) out.push_back(cur);
        }
        cur.clear();
    };
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) cur.push_back(c);
        else flush();
        if (out.size() >= max_words) break;
    }
    flush();
    return out;
}

} // namespace

GenerationResult NativeGenerationBackend::generate(const GenerationRequest& request) {
    GenerationResult result;
    result.backend     = BackendKind::VAE_GRAMMAR;
    result.backendKind = BackendKind::VAE_GRAMMAR;
    result.backendName = "NativeSentenceSynthesizer";

    if (!synth_) {
        result.success = false;
        result.failureReason = "NativeSentenceSynthesizer not wired";
        result.generationFailureReason = GenerationFailureReason::NotConfigured;
        result.fallbackEligible = true;
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();

    yuki::language::SynthesisFrame frame;
    frame.intent = "descriptive";
    frame.tone   = "helpful";

    const auto words = keywords(request.prompt, 4);
    static const char* kRoles[] = {"AGENT", "PATIENT", "INSTRUMENT", "LOCATION"};
    for (size_t i = 0; i < words.size(); ++i) {
        frame.slots.emplace_back(kRoles[i % 4], words[i]);
    }
    frame.target_word_count = 6 + words.size();

    std::string sentence = synth_->synthesize(frame, 5);

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    result.latencyMs = result.elapsedMs;

    if (sentence.empty()) {
        result.success = false;
        result.failureReason = "Native synthesis produced no sentence";
        result.generationFailureReason = GenerationFailureReason::InvalidResponse;
        result.fallbackEligible = true;
        return result;
    }

    result.success        = true;
    result.text           = std::move(sentence);
    result.confidence     = 0.6f;
    result.fluencyScore   = 0.6f;
    result.relevanceScore = 0.6f;
    result.safetyScore    = 1.0f;
    result.fallbackEligible = false;
    return result;
}

} // namespace yuki::brain::language
