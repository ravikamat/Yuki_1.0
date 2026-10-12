#pragma once
#include "LanguageBootstrap.h"
#include "brain/core/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace yuki {
namespace brain {
namespace language {

using json = nlohmann::json;

LanguageBootstrap::LanguageBootstrap(HdcSemanticGraph* graph) : graph_(graph) {}

LanguageBootstrap::~LanguageBootstrap() = default;

bool LanguageBootstrap::initialize(const std::filesystem::path& bootstrap_file) {
    if (!graph_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LanguageBootstrap",
            "HdcSemanticGraph not provided");
        return false;
    }

    loadBuiltinBootstrap();

    if (!bootstrap_file.empty() && std::filesystem::exists(bootstrap_file)) {
        return loadFromFile(bootstrap_file);
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LanguageBootstrap",
        "Initialized with " + std::to_string(word_bindings_.size()) + " word bindings, " +
        std::to_string(concept_bindings_.size()) + " concept bindings, " +
        std::to_string(grammar_patterns_.size()) + " grammar patterns");
    return true;
}

bool LanguageBootstrap::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "LanguageBootstrap",
            "Bootstrap file not found: " + path.string());
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LanguageBootstrap",
            "JSON parse error: " + std::string(e.what()));
        return false;
    }

    // Load word bindings
    if (j.contains("words") && j["words"].is_array()) {
        for (const auto& w : j["words"]) {
            WordBinding b;
            b.word = w.value("word", "");
            b.concept_id = w.value("concept_id", "");
            b.weight = w.value("weight", 1.0f);
            b.source = w.value("source", "bootstrap");
            b.created_at = std::chrono::system_clock::now();
            b.confidence = w.value("confidence", 100);
            if (!b.word.empty() && !b.concept_id.empty()) {
                word_bindings_[b.word] = b;
                bindInHDC(b);
            }
        }
    }

    // Load concept bindings
    if (j.contains("concepts") && j["concepts"].is_array()) {
        for (const auto& c : j["concepts"]) {
            ConceptBinding cbind;
            cbind.concept_id = c.value("concept_id", "");
            cbind.concept_name = c.value("concept_name", "");
            cbind.word_forms = c.value("word_forms", std::vector<std::string>{});
            cbind.synonyms = c.value("synonyms", std::vector<std::string>{});
            cbind.related_concepts = c.value("related_concepts", std::vector<std::string>{});
            cbind.definition = c.value("definition", "");
            cbind.examples = c.value("examples", std::vector<std::string>{});
            if (c.contains("properties") && c["properties"].is_object()) {
                for (auto& [k, v] : c["properties"].items()) {
                    cbind.properties[k] = v.get<float>();
                }
            }
            cbind.created_at = std::chrono::system_clock::now();
            cbind.confidence = c.value("confidence", 100);
            if (!cbind.concept_id.empty()) {
                concept_bindings_[cbind.concept_id] = cbind;
                bindConceptInHDC(cbind);
            }
        }
    }

    // Load grammar patterns
    if (j.contains("grammar") && j["grammar"].is_array()) {
        for (const auto& g : j["grammar"]) {
            GrammarPattern gp;
            gp.pattern_id = g.value("pattern_id", "");
            gp.pattern = g.value("pattern", "");
            gp.meaning = g.value("meaning", "");
            gp.examples = g.value("examples", std::vector<std::string>{});
            gp.confidence = g.value("confidence", 1.0f);
            gp.source = g.value("source", "bootstrap");
            if (!gp.pattern_id.empty()) {
                grammar_patterns_[gp.pattern_id] = gp;
                bindGrammarInHDC(gp);
            }
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LanguageBootstrap",
        "Loaded from file: " + path.string() + " (" + std::to_string(word_bindings_.size()) + " words)");
    return true;
}

bool LanguageBootstrap::saveToFile(const std::filesystem::path& path) const {
    json j;

    for (const auto& [_, b] : word_bindings_) {
        json w;
        w["word"] = b.word;
        w["concept_id"] = b.concept_id;
        w["weight"] = b.weight;
        w["source"] = b.source;
        w["confidence"] = b.confidence;
        j["words"].push_back(w);
    }

    for (const auto& [_, c] : concept_bindings_) {
        json cj;
        cj["concept_id"] = c.concept_id;
        cj["concept_name"] = c.concept_name;
        cj["word_forms"] = c.word_forms;
        cj["synonyms"] = c.synonyms;
        cj["related_concepts"] = c.related_concepts;
        cj["definition"] = c.definition;
        cj["examples"] = c.examples;
        cj["properties"] = c.properties;
        cj["confidence"] = c.confidence;
        j["concepts"].push_back(cj);
    }

    for (const auto& [_, g] : grammar_patterns_) {
        json gj;
        gj["pattern_id"] = g.pattern_id;
        gj["pattern"] = g.pattern;
        gj["meaning"] = g.meaning;
        gj["examples"] = g.examples;
        gj["confidence"] = g.confidence;
        gj["source"] = g.source;
        j["grammar"].push_back(gj);
    }

    std::ofstream file(path);
    if (!file) return false;
    file << j.dump(2);
    return true;
}

std::optional<WordBinding> LanguageBootstrap::lookupWord(const std::string& word) const {
    auto it = word_bindings_.find(word);
    if (it != word_bindings_.end()) return it->second;
    return std::nullopt;
}

std::optional<ConceptBinding> LanguageBootstrap::lookupConcept(const std::string& concept_id) const {
    auto it = concept_bindings_.find(concept_id);
    if (it != concept_bindings_.end()) return it->second;
    return std::nullopt;
}

std::optional<GrammarPattern> LanguageBootstrap::lookupGrammar(const std::string& pattern) const {
    auto it = grammar_patterns_.find(pattern);
    if (it != grammar_patterns_.end()) return it->second;
    return std::nullopt;
}

bool LanguageBootstrap::bindWord(const std::string& word, const std::string& concept_id, float weight, const std::string& source) {
    WordBinding binding;
    binding.word = word;
    binding.concept_id = concept_id;
    binding.weight = weight;
    binding.source = source;
    binding.created_at = std::chrono::system_clock::now();
    binding.confidence = (source == "correction" || source == "episodic") ? 100 : 80;

    word_bindings_[word] = binding;
    bindInHDC(binding);

    // Also update concept binding
    auto it = concept_bindings_.find(concept_id);
    if (it != concept_bindings_.end()) {
        it->second.word_forms.push_back(word);
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LanguageBootstrap",
        "Bound word: " + word + " -> " + concept_id + " (" + source + ")");
    return true;
}

bool LanguageBootstrap::unbindWord(const std::string& word) {
    auto it = word_bindings_.find(word);
    if (it == word_bindings_.end()) return false;

    unbindInHDC(word);
    word_bindings_.erase(it);
    return true;
}

bool LanguageBootstrap::bindConcept(const std::string& concept_id, const ConceptBinding& concept_binding) {
    concept_bindings_[concept_id] = concept_binding;
    bindConceptInHDC(concept_binding);
    return true;
}

bool LanguageBootstrap::unbindConcept(const std::string& concept_id) {
    auto it = concept_bindings_.find(concept_id);
    if (it == concept_bindings_.end()) return false;
    concept_bindings_.erase(it);
    return true;
}

bool LanguageBootstrap::bindGrammar(const GrammarPattern& pattern) {
    grammar_patterns_[pattern.pattern_id] = pattern;
    bindGrammarInHDC(pattern);
    return true;
}

bool LanguageBootstrap::unbindGrammar(const std::string& pattern_id) {
    auto it = grammar_patterns_.find(pattern_id);
    if (it == grammar_patterns_.end()) return false;
    grammar_patterns_.erase(it);
    return true;
}

void LanguageBootstrap::loadWordBindings(const std::vector<WordBinding>& bindings) {
    for (const auto& b : bindings) {
        word_bindings_[b.word] = b;
        bindInHDC(b);
    }
}

void LanguageBootstrap::loadConceptBindings(const std::vector<ConceptBinding>& concepts) {
    for (const auto& c : concepts) {
        concept_bindings_[c.concept_id] = c;
        bindConceptInHDC(c);
    }
}

void LanguageBootstrap::loadGrammarPatterns(const std::vector<GrammarPattern>& patterns) {
    for (const auto& g : patterns) {
        grammar_patterns_[g.pattern_id] = g;
        bindGrammarInHDC(g);
    }
}

std::vector<std::string> LanguageBootstrap::getWordsForConcept(const std::string& concept_id) const {
    std::vector<std::string> words;
    for (const auto& [word, binding] : word_bindings_) {
        if (binding.concept_id == concept_id) words.push_back(word);
    }
    return words;
}

std::vector<std::string> LanguageBootstrap::getConceptsForWord(const std::string& word) const {
    std::vector<std::string> concepts;
    auto it = word_bindings_.find(word);
    if (it != word_bindings_.end()) {
        concepts.push_back(it->second.concept_id);
    }
    return concepts;
}

std::vector<std::string> LanguageBootstrap::getSynonyms(const std::string& word) const {
    std::vector<std::string> synonyms;
    auto word_it = word_bindings_.find(word);
    if (word_it == word_bindings_.end()) return synonyms;

    auto concept_it = concept_bindings_.find(word_it->second.concept_id);
    if (concept_it != concept_bindings_.end()) {
        for (const auto& syn : concept_it->second.synonyms) {
            if (syn != word) synonyms.push_back(syn);
        }
        for (const auto& form : concept_it->second.word_forms) {
            if (form != word) synonyms.push_back(form);
        }
    }
    return synonyms;
}

std::vector<std::string> LanguageBootstrap::getRelatedConcepts(const std::string& concept_id) const {
    auto it = concept_bindings_.find(concept_id);
    if (it != concept_bindings_.end()) {
        return it->second.related_concepts;
    }
    return {};
}

std::optional<GrammarPattern> LanguageBootstrap::matchGrammar(const std::string& text) const {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& [_, pattern] : grammar_patterns_) {
        // Simple pattern matching - in production use regex
        std::string pattern_lower = pattern.pattern;
        std::transform(pattern_lower.begin(), pattern_lower.end(), pattern_lower.begin(), ::tolower);

        // Replace placeholders
        if (pattern_lower.find("verb") != std::string::npos || pattern_lower.find("noun") != std::string::npos) {
            // Check if words in text match pattern structure
            return pattern;
        }
    }
    return std::nullopt;
}

std::vector<std::string> LanguageBootstrap::findSimilarWords(const std::vector<float>& /*embedding*/, size_t k) const {
    std::vector<std::string> similar;
    // In production: use HNSW or cosine similarity on word embeddings
    // For now return top words by confidence
    std::vector<std::pair<std::string, int>> scored;
    for (const auto& [word, binding] : word_bindings_) {
        scored.emplace_back(word, binding.confidence);
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < std::min(k, scored.size()); ++i) {
        similar.push_back(scored[i].first);
    }
    return similar;
}

std::vector<std::string> LanguageBootstrap::findSimilarConcepts(const std::vector<float>& /*embedding*/, size_t k) const {
    std::vector<std::string> similar;
    std::vector<std::pair<std::string, int>> scored;
    for (const auto& [id, binding] : concept_bindings_) {
        scored.emplace_back(id, binding.confidence);
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < std::min(k, scored.size()); ++i) {
        similar.push_back(scored[i].first);
    }
    return similar;
}

void LanguageBootstrap::bindInHDC(const WordBinding& binding) {
    if (!graph_) return;
    graph_->ingestProposition(binding.word, "maps_to", binding.concept_id, binding.weight);
}

void LanguageBootstrap::unbindInHDC(const std::string& /*word*/) {
    if (!graph_) return;
    // In production: remove HDC binding
}

void LanguageBootstrap::bindConceptInHDC(const ConceptBinding& concept_binding) {
    if (!graph_) return;
    for (const auto& word : concept_binding.word_forms) {
        graph_->ingestProposition(word, "is_form_of", concept_binding.concept_id, 0.9f);
    }
    for (const auto& syn : concept_binding.synonyms) {
        graph_->ingestProposition(syn, "synonym_of", concept_binding.concept_id, 0.8f);
    }
    for (const auto& rel : concept_binding.related_concepts) {
        graph_->ingestProposition(concept_binding.concept_id, "related_to", rel, 0.7f);
    }
    for (const auto& [prop, val] : concept_binding.properties) {
        graph_->ingestProposition(concept_binding.concept_id, "has_property", prop, val);
    }
}

void LanguageBootstrap::bindGrammarInHDC(const GrammarPattern& pattern) {
    if (!graph_) return;
    graph_->ingestProposition(pattern.pattern_id, "is_grammar", pattern.meaning, pattern.confidence);
    for (const auto& ex : pattern.examples) {
        graph_->ingestProposition(pattern.pattern_id, "has_example", ex, 0.9f);
    }
}

void LanguageBootstrap::loadBuiltinBootstrap() {
    loadWordBindings(builtinWordBindings());
    loadConceptBindings(builtinConceptBindings());
    loadGrammarPatterns(builtinGrammarPatterns());
}

std::vector<WordBinding> LanguageBootstrap::builtinWordBindings() {
    return {
        // Core actions
        {"generate", "action_create", 0.9f, "bootstrap"},
        {"create", "action_create", 0.95f, "bootstrap"},
        {"make", "action_create", 0.9f, "bootstrap"},
        {"build", "action_build", 0.9f, "bootstrap"},
        {"produce", "action_produce", 0.85f, "bootstrap"},
        {"earn", "action_earn", 0.9f, "bootstrap"},
        {"make_money", "action_earn", 0.9f, "bootstrap"},
        {"get", "action_get", 0.8f, "bootstrap"},
        {"receive", "action_receive", 0.85f, "bootstrap"},
        {"credit", "action_credit", 0.85f, "bootstrap"},
        {"deposit", "action_deposit", 0.9f, "bootstrap"},

        // Financial
        {"income", "resource_money", 0.95f, "bootstrap"},
        {"money", "resource_money", 0.95f, "bootstrap"},
        {"cash", "resource_money", 0.9f, "bootstrap"},
        {"funds", "resource_money", 0.85f, "bootstrap"},
        {"salary", "resource_salary", 0.9f, "bootstrap"},
        {"revenue", "resource_revenue", 0.9f, "bootstrap"},
        {"profit", "resource_profit", 0.9f, "bootstrap"},
        {"account", "container_account", 0.9f, "bootstrap"},
        {"bank", "institution_bank", 0.95f, "bootstrap"},
        {"wallet", "container_wallet", 0.9f, "bootstrap"},
        {"balance", "property_balance", 0.85f, "bootstrap"},
        {"transaction", "action_transaction", 0.85f, "bootstrap"},
        {"transfer", "action_transfer", 0.9f, "bootstrap"},
        {"withdraw", "action_withdraw", 0.9f, "bootstrap"},

        // Time
        {"week", "time_week", 0.95f, "bootstrap"},
        {"day", "time_day", 0.95f, "bootstrap"},
        {"month", "time_month", 0.95f, "bootstrap"},
        {"year", "time_year", 0.9f, "bootstrap"},
        {"hour", "time_hour", 0.9f, "bootstrap"},
        {"minute", "time_minute", 0.8f, "bootstrap"},

        // Quantities
        {"dollar", "unit_dollar", 0.95f, "bootstrap"},
        {"usd", "unit_dollar", 0.95f, "bootstrap"},
        {"1000", "quantity_1000", 0.9f, "bootstrap"},

        // Intent markers
        {"me", "self_recipient", 0.95f, "bootstrap"},
        {"my", "self_possessive", 0.9f, "bootstrap"},
        {"to", "direction_to", 0.8f, "bootstrap"},
        {"in", "direction_in", 0.8f, "bootstrap"},
        {"into", "direction_into", 0.85f, "bootstrap"},

        // Modality
        {"want", "intent_desire", 0.9f, "bootstrap"},
        {"need", "intent_need", 0.95f, "bootstrap"},
        {"must", "intent_must", 0.9f, "bootstrap"},
        {"can", "intent_can", 0.8f, "bootstrap"},
        {"please", "polite_request", 0.8f, "bootstrap"},
    };
}

std::vector<ConceptBinding> LanguageBootstrap::builtinConceptBindings() {
    std::vector<ConceptBinding> bindings;
    bindings.reserve(12);
    
    bindings.emplace_back(ConceptBinding{
        "action_create", "create", {"create", "creates", "creating", "created"},
        {"make", "build", "produce", "generate", "construct"},
        {"action_make", "action_build", "action_produce", "resource_thing"},
        "To bring something into existence",
        {"create a file", "create an account", "create a plan"},
        {{"action", 0.95f}, {"intentional", 0.9f}, {"result", 0.8f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "action_earn", "earn", {"earn", "earns", "earning", "earned"},
        {"make_money", "gain", "acquire", "receive"},
        {"action_create", "resource_money", "action_work", "resource_value"},
        "To receive money in exchange for work or value provided",
        {"earn $1000", "earn a living", "earn interest"},
        {{"action", 0.95f}, {"financial", 0.9f}, {"reciprocal", 0.8f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "resource_money", "money", {"money", "cash", "funds", "currency"},
        {"income", "salary", "revenue", "profit", "wealth"},
        {"action_earn", "resource_value", "container_account", "institution_bank"},
        "Medium of exchange, measure of value, store of wealth",
        {"earn money", "save money", "spend money"},
        {{"financial", 0.95f}, {"fungible", 0.9f}, {"liquid", 0.85f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "container_account", "account", {"account", "accounts", "acct"},
        {"wallet", "bank_account", "ledger"},
        {"resource_money", "institution_bank", "property_balance", "action_deposit", "action_withdraw"},
        "A record of financial transactions and balance",
        {"bank account", "trading account", "savings account"},
        {{"container", 0.95f}, {"financial", 0.9f}, {"digital", 0.8f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "action_credit", "credit", {"credit", "credits", "credited", "crediting"},
        {"deposit", "add_funds", "fund"},
        {"resource_money", "container_account", "action_deposit", "direction_into"},
        "To add funds to an account",
        {"credit $1000 to my account", "credit the payment"},
        {{"action", 0.95f}, {"financial", 0.9f}, {"direction_into", 0.9f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "time_week", "week", {"week", "weeks", "weekly"},
        {"seven_days", "work_week"},
        {"time_day", "time_month", "property_duration"},
        "A period of seven days",
        {"in a week", "weekly income", "per week"},
        {{"time", 0.95f}, {"duration", 0.9f}, {"recurring", 0.8f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "unit_dollar", "dollar", {"dollar", "dollars", "usd", "$"},
        {"buck", "greenback"},
        {"resource_money", "quantity_1000", "property_value"},
        "US currency unit",
        {"$1000", "1000 dollars", "USD 1000"},
        {{"financial", 0.95f}, {"unit", 0.9f}, {"us_currency", 0.9f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "quantity_1000", "thousand", {"1000", "1k", "thousand", "grand"},
        {"k", "kilo"},
        {"unit_dollar", "property_magnitude", "time_week", "resource_money"},
        "Quantity of one thousand",
        {"$1000", "1k views", "1000 users"},
        {{"magnitude", 0.95f}, {"count", 0.9f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "self_recipient", "me", {"me", "myself"},
        {"my", "mine"},
        {"self_possessive", "intent_desire", "action_credit"},
        "First person singular object pronoun",
        {"give me", "credit me", "tell me"},
        {{"person", 1.0f}, {"object", 0.9f}, {"recipient", 0.95f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "direction_into", "into", {"into", "in", "to"},
        {"toward", "within"},
        {"direction_in", "direction_to", "action_credit", "container_account"},
        "Indicates movement toward inside",
        {"put into", "credit into", "deposit into"},
        {{"direction", 0.95f}, {"containment", 0.8f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "intent_desire", "want", {"want", "wants", "wanted", "wanting"},
        {"desire", "wish", "need", "would_like"},
        {"intent_need", "action_create", "resource_thing"},
        "Expresses desire for something",
        {"I want $1000", "want to earn", "want an account"},
        {{"intent", 0.95f}, {"volitional", 0.9f}},
        {}, 100
    });
    
    bindings.emplace_back(ConceptBinding{
        "intent_need", "need", {"need", "needs", "needed", "needing"},
        {"require", "must_have", "essential"},
        {"intent_desire", "resource_required", "priority_high"},
        "Expresses requirement or necessity",
        {"I need money", "need to earn", "need an account"},
        {{"intent", 0.95f}, {"necessity", 0.95f}, {"urgency", 0.8f}},
        {}, 100
    });
    
    return bindings;
}

std::vector<GrammarPattern> LanguageBootstrap::builtinGrammarPatterns() {
    std::vector<GrammarPattern> patterns;
    patterns.reserve(6);
    
    patterns.emplace_back(GrammarPattern{
        "verb_me_noun",
        "verb + me + noun",
        "action_recipient_object",
        {"generate me $1000", "give me money", "send me the file", "build me a bot"},
        0.95f, "bootstrap"
    });
    
    patterns.emplace_back(GrammarPattern{
        "verb_noun_into_noun",
        "verb + noun + into + noun",
        "action_object_destination",
        {"credit $1000 into my account", "deposit money into account", "move file into folder"},
        0.9f, "bootstrap"
    });
    
    patterns.emplace_back(GrammarPattern{
        "want_to_verb",
        "want + to + verb",
        "desire_action",
        {"want to earn", "want to create", "want to build"},
        0.9f, "bootstrap"
    });
    
    patterns.emplace_back(GrammarPattern{
        "need_to_verb",
        "need + to + verb",
        "necessity_action",
        {"need to earn", "need to create", "need to build"},
        0.95f, "bootstrap"
    });
    
    patterns.emplace_back(GrammarPattern{
        "generate_me_quantity_noun",
        "generate + me + quantity + noun",
        "request_generate_resource",
        {"generate me 1000 dollars", "generate me $1000", "generate me income"},
        0.95f, "bootstrap"
    });
    
    patterns.emplace_back(GrammarPattern{
        "credit_quantity_into_account",
        "credit + quantity + into + account",
        "action_credit_destination",
        {"credit 1000 dollars into my account", "credit $1000 into account"},
        0.95f, "bootstrap"
    });
    
    return patterns;
}

} // namespace language
} // namespace brain
} // namespace yuki