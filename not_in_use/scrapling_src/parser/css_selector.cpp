#include "css_selector.hpp"
#include <cctype>
#include <sstream>
#include <algorithm>
#include <regex>

namespace scrapling {

// Tokenize CSS selector
static std::vector<std::string> tokenize_css(const std::string& s) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < s.length()) {
        while (i < s.length() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i >= s.length()) break;

        char c = s[i];
        if (c == '>' || c == '+' || c == '~' || c == ',' || c == '(' || c == ')' || c == '[' || c == ']' || c == '=' || c == '*' || c == '|' || c == '^' || c == '$') {
            // Check for two-char operators
            if (i + 1 < s.length()) {
                std::string two = s.substr(i, 2);
                if (two == "~=" || two == "|=" || two == "^=" || two == "$=" || two == "*=") {
                    tokens.push_back(two);
                    i += 2;
                    continue;
                }
            }
            tokens.push_back(std::string(1, c));
            i++;
        } else if (c == '"' || c == ''') {
            char quote = c;
            i++;
            std::string val;
            while (i < s.length() && s[i] != quote) {
                if (s[i] == '\\' && i + 1 < s.length()) {
                    val += s[i+1];
                    i += 2;
                } else {
                    val += s[i++];
                }
            }
            tokens.push_back(""" + val + """);
            i++; // skip closing quote
        } else if (c == '#') {
            i++;
            std::string id;
            while (i < s.length() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '_' || s[i] == '.')) {
                id += s[i++];
            }
            tokens.push_back("#" + id);
        } else if (c == '.') {
            i++;
            std::string cls;
            while (i < s.length() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '_' || s[i] == '.')) {
                cls += s[i++];
            }
            tokens.push_back("." + cls);
        } else if (c == ':') {
            i++;
            std::string pseudo;
            while (i < s.length() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '_')) {
                pseudo += s[i++];
            }
            tokens.push_back(":" + pseudo);
        } else {
            std::string tag;
            while (i < s.length() && !std::isspace(static_cast<unsigned char>(s[i])) && s[i] != '>' && s[i] != '+' && s[i] != '~' && s[i] != '#' && s[i] != '.' && s[i] != '[' && s[i] != ':' && s[i] != '(' && s[i] != ')') {
                tag += s[i++];
            }
            if (!tag.empty()) tokens.push_back(tag);
        }
    }
    return tokens;
}

CssSelector parse_css_selector(const std::string& selector) {
    CssSelector result;
    auto tokens = tokenize_css(selector);

    size_t i = 0;
    while (i < tokens.size()) {
        CssSelector::SelectorUnit unit;

        // Parse simple selector
        auto& simple = unit.selector;
        while (i < tokens.size()) {
            const std::string& tok = tokens[i];

            if (tok == ">" || tok == "+" || tok == "~" || tok == " ") {
                break; // combinator
            }
            if (tok == ",") {
                break; // group separator (we only handle single selector for now)
            }

            if (tok.starts_with("#")) {
                simple.id = tok.substr(1);
                i++;
            } else if (tok.starts_with(".")) {
                simple.classes.push_back(tok.substr(1));
                i++;
            } else if (tok.starts_with(":")) {
                CssSelector::SimpleSelector::Pseudo pseudo;
                pseudo.name = tok.substr(1);
                i++;
                if (i < tokens.size() && tokens[i] == "(") {
                    i++;
                    std::string arg_str;
                    int paren_depth = 1;
                    while (i < tokens.size() && paren_depth > 0) {
                        if (tokens[i] == "(") paren_depth++;
                        else if (tokens[i] == ")") paren_depth--;
                        if (paren_depth > 0) {
                            if (!arg_str.empty()) arg_str += " ";
                            arg_str += tokens[i];
                        }
                        i++;
                    }
                    // Try parse as int
                    try {
                        pseudo.arg = std::stoi(arg_str);
                    } catch (...) {
                        pseudo.arg = arg_str;
                    }
                }
                simple.pseudos.push_back(pseudo);
            } else if (tok == "[") {
                i++;
                if (i < tokens.size()) {
                    CssSelector::SimpleSelector::AttrTest attr;
                    attr.name = tokens[i++];
                    if (i < tokens.size() && (tokens[i] == "=" || tokens[i] == "~=" || tokens[i] == "|=" || tokens[i] == "^=" || tokens[i] == "$=" || tokens[i] == "*=")) {
                        std::string op = tokens[i++];
                        if (op == "=") attr.op = CssSelector::SimpleSelector::AttrTest::Equals;
                        else if (op == "~=") attr.op = CssSelector::SimpleSelector::AttrTest::ContainsWord;
                        else if (op == "|=") attr.op = CssSelector::SimpleSelector::AttrTest::Hyphenated;
                        else if (op == "^=") attr.op = CssSelector::SimpleSelector::AttrTest::StartsWith;
                        else if (op == "$=") attr.op = CssSelector::SimpleSelector::AttrTest::EndsWith;
                        else if (op == "*=") attr.op = CssSelector::SimpleSelector::AttrTest::Contains;
                        if (i < tokens.size()) {
                            std::string val = tokens[i++];
                            if (val.starts_with('"') && val.ends_with('"')) {
                                val = val.substr(1, val.length() - 2);
                            }
                            attr.value = val;
                        }
                    } else {
                        attr.op = CssSelector::SimpleSelector::AttrTest::Exists;
                    }
                    if (i < tokens.size() && tokens[i] == "]") i++;
                    simple.attrs.push_back(attr);
                }
            } else {
                simple.tag = tok;
                std::transform(simple.tag.begin(), simple.tag.end(), simple.tag.begin(),
                              [](unsigned char c){ return std::tolower(c); });
                i++;
            }
        }

        // Combinator
        if (i < tokens.size()) {
            if (tokens[i] == ">") {
                unit.combinator = CssSelector::Child;
                i++;
            } else if (tokens[i] == "+") {
                unit.combinator = CssSelector::AdjacentSibling;
                i++;
            } else if (tokens[i] == "~") {
                unit.combinator = CssSelector::GeneralSibling;
                i++;
            } else if (tokens[i] == " ") {
                unit.combinator = CssSelector::Descendant;
                i++;
            } else if (tokens[i] == ",") {
                break; // For now, stop at comma (would need selector groups)
            }
        }

        result.units.push_back(unit);
    }

    return result;
}

// Check if simple selector matches a node
bool CssSelector::SimpleSelector::matches(const DomNode& node) const {
    if (node.type != DomNode::Element) return false;

    if (!tag.empty() && node.tag != tag) return false;
    if (!id.empty() && node.id() != id) return false;
    for (const auto& cls : classes) {
        if (!node.has_class(cls)) return false;
    }
    for (const auto& attr : attrs) {
        auto it = node.attributes.find(attr.name);
        if (it == node.attributes.end()) return false;
        if (attr.op == AttrTest::Equals && it->second != attr.value) return false;
        if (attr.op == AttrTest::ContainsWord) {
            std::stringstream ss(it->second);
            std::string word;
            bool found = false;
            while (ss >> word) {
                if (word == attr.value) { found = true; break; }
            }
            if (!found) return false;
        }
        if (attr.op == AttrTest::StartsWith && !it->second.starts_with(attr.value)) return false;
        if (attr.op == AttrTest::EndsWith && !it->second.ends_with(attr.value)) return false;
        if (attr.op == AttrTest::Contains && it->second.find(attr.value) == std::string::npos) return false;
        if (attr.op == AttrTest::Hyphenated) {
            if (it->second != attr.value && !it->second.starts_with(attr.value + "-")) return false;
        }
    }

    // Pseudo-classes
    for (const auto& pseudo : pseudos) {
        if (pseudo.name == "first-child") {
            auto parent = node.parent.lock();
            if (!parent) return false;
            for (const auto& child : parent->children) {
                if (child->type == DomNode::Element) {
                    return child.get() == &node;
                }
            }
            return false;
        } else if (pseudo.name == "last-child") {
            auto parent = node.parent.lock();
            if (!parent) return false;
            for (auto it = parent->children.rbegin(); it != parent->children.rend(); ++it) {
                if ((*it)->type == DomNode::Element) {
                    return it->get() == &node;
                }
            }
            return false;
        } else if (pseudo.name == "nth-child") {
            int n = 1;
            if (std::holds_alternative<int>(pseudo.arg)) n = std::get<int>(pseudo.arg);
            auto parent = node.parent.lock();
            if (!parent) return false;
            int idx = 0;
            for (const auto& child : parent->children) {
                if (child->type == DomNode::Element) {
                    idx++;
                    if (child.get() == &node) return idx == n;
                }
            }
            return false;
        } else if (pseudo.name == "nth-of-type") {
            int n = 1;
            if (std::holds_alternative<int>(pseudo.arg)) n = std::get<int>(pseudo.arg);
            auto parent = node.parent.lock();
            if (!parent) return false;
            int idx = 0;
            for (const auto& child : parent->children) {
                if (child->type == DomNode::Element && child->tag == node.tag) {
                    idx++;
                    if (child.get() == &node) return idx == n;
                }
            }
            return false;
        } else if (pseudo.name == "empty") {
            for (const auto& child : node.children) {
                if (child->type == DomNode::Element || (child->type == DomNode::Text && !child->text.empty())) {
                    return false;
                }
            }
        } else if (pseudo.name == "not") {
            if (std::holds_alternative<std::string>(pseudo.arg)) {
                auto neg = parse_css_selector(std::get<std::string>(pseudo.arg));
                if (neg.units.size() == 1 && neg.units[0].selector.matches(node)) return false;
            }
        } else if (pseudo.name == "has") {
            // :has() is complex — simplified: check if any descendant matches
            if (std::holds_alternative<std::string>(pseudo.arg)) {
                auto inner = parse_css_selector(std::get<std::string>(pseudo.arg));
                for (const auto& child : node.children) {
                    auto matches = match_css_selector(*child, std::get<std::string>(pseudo.arg));
                    if (!matches.empty()) return true;
                }
                return false;
            }
        }
    }

    return true;
}

// Helper: get all element descendants
static void collect_elements(const DomNode& node, std::vector<std::shared_ptr<DomNode>>& out) {
    for (const auto& child : node.children) {
        if (child->type == DomNode::Element) {
            out.push_back(child);
            collect_elements(*child, out);
        }
    }
}

// Helper: get element children
static void collect_children(const DomNode& node, std::vector<std::shared_ptr<DomNode>>& out) {
    for (const auto& child : node.children) {
        if (child->type == DomNode::Element) out.push_back(child);
    }
}

// Helper: get element siblings after
static void collect_next_siblings(const DomNode& node, std::vector<std::shared_ptr<DomNode>>& out) {
    auto sib = node.next_sibling.lock();
    while (sib) {
        if (sib->type == DomNode::Element) out.push_back(sib);
        sib = sib->next_sibling.lock();
    }
}

// Helper: get all element siblings
static void collect_all_siblings(const DomNode& node, std::vector<std::shared_ptr<DomNode>>& out) {
    auto parent = node.parent.lock();
    if (!parent) return;
    for (const auto& child : parent->children) {
        if (child->type == DomNode::Element && child.get() != &node) {
            out.push_back(child);
        }
    }
}

// Match full selector chain against a node
bool CssSelector::matches(const DomNode& node, const DomNode& root) const {
    if (units.empty()) return false;

    // Match rightmost unit against node, then walk backwards
    const DomNode* current = &node;
    for (int i = static_cast<int>(units.size()) - 1; i >= 0; --i) {
        if (!units[i].selector.matches(*current)) return false;

        if (i == 0) break;

        // Find candidates for previous unit based on combinator
        std::vector<std::shared_ptr<DomNode>> candidates;
        switch (units[i].combinator) {
            case Child:
                if (auto parent = current->parent.lock()) {
                    if (parent->type == DomNode::Element) candidates.push_back(parent);
                }
                break;
            case Descendant: {
                auto p = current->parent.lock();
                while (p) {
                    if (p->type == DomNode::Element) candidates.push_back(p);
                    p = p->parent.lock();
                }
                break;
            }
            case AdjacentSibling:
                if (auto prev = current->prev_sibling.lock()) {
                    while (prev) {
                        if (prev->type == DomNode::Element) {
                            candidates.push_back(prev);
                            break;
                        }
                        prev = prev->prev_sibling.lock();
                    }
                }
                break;
            case GeneralSibling:
                if (auto prev = current->prev_sibling.lock()) {
                    while (prev) {
                        if (prev->type == DomNode::Element) candidates.push_back(prev);
                        prev = prev->prev_sibling.lock();
                    }
                }
                break;
        }

        bool found = false;
        for (const auto& cand : candidates) {
            if (units[i-1].selector.matches(*cand)) {
                current = cand.get();
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

std::vector<std::shared_ptr<DomNode>> match_css_selector(const DomNode& root, const std::string& selector) {
    std::vector<std::shared_ptr<DomNode>> result;
    auto parsed = parse_css_selector(selector);

    std::vector<std::shared_ptr<DomNode>> candidates;
    collect_elements(root, candidates);
    // Note: root is passed by const ref, so we don't add it to candidates here.
    // If root itself needs to match, the caller should handle it separately.

    for (const auto& node : candidates) {
        if (parsed.matches(*node, root)) {
            result.push_back(node);
        }
    }

    return result;
}

} // namespace scrapling
