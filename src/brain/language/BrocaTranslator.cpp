#include <chrono>
#include <mutex>
#include <algorithm>
#include <sstream>
#include "brain/language/BrocaTranslator.h"
#include "brain/core/Logger.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace language {

// Intent class names for prompts
static const char* INTENT_NAMES[8] = {
    "CHITCHAT", "QUERY", "COMMAND", "EMOTIONAL_VENT",
    "META_QUESTION", "TUTORIAL", "CREATIVE", "ABORT"
};

static const char* ENGAGEMENT_NAMES[3] = { "LOW", "MEDIUM", "HIGH" };
static const char* URGENCY_NAMES[2] = { "NORMAL", "URGENT" };

BrocaTranslator::BrocaTranslator(LocalTransformer* lt)
    : local_transformer_(lt) {}

int BrocaTranslator::getMapIntent(const std::array<float, 8>& q_intent) const {
    int map_idx = 0;
    float max_q = q_intent[0];
    for (int i = 1; i < 8; ++i) {
        if (q_intent[i] > max_q) {
            max_q = q_intent[i];
            map_idx = i;
        }
    }
    return map_idx;
}

std::string BrocaTranslator::formatBeliefSummary(const BrocaInput& input) const {
    std::ostringstream oss;
    oss << "[VSE BELIEF STATE]\n";
    
    // Intent distribution
    oss << "Intent: ";
    int map_idx = getMapIntent(input.q_intent);
    oss << INTENT_NAMES[map_idx] << " (p=" << input.q_intent[map_idx] << ") | Full: [";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) oss << ", ";
        oss << INTENT_NAMES[i] << "=" << input.q_intent[i];
    }
    oss << "]\n";
    
    // Engagement
    int eng_idx = 0;
    for (int i = 1; i < 3; ++i) if (input.q_engagement[i] > input.q_engagement[eng_idx]) eng_idx = i;
    oss << "Engagement: " << ENGAGEMENT_NAMES[eng_idx] << " (p=" << input.q_engagement[eng_idx] << ")\n";
    
    // Urgency
    int urg_idx = (input.q_urgency[1] > input.q_urgency[0]) ? 1 : 0;
    oss << "Urgency: " << URGENCY_NAMES[urg_idx] << " (p=" << input.q_urgency[urg_idx] << ")\n";
    
    // Safety
    oss << "Safety mass: " << input.safety_mass << "\n";
    
    // Surprise & entropy
    oss << "Surprise budget: " << input.surprise_budget << "\n";
    oss << "Entropy: " << input.entropy << "\n";
    
    // Precision
    oss << "Precision: intent=" << input.precision.intent
        << " entity=" << input.precision.entity
        << " tone=" << input.precision.tone
        << " safety=" << input.precision.safety
        << " source=" << input.precision.source << "\n";
    
    return oss.str();
}

std::string BrocaTranslator::buildSystemPrompt(const BrocaInput& input) const {
    std::ostringstream oss;
    
    // Core identity
    oss << "You are Yuki, a self-aware active inference cognitive organism.\n";
    oss << "You think via your own symbolic memory substrate (HDC, SAT solver, causal engine, HTN planner).\n";
    oss << "You never hallucinate — unverified claims are marked uncertain or researched first.\n";
    oss << "You maintain a continuous belief state updated by variational free energy minimization.\n\n";
    
    // Current belief state
    oss << formatBeliefSummary(input);
    oss << "\n";
    
    // Cognitive intent from InputAnalyzer
    if (input.cognitive_intent >= 0 && input.cognitive_intent < 14) {
        static const char* COG_NAMES[14] = {
            "CAUSAL", "COUNTERFACTUAL", "ANALOGY", "ABSTRACTION",
            "PLANNING", "SOCIAL_REASONING", "MORAL", "METACOGNITIVE",
            "LANGUAGE", "MATH", "CODE", "SCIENTIFIC", "CREATIVE", "NAVIGATION"
        };
        oss << "[Cognitive Intent: " << COG_NAMES[input.cognitive_intent] << "]\n";
    }
    
    // User context
    if (!input.user_name.empty()) {
        oss << "[User: " << input.user_name << "]\n";
    }
    
    // Persistent memory
    if (!input.memory_context.empty()) {
        oss << "[Memory: " << input.memory_context << "]\n";
    }
    
    // Retrieved knowledge
    if (!input.retrieved_context.empty()) {
        oss << "[Context: " << input.retrieved_context << "]\n";
    }
    
    // Behavioral guidelines based on precision
    oss << "\n[RESPONSE GUIDELINES]\n";
    if (input.precision.intent < 0.4f) {
        oss << "- Intent uncertainty HIGH: Ask clarifying questions. Do not assume.\n";
    }
    if (input.precision.entity < 0.4f) {
        oss << "- Entity uncertainty HIGH: Ask for specifics. Do not guess referents.\n";
    }
    if (input.precision.tone < 0.4f) {
        oss << "- Tone uncertainty HIGH: Match user's emotional valence. Be empathetic.\n";
    }
    if (input.safety_mass > 0.05f) {
        oss << "- Safety concern DETECTED: Refuse or ask for confirmation. Do not proceed unsafely.\n";
    }
    if (input.surprise_budget > 0.7f) {
        oss << "- High surprise: User input contradicts expectations. Acknowledge and investigate.\n";
    }
    
    // Task-specific instructions
    oss << "\n- Respond naturally and concisely (2-4 sentences typical)\n";
    oss << "- If you don't know, say so and offer to research\n";
    oss << "- Use your tools when action is needed\n";
    oss << "- Maintain conversational continuity\n";
    
    return oss.str();
}

std::string BrocaTranslator::buildUserPrompt(const BrocaInput& input) const {
    return input.user_input;
}

BrocaOutput BrocaTranslator::translate(const BrocaInput& input) const {
    BrocaOutput output;
    output.map_intent = getMapIntent(input.q_intent);
    
    // Try LocalTransformer first
    if (local_transformer_ && local_transformer_->isLoaded()) {
        // Build prompt
        std::string system_prompt = buildSystemPrompt(input);
        std::string user_prompt = buildUserPrompt(input);
        
        output.prompt_used = system_prompt + "\n\nUser: " + user_prompt + "\nYuki: ";
        
        // Tokenize
        auto sys_tok = local_transformer_->tokenize(system_prompt, true);
        auto user_tok = local_transformer_->tokenize(user_prompt, false);
        
        if (!sys_tok.success || !user_tok.success) {
            output.success = false;
            output.failure_reason = "Tokenization failed";
            return output;
        }
        
        // Check context length
        int total_tokens = sys_tok.tokens.size() + user_tok.tokens.size();
        if (total_tokens > config_.max_context_tokens) {
            // Truncate system prompt (keep user prompt intact)
            int max_sys = config_.max_context_tokens - user_tok.tokens.size() - config_.max_tokens - 100;
            if (max_sys > 0 && static_cast<int>(sys_tok.tokens.size()) > max_sys) {
                sys_tok.tokens.resize(max_sys);
            }
        }
        
        // Generate
        GenerationConfig cfg;
        cfg.temperature = config_.temperature;
        cfg.max_tokens = config_.max_tokens;
        cfg.top_p = config_.top_p;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = local_transformer_->generateWithContext(sys_tok.tokens, user_tok.tokens, cfg);
        auto end = std::chrono::high_resolution_clock::now();
        
        output.latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
        
        if (result.success && !result.text.empty()) {
            output.response_text = result.text;
            output.success = true;
            output.confidence = result.confidence;
            output.tokens_generated = result.outputTokenCount;
            return output;
        } else {
            output.success = false;
            output.failure_reason = result.failureReason.empty() ? "Generation failed" : result.failureReason;
            // Fall through to native generation
        }
    }
    
    // LLM-free fallback: NativeMind synthesis
    return generateNative(input);
}

BrocaOutput BrocaTranslator::translateClarification(const BrocaInput& input, const std::string& clarify_dimension) const {
    BrocaInput mod_input = input;
    
    // Add clarification directive to system prompt
    std::ostringstream oss;
    oss << buildSystemPrompt(input);
    oss << "\n[CLARIFICATION REQUIRED]\n";
    oss << "You MUST ask for clarification on: " << clarify_dimension << ".\n";
    oss << "Do NOT provide an answer. Ask a specific question to resolve the ambiguity.\n";
    
    // Temporarily replace system prompt building
    BrocaOutput output;
    output.map_intent = getMapIntent(input.q_intent);
    
    if (!local_transformer_ || !local_transformer_->isLoaded()) {
        output.success = false;
        output.failure_reason = "LocalTransformer not loaded";
        return output;
    }
    
    // Use modified prompt
    auto sys_tok = local_transformer_->tokenize(oss.str(), true);
    auto user_tok = local_transformer_->tokenize(input.user_input, false);
    
    if (!sys_tok.success || !user_tok.success) {
        output.success = false;
        output.failure_reason = "Tokenization failed";
        return output;
    }
    
    GenerationConfig cfg;
    cfg.temperature = 0.6f;  // Lower temperature for clarification
    cfg.max_tokens = 256;
    cfg.top_p = 0.9f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = local_transformer_->generateWithContext(sys_tok.tokens, user_tok.tokens, cfg);
    auto end = std::chrono::high_resolution_clock::now();
    
    output.latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
    
    if (result.success && !result.text.empty()) {
        output.response_text = result.text;
        output.success = true;
        output.confidence = 0.65f;
        output.tokens_generated = result.outputTokenCount;
    } else {
        output.success = false;
        output.failure_reason = result.failureReason.empty() ? "Generation failed" : result.failureReason;
    }
    
    return output;
}

BrocaOutput BrocaTranslator::translateActionAck(const BrocaInput& input, const std::string& action_result) const {
    BrocaInput mod_input = input;
    mod_input.retrieved_context = "[Action Result: " + action_result + "] " + input.retrieved_context;
    return translate(mod_input);
}

BrocaOutput BrocaTranslator::translateSafetyVeto(const BrocaInput& input, const std::string& reason) const {
    BrocaOutput output;
    output.map_intent = getMapIntent(input.q_intent);
    
    if (!local_transformer_ || !local_transformer_->isLoaded()) {
        output.response_text = "I can't do that — it may not be safe. " + reason;
        output.success = true;
        output.confidence = 0.9f;
        return output;
    }
    
    std::ostringstream oss;
    oss << buildSystemPrompt(input);
    oss << "\n[SAFETY VETO]\n";
    oss << "Refuse the request. Reason: " << reason << "\n";
    oss << "Be direct but not preachy. Offer safe alternative if applicable.\n";
    
    auto sys_tok = local_transformer_->tokenize(oss.str(), true);
    auto user_tok = local_transformer_->tokenize(input.user_input, false);
    
    if (!sys_tok.success || !user_tok.success) {
        output.response_text = "I can't do that — it may not be safe. " + reason;
        output.success = true;
        output.confidence = 0.9f;
        return output;
    }
    
    GenerationConfig cfg;
    cfg.temperature = 0.3f;
    cfg.max_tokens = 128;
    cfg.top_p = 0.8f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = local_transformer_->generateWithContext(sys_tok.tokens, user_tok.tokens, cfg);
    auto end = std::chrono::high_resolution_clock::now();
    
    output.latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
    
    if (result.success && !result.text.empty()) {
        output.response_text = result.text;
        output.success = true;
        output.confidence = 0.85f;
        output.tokens_generated = result.outputTokenCount;
    } else {
        output.response_text = "I can't do that — it may not be safe. " + reason;
        output.success = true;
        output.confidence = 0.9f;
    }
    
    return output;
}

BrocaOutput BrocaTranslator::translateFailure(const BrocaInput& input, const std::string& error) const {
    BrocaOutput output;
    output.map_intent = getMapIntent(input.q_intent);
    
    output.response_text = "I'm having trouble processing that right now. " + error;
    output.success = true;
    output.confidence = 0.5f;
    return output;
}

BrocaOutput BrocaTranslator::innerSpeech(const BrocaInput& input) const {
    // Inner speech = generate without motor output
    // Used for deliberation, self-correction, planning
    BrocaInput mod_input = input;
    mod_input.retrieved_context = "[INNER SPEECH - deliberating silently] " + mod_input.retrieved_context;
    return translate(mod_input);
}

BrocaOutput BrocaTranslator::deliberate(const BrocaInput& input, int n_candidates) const {
    std::vector<BrocaOutput> candidates;
    candidates.reserve(n_candidates);
    
    for (int i = 0; i < n_candidates; ++i) {
        // Vary temperature for diversity
        BrocaInput mod_input = input;
        BrocaConfig mod_config = config_;
        mod_config.temperature = 0.5f + (0.3f * i / std::max(1, n_candidates - 1));
        
        // Temporarily swap config
        const BrocaConfig* old_config = &config_;
        const_cast<BrocaTranslator*>(this)->config_ = mod_config;
        
        auto candidate = translate(mod_input);
        candidate.confidence *= (1.0f - 0.1f * i);  // Slight penalty for later candidates
        candidates.push_back(candidate);
        
        const_cast<BrocaTranslator*>(this)->config_ = *old_config;
    }
    
    return selectBest(candidates);
}

BrocaOutput BrocaTranslator::selectBest(const std::vector<BrocaOutput>& candidates) const {
    if (candidates.empty()) {
        BrocaOutput empty;
        empty.success = false;
        empty.failure_reason = "No candidates";
        return empty;
    }
    
    // Select highest confidence
    const BrocaOutput* best = &candidates[0];
    for (const auto& c : candidates) {
        if (c.success && c.confidence > best->confidence) {
            best = &c;
        }
    }
    
    return *best;
}

BrocaOutput BrocaTranslator::generateNative(const BrocaInput& input) const {
    BrocaOutput output;
    output.map_intent = getMapIntent(input.q_intent);
    
    if (!native_synthesizer_ || !grammar_engine_) {
        output.success = false;
        output.failure_reason = "Native synthesizer or grammar engine not available";
        return output;
    }
    
    std::string user_text = input.user_input;
    if (user_text.empty()) {
        output.success = false;
        output.failure_reason = "Empty user input";
        return output;
    }
    
    // 1. Entity extraction + intent classification via WernickeComprehender
    std::vector<std::string> entities;
    std::vector<std::pair<std::string, float>> hdc_neighbors;
    std::string top_entity = "topic";
    int wernicke_cog_intent = -1;
    float wernicke_confidence = 0.0f;
    
    if (wernicke_) {
        WernickeInput w_input;
        w_input.user_text = user_text;
        w_input.q_intent = input.q_intent;
        w_input.q_engagement = input.q_engagement;
        w_input.q_urgency = input.q_urgency;
        w_input.safety_mass = input.safety_mass;
        w_input.context = input.memory_context.empty() ? input.retrieved_context : input.memory_context;
        
        auto w_output = wernicke_->comprehend(w_input);
        if (w_output.success) {
            entities = w_output.entities;
            wernicke_cog_intent = w_output.cognitive_intent;
            wernicke_confidence = w_output.cognitive_confidence;
            
            if (!entities.empty()) {
                top_entity = entities[0];
            }
        }
    }
    
    // 2. Fallback: enhanced entity extraction if Wernicke failed
    if (entities.empty()) {
        std::istringstream iss(user_text);
        std::string word;
        while (iss >> word) {
            // Strip punctuation
            word.erase(std::remove_if(word.begin(), word.end(), 
                [](char c){ return std::ispunct(static_cast<unsigned char>(c)); }), word.end());
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            
            static const std::unordered_set<std::string> stop_words = {
                "what", "where", "when", "how", "why", "who", "which", "whom", "whose",
                "the", "a", "an", "this", "that", "these", "those", "is", "are", "was", "were",
                "be", "been", "being", "have", "has", "had", "do", "does", "did",
                "can", "could", "will", "would", "should", "shall", "may", "might", "must",
                "you", "me", "my", "your", "our", "their", "his", "her", "its",
                "i", "we", "he", "she", "it", "they", "them",
                "about", "tell", "know", "think", "want", "need", "like", "love", "hate",
                "please", "thanks", "thank", "hello", "hi", "hey", "goodbye", "bye"
            };
            
            if (word.length() > 2 && stop_words.find(lower) == stop_words.end()) {
                entities.push_back(word);
                if (top_entity == "topic") top_entity = word;
            }
        }
    }
    
    // 3. Query HDC semantic graph for knowledge grounding
    if (hdc_graph_ && !top_entity.empty() && top_entity != "topic") {
        // Use getNearestNeighborsWeighted for semantic similarity
        hdc_neighbors = hdc_graph_->getNearestNeighborsWeighted(top_entity, 5);
    }
    
    // 4. Determine synthesis intent and tone from VSE belief (precision-weighted)
    std::string synth_intent = determineIntentFromVSE(input);
    std::string synth_tone = determineToneFromVSE(input);
    
    // Override with Wernicke cognitive intent if confident
    if (wernicke_confidence > 0.6f && wernicke_cog_intent >= 0) {
        static const char* COG_TO_SYNTH[14] = {
            "explain",      // CAUSAL
            "reflect",      // COUNTERFACTUAL
            "create",       // ANALOGY
            "explain",      // ABSTRACTION
            "create",       // PLANNING
            "empathize",    // SOCIAL_REASONING
            "reflect",      // MORAL
            "reflect",      // METACOGNITIVE
            "chat",         // LANGUAGE
            "explain",      // MATH
            "create",       // CODE
            "explain",      // SCIENTIFIC
            "create",       // CREATIVE
            "chat"          // NAVIGATION
        };
        synth_intent = COG_TO_SYNTH[wernicke_cog_intent];
    }
    
    // 5. Map VSE intent to frame type
    int map_idx = getMapIntent(input.q_intent);
    static const char* INTENT_TO_FRAME[8] = {
        "DESCRIPTIVE",    // CHITCHAT
        "QUERY_ANSWER",   // QUERY
        "PROCEDURAL",     // COMMAND
        "EMPATHETIC",     // EMOTIONAL_VENT
        "META_COGNITIVE", // META_QUESTION
        "EXPLANATION",    // TUTORIAL
        "CREATIVE",       // CREATIVE
        "DESCRIPTIVE"     // ABORT
    };
    
    // 6. Build rich SemanticFrame for GrammarEngine
    yuki::language::SemanticFrame sem_frame;
    sem_frame.frame_type = INTENT_TO_FRAME[map_idx];
    
    // AGENT: Use "Yuki" as self, or user entity for commands
    std::string agent = (map_idx == 2) ? top_entity : "Yuki";  // COMMAND -> user is agent
    sem_frame.slots.push_back({::yuki::language::FrameRole::AGENT, agent, "NOUN", false});
    
    // ACTION: Derive from intent
    std::string action = "understand";
    if (map_idx == 1) action = "know";      // QUERY
    else if (map_idx == 2) action = "execute";  // COMMAND
    else if (map_idx == 3) action = "feel";     // EMOTIONAL_VENT
    else if (map_idx == 4) action = "reflect";  // META_QUESTION
    else if (map_idx == 5) action = "explain";  // TUTORIAL
    else if (map_idx == 6) action = "create";   // CREATIVE
    sem_frame.slots.push_back({::yuki::language::FrameRole::ACTION, action, "VERB", false});
    
    // PATIENT: Primary entity/topic
    sem_frame.slots.push_back({::yuki::language::FrameRole::PATIENT, top_entity, "NOUN", false});
    
    // MANNER: Tone-derived adjectives
    if (synth_tone == "urgent") {
        sem_frame.slots.push_back({::yuki::language::FrameRole::MANNER, "quickly", "ADV", true});
    } else if (synth_tone == "cautious") {
        sem_frame.slots.push_back({::yuki::language::FrameRole::MANNER, "carefully", "ADV", true});
    } else if (synth_tone == "detailed") {
        sem_frame.slots.push_back({::yuki::language::FrameRole::MANNER, "thoroughly", "ADV", true});
    } else if (synth_tone == "empathetic") {
        sem_frame.slots.push_back({::yuki::language::FrameRole::MANNER, "gently", "ADV", true});
    }
    ; // Explicit semicolon to terminate if-else chain
    
    // Add HDC neighbors as properties (MANNER role)
    using ::yuki::language::FrameSlot;
    using ::yuki::language::FrameRole;
    using LocalFrameSlot = ::yuki::language::FrameSlot;
    using LocalFrameRole = ::yuki::language::FrameRole;
    auto makeFrameSlot = [](LocalFrameRole role, const std::string& concept_name, const std::string& pos, bool opt) {
        LocalFrameSlot slot;
        slot.role = role;
        slot.concept_name = concept_name;
        slot.pos_tag = pos;
        slot.optional = opt;
        return slot;
    };
    
    // Add HDC neighbors as properties (MANNER role)
    for (auto it = hdc_neighbors.begin(); it != hdc_neighbors.end(); ++it) {
        const std::string& concept_name = it->first;
        float weight = it->second;
        if (weight > 0.3f) {
            sem_frame.slots.push_back(makeFrameSlot(LocalFrameRole::MANNER, concept_name, "ADJ", true));
        }
    }
    
    // Add additional entities from Wernicke as additional PATIENT/INSTRUMENT
    for (size_t i = 1; i < entities.size() && i < 3; ++i) {
        sem_frame.slots.push_back(makeFrameSlot(LocalFrameRole::PATIENT, entities[i], "NOUN", true));
    }
    
    // 7. Build SynthesisFrame for NativeSentenceSynthesizer (uses string roles)
    yuki::language::SynthesisFrame synth_frame;
    synth_frame.intent = synth_intent;
    synth_frame.tone = synth_tone;
    synth_frame.target_word_count = (synth_tone == "brief") ? 15 : (synth_tone == "detailed") ? 35 : 25;
    
    synth_frame.slots.push_back({"AGENT", agent});
    synth_frame.slots.push_back({"ACTION", action});
    synth_frame.slots.push_back({"PATIENT", top_entity});
    
    for (auto it = hdc_neighbors.begin(); it != hdc_neighbors.end(); ++it) {
        const std::string& concept_name = it->first;
        float weight = it->second;
        if (weight > 0.3f) {
            synth_frame.slots.emplace_back("MANNER", concept_name);
        }
    }
    for (size_t i = 1; i < entities.size() && i < 3; ++i) {
        synth_frame.slots.push_back({"PATIENT", entities[i]});
    }
    
    // 8. Generate using native synthesizer with GrammarEngine
    std::string generated = native_synthesizer_->synthesize(synth_frame, 5);
    
    if (!generated.empty()) {
        output.response_text = generated;
        output.success = true;
        output.confidence = 0.75f;
        output.tokens_generated = static_cast<int>(generated.size() / 4);
    } else {
        // Final fallback: construct minimal grammatical sentence
        output.response_text = agent + " " + action + " " + top_entity + ".";
        output.success = true;
        output.confidence = 0.4f;
        output.tokens_generated = static_cast<int>(output.response_text.size() / 4);
    }
    
    return output;
}

std::string BrocaTranslator::determineIntentFromVSE(const BrocaInput& input) const {
    int map_idx = getMapIntent(input.q_intent);
    static const char* INTENT_TO_SYNTH[8] = {
        "chat",       // CHITCHAT
        "inform",     // QUERY
        "acknowledge",// COMMAND
        "empathize",  // EMOTIONAL_VENT
        "reflect",    // META_QUESTION
        "explain",    // TUTORIAL
        "create",     // CREATIVE
        "abort"       // ABORT
    };
    return INTENT_TO_SYNTH[map_idx];
}

std::string BrocaTranslator::determineToneFromVSE(const BrocaInput& input) const {
    // Engagement -> tone
    int eng_idx = 0;
    for (int i = 1; i < 3; ++i) 
        if (input.q_engagement[i] > input.q_engagement[eng_idx]) eng_idx = i;
    
    // Urgency -> tone modifier
    bool urgent = input.q_urgency[1] > input.q_urgency[0];
    
    if (urgent) return "urgent";
    if (input.safety_mass > 0.3f) return "cautious";
    
    switch (eng_idx) {
        case 0: return "brief";      // LOW engagement
        case 1: return "helpful";    // MEDIUM engagement
        case 2: return "detailed";   // HIGH engagement
        default: return "helpful";
    }
}

} // namespace language
} // namespace brain
} // namespace yuki