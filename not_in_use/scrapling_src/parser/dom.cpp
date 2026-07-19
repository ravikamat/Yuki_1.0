#include "dom.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace scrapling {

std::string DomNode::id() const {
    auto it = attributes.find("id");
    return (it != attributes.end()) ? it->second : "";
}

std::vector<std::string> DomNode::classes() const {
    auto it = attributes.find("class");
    if (it == attributes.end()) return {};
    std::vector<std::string> result;
    std::stringstream ss(it->second);
    std::string cls;
    while (ss >> cls) {
        if (!cls.empty()) result.push_back(cls);
    }
    return result;
}

bool DomNode::has_class(const std::string& cls) const {
    auto clist = classes();
    return std::find(clist.begin(), clist.end(), cls) != clist.end();
}

std::string DomNode::attr(const std::string& name) const {
    auto it = attributes.find(name);
    return (it != attributes.end()) ? it->second : "";
}

std::string DomNode::inner_text() const {
    if (type == Text) return text;
    std::string result;
    for (const auto& child : children) {
        if (child->type == Text) {
            result += child->text;
        } else if (child->type == Element) {
            result += child->inner_text();
        }
    }
    return result;
}

std::string DomNode::inner_html() const {
    if (type == Text) return text;
    std::string result;
    for (const auto& child : children) {
        result += child->outer_html();
    }
    return result;
}

std::string DomNode::outer_html() const {
    if (type == Text) return text;
    if (type == Comment) return "<!--" + text + "-->";
    if (type == Document) return inner_html();

    std::string result = "<" + tag;
    for (const auto& [k, v] : attributes) {
        result += " " + k + "=\"" + v + "\"";
    }
    if (children.empty()) {
        // Self-closing tags
        static const std::vector<std::string> void_tags = {
            "area", "base", "br", "col", "embed", "hr", "img", "input",
            "link", "meta", "param", "source", "track", "wbr"
        };
        if (std::find(void_tags.begin(), void_tags.end(), tag) != void_tags.end()) {
            result += " />";
        } else {
            result += "></" + tag + ">";
        }
    } else {
        result += ">" + inner_html() + "</" + tag + ">";
    }
    return result;
}

std::shared_ptr<DomNode> DomNode::first_child() const {
    if (children.empty()) return nullptr;
    return children.front();
}

std::shared_ptr<DomNode> DomNode::last_child() const {
    if (children.empty()) return nullptr;
    return children.back();
}

std::shared_ptr<DomNode> DomNode::next_element_sibling() const {
    auto sib = next_sibling.lock();
    while (sib) {
        if (sib->type == Element) return sib;
        sib = sib->next_sibling.lock();
    }
    return nullptr;
}

std::shared_ptr<DomNode> DomNode::prev_element_sibling() const {
    auto sib = prev_sibling.lock();
    while (sib) {
        if (sib->type == Element) return sib;
        sib = sib->prev_sibling.lock();
    }
    return nullptr;
}

// Forward declarations for CSS selector matching
std::vector<std::shared_ptr<DomNode>> match_css_selector(const DomNode& root, const std::string& selector);

std::vector<std::shared_ptr<DomNode>> DomNode::find_all(const std::string& css_selector) const {
    return match_css_selector(*this, css_selector);
}

std::shared_ptr<DomNode> DomNode::find(const std::string& css_selector) const {
    auto results = find_all(css_selector);
    return results.empty() ? nullptr : results[0];
}

std::shared_ptr<DomNode> DomNode::find_by_id(const std::string& id_val) const {
    return find("#" + id_val);
}

std::vector<std::shared_ptr<DomNode>> DomNode::find_by_tag(const std::string& tag_name) const {
    return find_all(tag_name);
}

std::vector<std::shared_ptr<DomNode>> DomNode::find_by_class(const std::string& cls) const {
    return find_all("." + cls);
}

// HtmlDocument helpers
std::shared_ptr<DomNode> HtmlDocument::body() const {
    if (!root) return nullptr;
    return root->find("body");
}

std::string HtmlDocument::title() const {
    if (!root) return "";
    auto title_node = root->find("title");
    return title_node ? title_node->inner_text() : "";
}

} // namespace scrapling
