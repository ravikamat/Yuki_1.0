#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace scrapling {

// Forward declarations
class Selector;
class Response;

// Text handling — mirrors Scrapling's TextHandler
class TextHandler {
    std::string text_;
public:
    explicit TextHandler(std::string text) : text_(std::move(text)) {}

    const std::string& raw() const { return text_; }
    std::string stripped() const {
        std::string s = text_;
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }
    std::string as_json() const;
    bool empty() const { return text_.empty(); }
    operator std::string() const { return text_; }
};

// Attribute handling — mirrors Scrapling's AttributesHandler
class AttributesHandler {
    std::map<std::string, std::string> attrs_;
public:
    explicit AttributesHandler(std::map<std::string, std::string> attrs) : attrs_(std::move(attrs)) {}

    std::optional<std::string> get(const std::string& name) const {
        auto it = attrs_.find(name);
        if (it != attrs_.end()) return it->second;
        return std::nullopt;
    }
    bool has(const std::string& name) const { return attrs_.count(name); }
    const std::map<std::string, std::string>& all() const { return attrs_; }
    std::string as_json() const;
};

// Selector configuration
struct SelectorConfig {
    bool preserve_whitespace = false;
    bool case_sensitive = false;
    std::string base_url;
};

// URL utilities
namespace url {
    std::string join(const std::string& base, const std::string& relative);
    std::string domain(const std::string& url);
    bool is_absolute(const std::string& url);
}

} // namespace scrapling
