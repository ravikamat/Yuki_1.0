#pragma once
#include "dom.hpp"
#include "css_selector.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

namespace scrapling {

class Response;

// Selector — mirrors Scrapling's Selector class
class Selector {
    std::shared_ptr<DomNode> node_;
    std::string url_;
    SelectorConfig config_;

public:
    Selector() = default;
    Selector(const std::string& html, const std::string& url = "", SelectorConfig config = {});
    Selector(std::shared_ptr<DomNode> node, const std::string& url = "", SelectorConfig config = {});

    // Query
    std::shared_ptr<Selector> css(const std::string& selector) const;
    std::vector<std::shared_ptr<Selector>> css_all(const std::string& selector) const;
    std::shared_ptr<Selector> xpath(const std::string& expr) const; // Simplified
    std::vector<std::shared_ptr<Selector>> xpath_all(const std::string& expr) const;

    // Content extraction
    TextHandler text() const;
    TextHandler get_text(const std::string& css_selector = "") const;
    AttributesHandler attrs() const;
    std::optional<std::string> attr(const std::string& name) const;
    std::string html() const;
    std::string outer_html() const;

    // Navigation
    std::shared_ptr<Selector> parent() const;
    std::shared_ptr<Selector> next() const;
    std::shared_ptr<Selector> prev() const;
    std::vector<std::shared_ptr<Selector>> children() const;

    // URL resolution
    std::string absolute_url(const std::string& relative) const;
    std::string href() const;
    std::string src() const;

    // Data extraction helpers
    std::string clean_text() const;
    std::vector<std::string> text_list(const std::string& css_selector = "") const;
    std::map<std::string, std::string> attr_map(const std::vector<std::string>& names) const;

    // Table extraction
    std::vector<std::vector<std::string>> table() const;

    // JSON-LD / structured data
    std::vector<nlohmann::json> json_ld() const;

    // Access underlying node
    std::shared_ptr<DomNode> node() const { return node_; }
    bool empty() const { return !node_; }

    // Iteration
    std::vector<std::shared_ptr<Selector>> css_all(const std::string& selector, std::function<bool(const Selector&)> predicate) const;
};

// Selectors — collection of Selector objects
class Selectors {
    std::vector<std::shared_ptr<Selector>> items_;
public:
    Selectors() = default;
    explicit Selectors(std::vector<std::shared_ptr<Selector>> items) : items_(std::move(items)) {}

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    std::shared_ptr<Selector> operator[](size_t i) const { return items_[i]; }
    std::shared_ptr<Selector> first() const { return items_.empty() ? nullptr : items_[0]; }
    std::shared_ptr<Selector> last() const { return items_.empty() ? nullptr : items_.back(); }

    std::vector<TextHandler> texts() const;
    std::vector<AttributesHandler> attrs_list() const;
    std::vector<std::string> htmls() const;
    std::vector<std::string> hrefs() const;
    std::vector<std::string> srcs() const;

    std::vector<std::shared_ptr<Selector>> filter(std::function<bool(const Selector&)> pred) const;
    std::vector<std::shared_ptr<Selector>> drop(std::function<bool(const Selector&)> pred) const;

    const std::vector<std::shared_ptr<Selector>>& items() const { return items_; }
};

} // namespace scrapling
