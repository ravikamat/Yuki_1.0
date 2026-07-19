#pragma once
#include "dom.hpp"
#include <string>
#include <optional>

namespace scrapling {

class HtmlParser {
public:
    HtmlParser() = default;

    HtmlDocument parse(const std::string& html, const std::string& base_url = "");

private:
    struct Token {
        enum Type { TagOpen, TagClose, Text, Comment, Doctype, EOF } type;
        std::string tag;
        std::map<std::string, std::string> attrs;
        std::string text;
        bool self_closing = false;
    };

    std::string input_;
    size_t pos_ = 0;

    Token next_token();
    void skip_whitespace();
    std::string parse_tag_name();
    std::map<std::string, std::string> parse_attributes();
    std::string parse_attribute_value();
    std::string parse_text();
    std::string parse_comment();
    std::string parse_doctype();

    bool is_void_tag(const std::string& tag) const;
    bool can_contain(const std::string& parent, const std::string& child) const;
    void auto_close(std::vector<std::shared_ptr<DomNode>>& stack, const std::string& tag);
};

} // namespace scrapling
