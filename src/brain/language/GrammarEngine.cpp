#include "GrammarEngine.h"
#include "brain/learning/FastText.h"
#include "brain/memory/ConceptNetIngestor.h"
#include "brain/core/Logger.h"
#include "brain/core/RandomEngine.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <cctype>

namespace yuki::language {

static FrameRole parseRole(const std::string& r) {
    if (r == "AGENT") return FrameRole::AGENT;
    if (r == "PATIENT") return FrameRole::PATIENT;
    if (r == "ACTION") return FrameRole::ACTION;
    if (r == "INSTRUMENT") return FrameRole::INSTRUMENT;
    if (r == "LOCATION") return FrameRole::LOCATION;
    if (r == "TIME") return FrameRole::TIME;
    if (r == "MANNER") return FrameRole::MANNER;
    if (r == "CAUSE") return FrameRole::CAUSE;
    if (r == "RESULT") return FrameRole::RESULT;
    return FrameRole::AGENT;
}

GrammarEngine::GrammarEngine(
    const yuki::learning::FastText* ft,
    const memory::ConceptNetIngestor* conceptnet)
    : ft_(ft), conceptnet_(conceptnet)
{
    loadFrames("data/grammar_frames.txt");
    loadRules("data/syntactic_rules.txt");
    loadLexicon("data/lexicon.txt");
}

GrammarEngine::~GrammarEngine() = default;

bool GrammarEngine::loadFrames(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
            "GrammarEngine failed to open frames file: " + filepath);
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string frame_type;
        if (!std::getline(ss, frame_type, '|')) continue;

        // Trim whitespace
        frame_type.erase(0, frame_type.find_first_not_of(" \t"));
        frame_type.erase(frame_type.find_last_not_of(" \t") + 1);

        SemanticFrame frame;
        frame.frame_type = frame_type;

        std::string token;
        while (std::getline(ss, token, '|')) {
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            if (token.find('=') != std::string::npos) {
                std::size_t eq = token.find('=');
                std::string k = token.substr(0, eq);
                std::string v = token.substr(eq + 1);
                frame.constraints[k] = v;
            } else if (token.find(':') != std::string::npos) {
                std::stringstream tss(token);
                std::string name, role_str, pos, concept_var, opt_str;
                std::getline(tss, name, ':');
                std::getline(tss, role_str, ':');
                std::getline(tss, pos, ':');
                std::getline(tss, concept_var, ':');
                std::getline(tss, opt_str, ':');

                FrameSlot slot;
                slot.role = parseRole(role_str);
                slot.pos_tag = pos;
                slot.concept_name = concept_var;
                slot.optional = (opt_str == "1");
                frame.slots.push_back(slot);
            }
        }

        frame_templates_[frame_type] = frame;
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "GrammarEngine loaded " + std::to_string(frame_templates_.size()) + " frame templates.");
    return true;
}

bool GrammarEngine::loadRules(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
            "GrammarEngine failed to open rules file: " + filepath);
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string lhs, rhs_str, prob_str, constraint_str;

        if (std::getline(ss, lhs, '|') && std::getline(ss, rhs_str, '|')) {
            std::getline(ss, prob_str, '|');
            std::getline(ss, constraint_str, '|');

            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t"));
                if (!s.empty()) s.erase(s.find_last_not_of(" \t") + 1);
            };

            trim(lhs);
            trim(rhs_str);
            trim(prob_str);
            trim(constraint_str);

            PcfgRule rule;
            rule.lhs = lhs;
            try {
                if (!prob_str.empty()) rule.probability = std::stof(prob_str);
            } catch (...) {
                rule.probability = 1.0f;
            }

            std::stringstream rhs_ss(rhs_str);
            std::string sym;
            while (rhs_ss >> sym) {
                rule.rhs.push_back(sym);
            }

            if (!constraint_str.empty() && constraint_str.find('=') != std::string::npos) {
                std::size_t eq = constraint_str.find('=');
                std::string k = constraint_str.substr(0, eq);
                std::string v = constraint_str.substr(eq + 1);
                rule.constraints[k] = v;
            }

            rules_[lhs].push_back(rule);
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "GrammarEngine loaded PCFG rules for " + std::to_string(rules_.size()) + " non-terminals.");
    return true;
}

bool GrammarEngine::loadLexicon(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
            "GrammarEngine failed to open lexicon file: " + filepath);
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string word, pos, tags_str, freq_str, level_str;

        if (std::getline(ss, word, '|') && std::getline(ss, pos, '|')) {
            std::getline(ss, tags_str, '|');
            std::getline(ss, freq_str, '|');
            std::getline(ss, level_str, '|');

            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t"));
                if (!s.empty()) s.erase(s.find_last_not_of(" \t") + 1);
            };

            trim(word); trim(pos); trim(tags_str); trim(freq_str); trim(level_str);

            LexicalEntry entry;
            entry.word = word;
            entry.pos = pos;
            try {
                if (!freq_str.empty()) entry.frequency = std::stof(freq_str);
            } catch (...) {
                entry.frequency = 1.0f;
            }
            entry.complexity_level = level_str.empty() ? "standard" : level_str;

            std::stringstream tag_ss(tags_str);
            std::string tag;
            while (std::getline(tag_ss, tag, ',')) {
                trim(tag);
                if (!tag.empty()) entry.semantic_tags.push_back(tag);
            }

            lexicon_[pos].push_back(entry);
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "GrammarEngine loaded lexicon for " + std::to_string(lexicon_.size()) + " POS categories.");
    return true;
}

SemanticFrame GrammarEngine::buildCausalFrame(
    const std::string& cause,
    const std::string& effect,
    const std::string& mechanism)
{
    SemanticFrame frame;
    frame.frame_type = "CAUSAL";
    frame.slots.push_back({FrameRole::AGENT, cause, "NOUN", false});
    frame.slots.push_back({FrameRole::ACTION, "causes", "VERB", false});
    frame.slots.push_back({FrameRole::PATIENT, effect, "NOUN", false});
    if (!mechanism.empty()) {
        frame.slots.push_back({FrameRole::INSTRUMENT, mechanism, "NOUN", true});
    }
    return frame;
}

SemanticFrame GrammarEngine::buildDescriptiveFrame(
    const std::string& subject,
    const std::vector<std::string>& properties)
{
    SemanticFrame frame;
    frame.frame_type = "DESCRIPTIVE";
    frame.slots.push_back({FrameRole::AGENT, subject, "NOUN", false});
    for (const auto& prop : properties) {
        frame.slots.push_back({FrameRole::MANNER, prop, "ADJ", false});
    }
    return frame;
}

SemanticFrame GrammarEngine::buildDefinitionFrame(
    const std::string& term,
    const std::string& category,
    const std::string& distinguishing_feature)
{
    SemanticFrame frame;
    frame.frame_type = "DEFINITION";
    frame.slots.push_back({FrameRole::AGENT, term, "NOUN", false});
    frame.slots.push_back({FrameRole::PATIENT, category, "NOUN", false});
    frame.slots.push_back({FrameRole::MANNER, distinguishing_feature, "ADJ", false});
    return frame;
}

SemanticFrame GrammarEngine::buildAnalogyFrame(
    const std::string& source_domain,
    const std::string& target_domain,
    const std::vector<std::tuple<std::string, std::string, std::string>>& mappings)
{
    SemanticFrame frame;
    frame.frame_type = "ANALOGY";
    frame.slots.push_back({FrameRole::AGENT, source_domain, "NOUN", false});
    frame.slots.push_back({FrameRole::PATIENT, target_domain, "NOUN", false});
    if (!mappings.empty()) {
        frame.slots.push_back({FrameRole::RESULT, std::get<0>(mappings[0]) + " maps to " + std::get<1>(mappings[0]), "NOUN", false});
    }
    return frame;
}

bool GrammarEngine::isWordComplexityAppropriate(const std::string& word, Complexity level) const {
    for (const auto& kv : lexicon_) {
        for (const auto& entry : kv.second) {
            if (entry.word == word) {
                if (level == Complexity::CHILD) {
                    return entry.complexity_level == "child" || entry.complexity_level == "standard";
                }
                if (level == Complexity::STANDARD) {
                    return entry.complexity_level != "academic";
                }
                return true; // ACADEMIC allows all
            }
        }
    }
    return true; // default allow if not in lexicon
}

std::string GrammarEngine::selectLexicalItem(
    const std::string& pos,
    const std::string& concept_name,
    const GenerationConfig& cfg)
{
    auto it = lexicon_.find(pos);
    if (it == lexicon_.end() || it->second.empty()) {
        return concept_name;
    }

    if (ft_ && cfg.use_word2vec_lexical_selection && ft_->hasWord(concept_name)) {
        float max_score = -1.0f;
        std::string best_word = concept_name;

        for (const auto& entry : it->second) {
            if (!isWordComplexityAppropriate(entry.word, cfg.complexity)) continue;

            float sim = 0.0f;
            if (ft_->hasWord(entry.word)) {
                sim = ft_->cosineSimilarity(concept_name, entry.word);
            } else {
                sim = entry.frequency * 0.0001f;
            }

            if (sim > max_score) {
                max_score = sim;
                best_word = entry.word;
            }
        }
        return best_word;
    }

    // Fallback: pick highest frequency word appropriate for complexity level
    for (const auto& entry : it->second) {
        if (isWordComplexityAppropriate(entry.word, cfg.complexity)) {
            return entry.word;
        }
    }

    return it->second[0].word;
}

bool GrammarEngine::verifyWithConceptNet(
    const std::string& subject,
    const std::string& verb,
    const std::string& object) const
{
    if (!conceptnet_) return true;
    return conceptnet_->isPlausible(subject, verb, object);
}

std::string GrammarEngine::expandSymbol(const std::string& symbol, const SemanticFrame& frame, const GenerationConfig& cfg) {
    // Check terminal placeholders matching frame slots
    if (symbol == "__AGENT__" || symbol == "__SUBJECT__" || symbol == "__TERM__" || symbol == "__SOURCE__") {
        for (const auto& s : frame.slots) {
            if (s.role == FrameRole::AGENT) {
                return selectLexicalItem("NOUN", s.concept_name, cfg);
            }
        }
    }
    if (symbol == "__PATIENT__" || symbol == "__CATEGORY__" || symbol == "__TARGET__") {
        for (const auto& s : frame.slots) {
            if (s.role == FrameRole::PATIENT) {
                return selectLexicalItem("NOUN", s.concept_name, cfg);
            }
        }
    }
    if (symbol == "__ACTION__") {
        for (const auto& s : frame.slots) {
            if (s.role == FrameRole::ACTION) {
                return selectLexicalItem("VERB", s.concept_name, cfg);
            }
        }
    }
    if (symbol == "__INSTRUMENT__" || symbol == "__LOCATION__" || symbol == "__MAPPING__") {
        for (const auto& s : frame.slots) {
            if (s.role == FrameRole::INSTRUMENT || s.role == FrameRole::LOCATION || s.role == FrameRole::RESULT) {
                return selectLexicalItem("NOUN", s.concept_name, cfg);
            }
        }
    }
    if (symbol == "__PROPERTY__" || symbol == "__FEATURE__" || symbol == "__MANNER__") {
        for (const auto& s : frame.slots) {
            if (s.role == FrameRole::MANNER) {
                return selectLexicalItem("ADJ", s.concept_name, cfg);
            }
        }
    }

    // Check if non-terminal in rules
    auto it = rules_.find(symbol);
    if (it == rules_.end() || it->second.empty()) {
        // Direct terminal lookup
        if (lexicon_.find(symbol) != lexicon_.end()) {
            return selectLexicalItem(symbol, symbol, cfg);
        }
        // Frame placeholders should be handled by expandSymbol's placeholder logic above
        // If we reach here with a placeholder like __AGENT__, __ACTION__, etc.,
        // it means the slot was missing from the frame. Return a sensible default.
        if (symbol.size() >= 2 && symbol.front() == '_' && symbol.back() == '_') {
            if (symbol == "__AGENT__" || symbol == "__SUBJECT__" || symbol == "__TERM__" || symbol == "__SOURCE__") {
                return selectLexicalItem("NOUN", "I", cfg);
            }
            if (symbol == "__PATIENT__" || symbol == "__CATEGORY__" || symbol == "__TARGET__") {
                return selectLexicalItem("NOUN", "it", cfg);
            }
            if (symbol == "__ACTION__") {
                return selectLexicalItem("VERB", "understand", cfg);
            }
            if (symbol == "__INSTRUMENT__" || symbol == "__LOCATION__" || symbol == "__MAPPING__" || symbol == "__SETTING__") {
                return selectLexicalItem("NOUN", "here", cfg);
            }
            if (symbol == "__PROPERTY__" || symbol == "__FEATURE__" || symbol == "__MANNER__") {
                return selectLexicalItem("ADJ", "relevant", cfg);
            }
            if (symbol == "__CONFLICT__") {
                return selectLexicalItem("NOUN", "issue", cfg);
            }
        }
        return "";
    }

    // Filter rules by preference (prefer short or long)
    const auto& rule_list = it->second;
    std::size_t best_idx = 0;
    if (cfg.prefer_short_rules) {
        std::size_t min_len = 999;
        for (std::size_t i = 0; i < rule_list.size(); ++i) {
            if (rule_list[i].rhs.size() < min_len) {
                min_len = rule_list[i].rhs.size();
                best_idx = i;
            }
        }
    } else if (cfg.prefer_long_rules) {
        std::size_t max_len = 0;
        for (std::size_t i = 0; i < rule_list.size(); ++i) {
            if (rule_list[i].rhs.size() > max_len) {
                max_len = rule_list[i].rhs.size();
                best_idx = i;
            }
        }
    } else {
        // Sample uniformly from the shared, properly-seeded engine (the old
        // unseeded rand() produced the same sequence on every process run).
        if (!rule_list.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, rule_list.size() - 1);
            best_idx = pick(yuki::core::getRandomEngine());
        }
    }

    std::string result;
    for (const auto& rhs_sym : rule_list[best_idx].rhs) {
        std::string expanded = expandSymbol(rhs_sym, frame, cfg);
        if (!expanded.empty()) {
            if (!result.empty()) result += " ";
            result += expanded;
        }
    }
    return result;
}

std::string GrammarEngine::expandFrame(const SemanticFrame& frame, const GenerationConfig& cfg) {
    return expandSymbol("S", frame, cfg);
}

std::string GrammarEngine::generate(const SemanticFrame& frame, const GenerationConfig& cfg) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::string sentence = expandFrame(frame, cfg);

        if (cfg.use_conceptnet_verification && conceptnet_) {
            auto toks = tokenize(sentence);
            if (toks.size() >= 3) {
                std::string subj = toks[0];
                std::string verb = toks[1];
                std::string obj = toks[2];
                if (!verifyWithConceptNet(subj, verb, obj)) {
                    continue; // regenerate if commonsense check fails
                }
            }
        }

        if (cfg.target_word_count > 0 && !satisfiesWordCount(sentence, cfg.target_word_count)) {
            continue;
        }

        return sentence;
    }
    return expandFrame(frame, cfg);
}

std::vector<std::string> GrammarEngine::generateVariants(
    const SemanticFrame& frame,
    std::size_t n,
    const GenerationConfig& cfg)
{
    std::vector<std::string> variants;
    for (std::size_t i = 0; i < n; ++i) {
        variants.push_back(generate(frame, cfg));
    }
    return variants;
}

std::string GrammarEngine::generateExactWordCount(
    const SemanticFrame& frame,
    std::size_t word_count)
{
    GenerationConfig cfg;
    cfg.target_word_count = word_count;

    for (int attempt = 1; attempt <= 20; ++attempt) {
        if (attempt > 10) cfg.prefer_short_rules = true;
        std::string sentence = generate(frame, cfg);
        if (countWords(sentence) == word_count) {
            return sentence;
        }
    }

    // Return best attempt or format exact count pad/trim
    std::string s = generate(frame, cfg);
    auto toks = tokenize(s);
    if (toks.size() == word_count) return s;

    while (toks.size() < word_count) {
        toks.push_back("well");
    }
    if (toks.size() > word_count) {
        toks.resize(word_count);
    }
    return detokenize(toks);
}

std::size_t GrammarEngine::countSyllables(const std::string& word) const {
    std::string clean;
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            clean.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    if (clean.empty()) return 0;
    if (clean.length() <= 3) return 1;

    std::size_t count = 0;
    bool prev_vowel = false;
    auto is_vowel = [](char c) {
        return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
    };

    for (char c : clean) {
        bool v = is_vowel(c);
        if (v && !prev_vowel) {
            count++;
        }
        prev_vowel = v;
    }

    if (!clean.empty() && clean.back() == 'e' && count > 1 && (clean.size() < 2 || clean.substr(clean.size() - 2) != "le")) {
        count--;
    }

    return std::max(static_cast<std::size_t>(1), count);
}

std::size_t GrammarEngine::countWords(const std::string& sentence) const {
    return tokenize(sentence).size();
}

std::vector<std::string> GrammarEngine::tokenize(const std::string& sentence) const {
    std::vector<std::string> tokens;
    std::istringstream iss(sentence);
    std::string w;
    while (iss >> w) {
        std::string cleaned;
        for (char c : w) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                cleaned.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        if (!cleaned.empty()) {
            tokens.push_back(cleaned);
        }
    }
    return tokens;
}

std::string GrammarEngine::detokenize(const std::vector<std::string>& tokens) const {
    std::string result;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) result += " ";
        std::string w = tokens[i];
        if (i == 0 && !w.empty()) {
            w[0] = static_cast<char>(std::toupper(w[0]));
        }
        result += w;
    }
    if (!result.empty() && result.back() != '.') {
        result += ".";
    }
    return result;
}

bool GrammarEngine::satisfiesWordCount(const std::string& sentence, std::size_t target) const {
    return countWords(sentence) == target;
}

bool GrammarEngine::satisfiesSyllablePattern(const std::string& sentence, const std::size_t pattern[3]) const {
    auto toks = tokenize(sentence);
    std::size_t total = 0;
    for (const auto& t : toks) total += countSyllables(t);
    return total == (pattern[0] + pattern[1] + pattern[2]);
}

std::string GrammarEngine::generateHaiku(const std::string& topic) {
    std::string t = topic;
    if (t.empty()) t = "nature";

    std::vector<std::pair<std::string, float>> nns;
    if (ft_ && ft_->hasWord(t)) {
        nns = ft_->nearestNeighbors(t, 10);
    }

    std::string word1 = "cold";
    std::string word2 = "snow";
    std::string word3 = "falling";

    if (nns.size() >= 3) {
        word1 = nns[0].first;
        word2 = nns[1].first;
        word3 = nns[2].first;
    }

    // Generate 5-7-5 lines
    std::string line1 = "Silent " + word1 + " falls"; // ~5
    std::string line2 = "Deep " + word2 + " covers the cold earth"; // ~7
    std::string line3 = "Peaceful " + word3 + " night"; // ~5

    return line1 + " / " + line2 + " / " + line3;
}

std::string GrammarEngine::generateComplexityScaled(const SemanticFrame& frame, Complexity level) {
    GenerationConfig cfg;
    cfg.complexity = level;
    return generate(frame, cfg);
}

GrammarEngine::GrammarCheck GrammarEngine::verify(const std::string& text) const {
    GrammarCheck check;
    check.well_formed = true;
    check.grammatical_score = 1.0f;
    check.corrected = text;

    if (text.empty()) {
        check.well_formed = false;
        check.grammatical_score = 0.0f;
        ParseError err;
        err.token_position = 0;
        err.offending_token = "";
        err.expected_pos_or_symbol = "NON_EMPTY_STRING";
        err.message = "Candidate text is empty";
        check.errors.push_back(std::move(err));
        return check;
    }

    // 1. Delimiter balance check
    std::vector<char> delim_stack;
    bool in_double_quote = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
            in_double_quote = !in_double_quote;
        } else if (!in_double_quote) {
            if (c == '(' || c == '[' || c == '{') {
                delim_stack.push_back(c);
            } else if (c == ')' || c == ']' || c == '}') {
                if (delim_stack.empty()) {
                    check.well_formed = false;
                    check.grammatical_score -= 0.25f;
                    ParseError err;
                    err.token_position = i;
                    err.offending_token = std::string(1, c);
                    err.expected_pos_or_symbol = "MATCHING_OPEN_DELIMITER";
                    err.message = "Unmatched closing delimiter: " + std::string(1, c);
                    check.errors.push_back(std::move(err));
                } else {
                    char open = delim_stack.back();
                    if ((open == '(' && c == ')') ||
                        (open == '[' && c == ']') ||
                        (open == '{' && c == '}')) {
                        delim_stack.pop_back();
                    } else {
                        check.well_formed = false;
                        check.grammatical_score -= 0.25f;
                        ParseError err;
                        err.token_position = i;
                        err.offending_token = std::string(1, c);
                        err.expected_pos_or_symbol = "CORRESPONDING_DELIMITER";
                        err.message = "Mismatched delimiter pairing";
                        check.errors.push_back(std::move(err));
                    }
                }
            }
        }
    }

    if (!delim_stack.empty() || in_double_quote) {
        check.well_formed = false;
        check.grammatical_score -= 0.20f;
        ParseError err;
        err.token_position = text.size();
        err.offending_token = "EOF";
        err.expected_pos_or_symbol = "CLOSING_DELIMITER";
        err.message = "Unclosed delimiter at end of text";
        check.errors.push_back(std::move(err));
    }

    // 2. Terminal punctuation check
    char last_char = text.back();
    if (last_char != '.' && last_char != '!' && last_char != '?' &&
        last_char != '`' && last_char != '}' && last_char != ']' && last_char != ')') {
        check.grammatical_score -= 0.10f;
        // Minor flaw, repairable
        ParseError err;
        err.token_position = text.size();
        err.offending_token = std::string(1, last_char);
        err.expected_pos_or_symbol = "TERMINAL_PUNCTUATION";
        err.message = "Missing terminal sentence punctuation";
        check.errors.push_back(std::move(err));
    }

    if (!check.errors.empty()) {
        for (const auto& err : check.errors) {
            check.corrected = minimal_correction(check.corrected, err);
        }
    }

    check.grammatical_score = std::clamp(check.grammatical_score, 0.0f, 1.0f);
    return check;
}

std::string GrammarEngine::minimal_correction(const std::string& text, const ParseError& error) const {
    if (text.empty()) return text;

    std::string corrected = text;

    if (error.expected_pos_or_symbol == "TERMINAL_PUNCTUATION") {
        if (!corrected.empty() && corrected.back() != '.' && corrected.back() != '!' && corrected.back() != '?') {
            corrected += ".";
        }
    } else if (error.expected_pos_or_symbol == "CLOSING_DELIMITER") {
        // Count unclosed brackets and balance
        int round_b = 0, square_b = 0, curly_b = 0;
        for (char c : corrected) {
            if (c == '(') ++round_b;
            else if (c == ')') --round_b;
            else if (c == '[') ++square_b;
            else if (c == ']') --square_b;
            else if (c == '{') ++curly_b;
            else if (c == '}') --curly_b;
        }
        while (curly_b > 0) { corrected += "}"; --curly_b; }
        while (square_b > 0) { corrected += "]"; --square_b; }
        while (round_b > 0) { corrected += ")"; --round_b; }
    }

    return corrected;
}

} // namespace yuki::language
