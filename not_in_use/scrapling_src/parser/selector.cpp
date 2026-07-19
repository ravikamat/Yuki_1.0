#include "selector.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <regex>

namespace scrapling {

Selector::Selector(const std::string& html, const std::string& url, SelectorConfig config)
    : url_(url), config_(std::move(config)) {
    HtmlParser parser;
    auto doc = parser.parse(html, url);
    node_ = doc.root;
}

Selector::Selector(std::shared_ptr<DomNode> node, const std::string& url, SelectorConfig config)
    : node_(std::move(node)), url_(url), config_(std::move(config)) {}

std::shared_ptr<Selector> Selector::css(const std::string& selector) const {
    if (!node_) return nullptr;
    auto results = node_->find_all(selector);
    return results.empty() ? nullptr : std::make_shared<Selector>(results[0], url_, config_);
}

std::vector<std::shared_ptr<Selector>> Selector::css_all(const std::string& selector) const {
    if (!node_) return {};
    auto results = node_->find_all(selector);
    std::vector<std::shared_ptr<Selector>> out;
    for (const auto& r : results) {
        out.push_back(std::make_shared<Selector>(r, url_, config_));
    }
    return out;
}

std::shared_ptr<Selector> Selector::xpath(const std::string& expr) const {
    // Simplified XPath: only support /tag, //tag, tag[@attr='val'], tag[n]
    // For full XPath we'd need a proper parser — this is a pragmatic subset
    if (!node_) return nullptr;

    std::string sel = expr;
    // Convert simple XPath to CSS
    sel = std::regex_replace(sel, std::regex("^//"), " ");
    sel = std::regex_replace(sel, std::regex("^/"), "");
    sel = std::regex_replace(sel, std::regex("\[@([^=]+)='([^']+)'\]"), "[$1='$2']");
    sel = std::regex_replace(sel, std::regex("\[(\d+)\]"), ":nth-of-type($1)");

    return css(sel);
}

std::vector<std::shared_ptr<Selector>> Selector::xpath_all(const std::string& expr) const {
    if (!node_) return {};
    std::string sel = expr;
    sel = std::regex_replace(sel, std::regex("^//"), " ");
    sel = std::regex_replace(sel, std::regex("^/"), "");
    sel = std::regex_replace(sel, std::regex("\[@([^=]+)='([^']+)'\]"), "[$1='$2']");
    sel = std::regex_replace(sel, std::regex("\[(\d+)\]"), ":nth-of-type($1)");
    return css_all(sel);
}

TextHandler Selector::text() const {
    if (!node_) return TextHandler("");
    return TextHandler(node_->inner_text());
}

TextHandler Selector::get_text(const std::string& css_selector) const {
    if (!css_selector.empty()) {
        auto el = css(css_selector);
        if (el) return el->text();
    }
    return text();
}

AttributesHandler Selector::attrs() const {
    if (!node_) return AttributesHandler({});
    return AttributesHandler(node_->attributes);
}

std::optional<std::string> Selector::attr(const std::string& name) const {
    if (!node_) return std::nullopt;
    auto it = node_->attributes.find(name);
    if (it != node_->attributes.end()) return it->second;
    return std::nullopt;
}

std::string Selector::html() const {
    if (!node_) return "";
    return node_->inner_html();
}

std::string Selector::outer_html() const {
    if (!node_) return "";
    return node_->outer_html();
}

std::shared_ptr<Selector> Selector::parent() const {
    if (!node_) return nullptr;
    auto p = node_->parent.lock();
    return p ? std::make_shared<Selector>(p, url_, config_) : nullptr;
}

std::shared_ptr<Selector> Selector::next() const {
    if (!node_) return nullptr;
    auto n = node_->next_element_sibling();
    return n ? std::make_shared<Selector>(n, url_, config_) : nullptr;
}

std::shared_ptr<Selector> Selector::prev() const {
    if (!node_) return nullptr;
    auto p = node_->prev_element_sibling();
    return p ? std::make_shared<Selector>(p, url_, config_) : nullptr;
}

std::vector<std::shared_ptr<Selector>> Selector::children() const {
    if (!node_) return {};
    std::vector<std::shared_ptr<Selector>> out;
    for (const auto& child : node_->children) {
        if (child->type == DomNode::Element) {
            out.push_back(std::make_shared<Selector>(child, url_, config_));
        }
    }
    return out;
}

std::string Selector::absolute_url(const std::string& relative) const {
    return url::join(url_, relative);
}

std::string Selector::href() const {
    auto a = attr("href");
    return a ? absolute_url(*a) : "";
}

std::string Selector::src() const {
    auto s = attr("src");
    return s ? absolute_url(*s) : "";
}

std::string Selector::clean_text() const {
    auto t = text().raw();
    // Collapse whitespace
    std::string result;
    bool in_space = false;
    for (char c : t) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                result += ' ';
                in_space = true;
            }
        } else {
            result += c;
            in_space = false;
        }
    }
    // Trim
    auto start = result.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    auto end = result.find_last_not_of(' ');
    return result.substr(start, end - start + 1);
}

std::vector<std::string> Selector::text_list(const std::string& css_selector) const {
    std::vector<std::string> out;
    auto items = css_selector.empty() ? children() : css_all(css_selector);
    for (const auto& item : items) {
        out.push_back(item->text().raw());
    }
    return out;
}

std::map<std::string, std::string> Selector::attr_map(const std::vector<std::string>& names) const {
    std::map<std::string, std::string> out;
    if (!node_) return out;
    for (const auto& name : names) {
        auto it = node_->attributes.find(name);
        if (it != node_->attributes.end()) out[name] = it->second;
    }
    return out;
}

std::vector<std::vector<std::string>> Selector::table() const {
    std::vector<std::vector<std::string>> result;
    if (!node_) return result;

    auto rows = css_all("tr");
    for (const auto& row : rows) {
        std::vector<std::string> cells;
        auto tds = row->css_all("td, th");
        for (const auto& td : tds) {
            cells.push_back(td->clean_text());
        }
        if (!cells.empty()) result.push_back(cells);
    }
    return result;
}

std::vector<nlohmann::json> Selector::json_ld() const {
    std::vector<nlohmann::json> result;
    if (!node_) return result;

    auto scripts = css_all("script[type='application/ld+json']");
    for (const auto& script : scripts) {
        try {
            auto j = nlohmann::json::parse(script->text().raw(), nullptr, false);
            if (!j.is_discarded()) result.push_back(j);
        } catch (...) {
            // Skip invalid JSON-LD
        }
    }
    return result;
}

std::vector<std::shared_ptr<Selector>> Selector::css_all(
    const std::string& selector, 
    std::function<bool(const Selector&)> predicate) const {
    auto all = css_all(selector);
    std::vector<std::shared_ptr<Selector>> out;
    for (const auto& s : all) {
        if (predicate(*s)) out.push_back(s);
    }
    return out;
}

// Selectors implementation
std::vector<TextHandler> Selectors::texts() const {
    std::vector<TextHandler> out;
    for (const auto& s : items_) out.push_back(s->text());
    return out;
}

std::vector<AttributesHandler> Selectors::attrs_list() const {
    std::vector<AttributesHandler> out;
    for (const auto& s : items_) out.push_back(s->attrs());
    return out;
}

std::vector<std::string> Selectors::htmls() const {
    std::vector<std::string> out;
    for (const auto& s : items_) out.push_back(s->html());
    return out;
}

std::vector<std::string> Selectors::hrefs() const {
    std::vector<std::string> out;
    for (const auto& s : items_) {
        auto h = s->attr("href");
        if (h) out.push_back(s->absolute_url(*h));
    }
    return out;
}

std::vector<std::string> Selectors::srcs() const {
    std::vector<std::string> out;
    for (const auto& s : items_) {
        auto s_ = s->attr("src");
        if (s_) out.push_back(s->absolute_url(*s_));
    }
    return out;
}

std::vector<std::shared_ptr<Selector>> Selectors::filter(std::function<bool(const Selector&)> pred) const {
    std::vector<std::shared_ptr<Selector>> out;
    for (const auto& s : items_) {
        if (pred(*s)) out.push_back(s);
    }
    return out;
}

std::vector<std::shared_ptr<Selector>> Selectors::drop(std::function<bool(const Selector&)> pred) const {
    std::vector<std::shared_ptr<Selector>> out;
    for (const auto& s : items_) {
        if (!pred(*s)) out.push_back(s);
    }
    return out;
}

} // namespace scrapling
