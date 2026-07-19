// VseBootstrapTrainer.cpp
// Injects 160 synthetic examples (20 per intent class) into the VSE
// GenerativeModel to bootstrap intent→feature mappings from structural
// priors toward learned predictions.
//
// Feature vector (12 dims):
//  [0] length_norm           [1] word_count_norm
//  [2] question              [3] command
//  [4] emotional             [5] technical
//  [6] greeting              [7] urgency
//  [8] yuki_name             [9] action_cue
//  [10] polarity             [11] confidence

#include "brain/inference/VseBootstrapTrainer.h"
#include <iostream>
#include <cmath>

namespace yuki::inference {

VseBootstrapTrainer::VseBootstrapTrainer(VariationalStateEstimator* vse)
    : vse_(vse) {}

// ── Feature builders ─────────────────────────────────────────────────────────

std::vector<float> VseBootstrapTrainer::queryFeatures_(bool is_question,
                                                        bool has_topic,
                                                        bool is_technical) {
    // QUERY: length=0.5, wc=0.4, question=0.9, technical=0.3, confidence=0.9
    return {0.5f, 0.4f, 0.9f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol   conf
}

std::vector<float> VseBootstrapTrainer::commandFeatures_(bool is_imperative,
                                                          bool has_target,
                                                          bool is_short) {
    // COMMAND: length=0.4, wc=0.3, command=0.9, action_cue=0.85 (boosted), confidence=0.9
    // action_cue raised from 0.5 → 0.85 to sharpen separation from CLARIFICATION
    return {0.4f, 0.3f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.85f, 0.0f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act   pol   conf
}

std::vector<float> VseBootstrapTrainer::tutorialFeatures_() {
    // TUTORIAL: technical=0.9, question=0.6 ("explain"/"show me how"), action_cue=0.3
    // question and action_cue added so "explain X" / "show me" don't fall to CLARIFICATION
    return {0.6f, 0.5f, 0.6f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol   conf
}

std::vector<float> VseBootstrapTrainer::emotionalFeatures_(bool positive) {
    // EMOTIONAL: length=0.6, wc=0.5, emotional=0.9, polarity=±0.3, confidence=0.9
    return {0.6f, 0.5f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, positive?0.3f:-0.3f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol              conf
}

std::vector<float> VseBootstrapTrainer::clarificationFeatures_() {
    // CLARIFICATION: MUST look like a genuine question (question↑) with no action verb.
    // Previously all-zeros on intent dims → won whenever input had weak signals.
    // Now requires: question=0.75 (it IS a question), command=0.1 (NOT a command),
    // action_cue=0.1 (no action verb), urgency=0.6 ("I need to clarify").
    return {0.5f, 0.5f, 0.75f, 0.1f, 0.3f, 0.4f, 0.2f, 0.6f, 0.1f, 0.1f, 0.0f, 0.5f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol   conf
}

std::vector<float> VseBootstrapTrainer::metaFeatures_() {
    // META_QUESTION/GREETING: short input, strong greeting signal, no question/command
    // [0]=length_norm [1]=word_count_norm [2]=question [3]=command [4]=emotional
    // [5]=technical   [6]=greeting        [7]=urgency  [8]=yuki_name [9]=action_cue
    // [10]=polarity   [11]=confidence
    return {0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol   conf
}

std::vector<float> VseBootstrapTrainer::abortFeatures_() {
    // ABORT: very short, neutral, slightly urgent — no greeting signal
    return {0.1f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.85f, 0.0f, 0.0f, 0.0f, 0.9f};
    //      len   wc    q     cmd   emo   tech  greet  urg  name  act  pol   conf
}

std::vector<float> VseBootstrapTrainer::generalFeatures_() {
    return {0.5f, 0.3f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.5f};
}

// ── Example builder ───────────────────────────────────────────────────────────

std::vector<VseBootstrapTrainer::TrainingExample>
VseBootstrapTrainer::buildExamples_() const {
    using M = yuki::perception::Modality;
    std::vector<TrainingExample> examples;
    examples.reserve(160);

    // ── QUERY: 20 examples ────────────────────────────────────────────────────
    // "what is X?", "how does Y work?", "tell me about Z"
    for (int i = 0; i < 20; ++i) {
        examples.push_back({IntentClass::QUERY, M::TEXT,
            queryFeatures_(i % 2 == 0, i % 3 != 0, i % 4 == 0)});
    }

    // ── COMMAND: 20 examples ──────────────────────────────────────────────────
    // "open X", "start Y", "show me Z", "do X", "run Y"
    for (int i = 0; i < 20; ++i) {
        examples.push_back({IntentClass::COMMAND, M::TEXT,
            commandFeatures_(i % 3 == 0, i % 2 == 0, i % 4 < 2)});
    }

    // ── TUTORIAL: 20 examples ─────────────────────────────────────────────────
    // "how do I learn X?", "explain step by step", "can you teach me"
    for (int i = 0; i < 20; ++i) {
        auto f = tutorialFeatures_();
        // Vary length and word_count slightly across examples (stay in spec range)
        f[0] = 0.5f + 0.05f * static_cast<float>(i % 5);  // [0] length_norm: 0.5–0.7
        f[1] = 0.4f + 0.04f * static_cast<float>(i % 4);  // [1] word_count_norm: 0.4–0.56
        // [7] urgency stays 0.0 — tutorial inputs are not urgent
        examples.push_back({IntentClass::TUTORIAL, M::TEXT, f});
    }

    // ── EMOTIONAL_VENT: 20 examples ───────────────────────────────────────────
    // "I'm frustrated", "this makes me happy", "I feel lost"
    for (int i = 0; i < 20; ++i) {
        examples.push_back({IntentClass::EMOTIONAL_VENT, M::TEXT,
            emotionalFeatures_(i % 2 == 0)});
    }

    // ── CLARIFICATION_RESPONSE: 20 examples ───────────────────────────────────
    // "I meant X", "no, I was asking about Y", "to clarify, Z"
    for (int i = 0; i < 20; ++i) {
        auto f = clarificationFeatures_();
        f[10] = (i % 3 == 0) ? 0.6f : 0.2f; // vary negation
        examples.push_back({IntentClass::CLARIFICATION_RESPONSE, M::TEXT, f});
    }

    // ── META_QUESTION: 20 examples ────────────────────────────────────────────
    // "what can you do?", "hi there", "are you ready?", "hello", "good morning"
    for (int i = 0; i < 20; ++i) {
        auto f = metaFeatures_();
        // Vary length slightly across examples
        f[0] = 0.1f + 0.05f * static_cast<float>(i % 5);  // [0] length_norm
        f[1] = 0.1f + 0.02f * static_cast<float>(i % 4);  // [1] word_count_norm
        examples.push_back({IntentClass::META_QUESTION, M::TEXT, f});
    }

    // ── ABORT: 20 examples ────────────────────────────────────────────────────
    // "nevermind", "stop", "forget it", "cancel", "never mind"
    for (int i = 0; i < 20; ++i) {
        auto f = abortFeatures_();
        // Some abort commands have mild urgency variation — stays in [7] slot
        f[7] = 0.75f + 0.05f * static_cast<float>(i % 5);  // [7] urgency HIGH (0.75-0.95)
        examples.push_back({IntentClass::ABORT, M::TEXT, f});
    }

    // ── UNKNOWN/GENERAL: 20 examples ─────────────────────────────────────────
    // Mixed signals, casual chat, short phatic utterances
    for (int i = 0; i < 20; ++i) {
        auto f = generalFeatures_();
        // Greetings: set greeting at [6] (was wrongly using [8]=yuki_name)
        if (i < 5) { f[6] = 0.8f; }          // "hi", "hello", "hey" — greeting only
        // Farewells: set urgency at [7] (was wrongly using [9]=action_cue)
        else if (i < 8) { f[7] = 0.3f; }     // "bye", "goodbye"
        examples.push_back({IntentClass::UNKNOWN, M::TEXT, f});
    }

    return examples;
}

// ── Main injection ────────────────────────────────────────────────────────────

int VseBootstrapTrainer::injectAll() {
    if (!vse_) {
        std::cerr << "[VseBootstrapTrainer] VSE is null — cannot inject.\n";
        return 0;
    }

    auto examples = buildExamples_();
    int injected = 0;

    auto& gm = vse_->generativeModel();

    for (const auto& ex : examples) {
        try {
            // Each example updates the GenerativeModel mapping with a fixed
            // bootstrap learning rate. Use a higher rate than live turns
            // (0.15 vs 0.05) since these are clean synthetic priors.
            gm.updateMapping(ex.intent, ex.modality, ex.features, 0.15f);
            ++injected;
        } catch (...) {
            // Non-fatal: skip and continue
        }
    }

    std::cout << "[VseBootstrapTrainer] Injected " << injected
              << "/" << examples.size() << " synthetic examples.\n";

    // === PERMANENT Phase 4: Boost underrepresented classes ===
    // 20 COMMAND examples with strong command + action_cue signals.
    // 20 TUTORIAL examples with strong technical + question signals.
    // These extra examples shift class priors so CLARIFICATION must have
    // a stronger signal to win against COMMAND / TUTORIAL.
    auto neutral = generalFeatures_();

    for (int i = 0; i < 20; ++i) {
        auto obs = neutral;
        obs[3] = 0.9f;                               // [3] command — strong
        obs[9] = 0.85f + 0.01f * (i % 3);           // [9] action_cue — very strong
        obs[7] = 0.2f + 0.05f * (i % 4);            // [7] urgency — mild variation
        obs[2] = 0.0f;                               // [2] question — must be low
        try {
            gm.updateMapping(IntentClass::COMMAND,
                yuki::perception::Modality::TEXT, obs, 0.15f);
            ++injected;
        } catch (...) {}
    }

    for (int i = 0; i < 20; ++i) {
        auto obs = neutral;
        obs[5] = 0.85f + 0.01f * (i % 3);           // [5] technical — strong
        obs[2] = 0.65f + 0.05f * (i % 3);           // [2] question — moderate-high
        obs[9] = 0.3f + 0.02f * (i % 5);            // [9] action_cue — "explain", "show"
        obs[3] = 0.0f;                               // [3] command — must be low
        try {
            gm.updateMapping(IntentClass::TUTORIAL,
                yuki::perception::Modality::TEXT, obs, 0.15f);
            ++injected;
        } catch (...) {}
    }

    std::cout << "[VseBootstrapTrainer] Total with boost: " << injected
              << " examples (160 base + 40 class-balance).\n";
    return injected;

}

} // namespace yuki::inference
