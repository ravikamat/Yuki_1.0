#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>

namespace scrapling {

// DOM Node — lightweight tree structure
class DomNode : public std::enable_shared_from_this<DomNode> {
public:
    enum Type { Element, Text, Comment, Document };

    Type type = Element;
    std::string tag;           // lowercase tag name for elements
    std::map<std::string, std::string> attributes;
    std::string text;          // text content for Text nodes, or inner text for Element
    std::vector<std::shared_ptr<DomNode>> children;
    std::weak_ptr<DomNode> parent;
    std::weak_ptr<DomNode> next_sibling;
    std::weak_ptr<DomNode> prev_sibling;

    DomNode(Type t = Element) : type(t) {}

    // Helpers
    std::string id() const;
    std::vector<std::string> classes() const;
    bool has_class(const std::string& cls) const;
    std::string attr(const std::string& name) const;
    std::string inner_text() const;
    std::string inner_html() const;
    std::string outer_html() const;

    // Traversal
    std::shared_ptr<DomNode> first_child() const;
    std::shared_ptr<DomNode> last_child() const;
    std::shared_ptr<DomNode> next_element_sibling() const;
    std::shared_ptr<DomNode> prev_element_sibling() const;

    // Query
    std::vector<std::shared_ptr<DomNode>> find_all(const std::string& css_selector) const;
    std::shared_ptr<DomNode> find(const std::string& css_selector) const;
    std::shared_ptr<DomNode> find_by_id(const std::string& id) const;
    std::vector<std::shared_ptr<DomNode>> find_by_tag(const std::string& tag) const;
    std::vector<std::shared_ptr<DomNode>> find_by_class(const std::string& cls) const;
};

// Document root
class HtmlDocument {
public:
    std::shared_ptr<DomNode> root;
    std::string base_url;

    explicit HtmlDocument(std::shared_ptr<DomNode> r = nullptr) : root(std::move(r)) {
        if (!root) root = std::make_shared<DomNode>(DomNode::Document);
    }

    std::shared_ptr<DomNode> body() const;
    std::string title() const;
};

} // namespace scrapling
