#pragma once
#include "dom.hpp"
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <functional>

namespace scrapling {

// CSS Selector AST
struct CssSelector {
    struct SimpleSelector {
        std::string tag;           // empty = any tag
        std::string id;
        std::vector<std::string> classes;
        struct AttrTest {
            std::string name;
            enum Op { Exists, Equals, ContainsWord, StartsWith, EndsWith, Contains, Hyphenated } op;
            std::string value;
        };
        std::vector<AttrTest> attrs;

        struct Pseudo {
            std::string name;
            std::variant<std::monostate, int, std::string, std::shared_ptr<CssSelector>> arg;
        };
        std::vector<Pseudo> pseudos;

        bool matches(const DomNode& node) const;
    };

    enum Combinator { Descendant, Child, AdjacentSibling, GeneralSibling };
    struct SelectorUnit {
        SimpleSelector selector;
        Combinator combinator = Descendant; // combinator to NEXT unit (right side)
    };
    std::vector<SelectorUnit> units; // left to right

    bool matches(const DomNode& node, const DomNode& root) const;
};

// Parse CSS selector string
CssSelector parse_css_selector(const std::string& selector);

// Match against DOM tree
std::vector<std::shared_ptr<DomNode>> match_css_selector(const DomNode& root, const std::string& selector);

} // namespace scrapling
