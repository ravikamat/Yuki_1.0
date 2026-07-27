#include "brain/language/GrammarExtractor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace yuki::language {

bool GrammarExtractor::parseSubtree(const std::string& text, size_t& pos, std::string& out_label,
                                    std::vector<std::string>& out_children) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) pos++;
    if (pos >= text.size() || text[pos] != '(') return false;
    pos++; // consume '('

    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) pos++;
    size_t label_start = pos;
    while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])) &&
           text[pos] != '(' && text[pos] != ')') {
        pos++;
    }
    out_label = text.substr(label_start, pos - label_start);
    if (out_label.empty()) return false;

    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) pos++;
        if (pos >= text.size()) break;

        if (text[pos] == '(') {
            std::string child_label;
            std::vector<std::string> child_children;
            if (parseSubtree(text, pos, child_label, child_children)) {
                out_children.push_back(child_label);
            } else {
                return false;
            }
        } else if (text[pos] == ')') {
            pos++; // consume ')'
            break;
        } else {
            // Terminal word
            size_t word_start = pos;
            while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])) && text[pos] != ')') {
                pos++;
            }
            std::string word = text.substr(word_start, pos - word_start);
            if (!word.empty()) {
                out_children.push_back(word);
                // Lexical entry
                LexicalEntry entry{word, out_label, 1.0f, 1};
                bool found = false;
                for (auto& le : lexicon_[word]) {
                    if (le.pos_tag == out_label) {
                        le.count++;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    lexicon_[word].push_back(entry);
                }
            }
        }
    }

    if (!out_children.empty()) {
        extractRules(out_label, out_children);
    }
    return true;
}

void GrammarExtractor::extractRules(const std::string& label, const std::vector<std::string>& children) {
    auto& rule_list = rules_[label];
    bool found = false;
    for (auto& r : rule_list) {
        if (r.rhs == children) {
            r.count++;
            found = true;
            break;
        }
    }
    if (!found) {
        PcfgRule r;
        r.lhs = label;
        r.rhs = children;
        r.count = 1;
        rule_list.push_back(r);
    }
}

bool GrammarExtractor::parseBracketedLine(const std::string& line) {
    if (line.empty()) return false;
    size_t pos = 0;
    std::string label;
    std::vector<std::string> children;
    bool ok = parseSubtree(line, pos, label, children);
    if (ok) {
        computeProbabilities();
    }
    return ok;
}

bool GrammarExtractor::parseFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    size_t parsed_count = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = 0;
        std::string label;
        std::vector<std::string> children;
        if (parseSubtree(line, pos, label, children)) {
            parsed_count++;
        }
    }
    computeProbabilities();
    return parsed_count > 0;
}

void GrammarExtractor::computeProbabilities() {
    for (auto& [lhs, rule_list] : rules_) {
        uint64_t total = 0;
        for (const auto& r : rule_list) total += r.count;
        if (total > 0) {
            for (auto& r : rule_list) {
                r.probability = static_cast<float>(r.count) / static_cast<float>(total);
            }
        }
    }

    for (auto& [word, lex_list] : lexicon_) {
        uint64_t total = 0;
        for (const auto& le : lex_list) total += le.count;
        if (total > 0) {
            for (auto& le : lex_list) {
                le.probability = static_cast<float>(le.count) / static_cast<float>(total);
            }
        }
    }
}

void GrammarExtractor::exportToGrammarEngine(const std::string& frames_path,
                                            const std::string& rules_path,
                                            const std::string& lexicon_path) const {
    std::ofstream out_rules(rules_path, std::ios::app);
    if (out_rules.is_open()) {
        for (const auto& [lhs, rule_list] : rules_) {
            for (const auto& r : rule_list) {
                out_rules << r.lhs << " ->";
                for (const auto& child : r.rhs) {
                    out_rules << " " << child;
                }
                out_rules << " | " << r.probability << " | " << r.count << "\n";
            }
        }
    }

    std::ofstream out_lex(lexicon_path, std::ios::app);
    if (out_lex.is_open()) {
        for (const auto& [word, lex_list] : lexicon_) {
            for (const auto& le : lex_list) {
                out_lex << le.word << " | " << le.pos_tag << " | " << le.probability << " | general | standard\n";
            }
        }
    }
}

} // namespace yuki::language
