#include "html_parser.hpp"
#include <cctype>
#include <algorithm>
#include <stack>

namespace scrapling {

static const std::vector<std::string> VOID_TAGS = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"
};

static const std::vector<std::string> RAW_TEXT_TAGS = {
    "script", "style", "textarea", "title"
};

bool HtmlParser::is_void_tag(const std::string& tag) const {
    return std::find(VOID_TAGS.begin(), VOID_TAGS.end(), tag) != VOID_TAGS.end();
}

bool HtmlParser::can_contain(const std::string& parent, const std::string& child) const {
    // Simplified HTML5 content model
    if (parent == "table" && child != "thead" && child != "tbody" && child != "tfoot" && child != "tr" && child != "caption" && child != "colgroup" && child != "col") return false;
    if (parent == "ul" && child != "li") return false;
    if (parent == "ol" && child != "li") return false;
    if (parent == "select" && child != "option" && child != "optgroup") return false;
    if (parent == "colgroup" && child != "col") return false;
    return true;
}

void HtmlParser::auto_close(std::vector<std::shared_ptr<DomNode>>& stack, const std::string& tag) {
    // Auto-close tags that can't contain the new tag
    while (!stack.empty()) {
        auto& top = stack.back();
        if (top->tag == tag) return; // Will be handled by close tag
        if (!can_contain(top->tag, tag)) {
            stack.pop_back();
        } else {
            break;
        }
    }
}

HtmlParser::Token HtmlParser::next_token() {
    if (pos_ >= input_.length()) return {Token::EOF, {}, {}, {}, false};

    if (input_[pos_] == '<') {
        if (pos_ + 1 < input_.length() && input_[pos_+1] == '!') {
            if (pos_ + 3 < input_.length() && input_.substr(pos_+2, 2) == "--") {
                pos_ += 4;
                return {Token::Comment, {}, {}, parse_comment(), false};
            }
            if (pos_ + 8 < input_.length() && input_.substr(pos_+2, 7) == "DOCTYPE") {
                pos_ += 9;
                return {Token::Doctype, {}, {}, parse_doctype(), false};
            }
        }
        if (pos_ + 1 < input_.length() && input_[pos_+1] == '/') {
            pos_ += 2;
            std::string tag = parse_tag_name();
            while (pos_ < input_.length() && input_[pos_] != '>') pos_++;
            if (pos_ < input_.length()) pos_++;
            return {Token::TagClose, tag, {}, {}, false};
        }
        pos_++;
        std::string tag = parse_tag_name();
        auto attrs = parse_attributes();
        bool self_closing = false;
        if (pos_ < input_.length() && input_[pos_] == '/') {
            self_closing = true;
            pos_++;
        }
        while (pos_ < input_.length() && input_[pos_] != '>') pos_++;
        if (pos_ < input_.length()) pos_++;
        return {Token::TagOpen, tag, attrs, {}, self_closing || is_void_tag(tag)};
    }

    return {Token::Text, {}, {}, parse_text(), false};
}

std::string HtmlParser::parse_text() {
    std::string result;
    while (pos_ < input_.length() && input_[pos_] != '<') {
        result += input_[pos_++];
    }
    return result;
}

std::string HtmlParser::parse_comment() {
    std::string result;
    while (pos_ + 2 < input_.length()) {
        if (input_[pos_] == '-' && input_[pos_+1] == '-' && input_[pos_+2] == '>') {
            pos_ += 3;
            break;
        }
        result += input_[pos_++];
    }
    return result;
}

std::string HtmlParser::parse_doctype() {
    std::string result;
    while (pos_ < input_.length() && input_[pos_] != '>') {
        result += input_[pos_++];
    }
    if (pos_ < input_.length()) pos_++;
    return result;
}

std::string HtmlParser::parse_tag_name() {
    std::string name;
    while (pos_ < input_.length() && !std::isspace(static_cast<unsigned char>(input_[pos_])) && input_[pos_] != '>' && input_[pos_] != '/') {
        name += std::tolower(static_cast<unsigned char>(input_[pos_++]));
    }
    return name;
}

std::map<std::string, std::string> HtmlParser::parse_attributes() {
    std::map<std::string, std::string> attrs;
    while (pos_ < input_.length() && input_[pos_] != '>' && input_[pos_] != '/') {
        skip_whitespace();
        if (pos_ >= input_.length() || input_[pos_] == '>' || input_[pos_] == '/') break;

        std::string name;
        while (pos_ < input_.length() && !std::isspace(static_cast<unsigned char>(input_[pos_])) && input_[pos_] != '=' && input_[pos_] != '>' && input_[pos_] != '/') {
            name += std::tolower(static_cast<unsigned char>(input_[pos_++]));
        }
        skip_whitespace();
        std::string value;
        if (pos_ < input_.length() && input_[pos_] == '=') {
            pos_++;
            skip_whitespace();
            value = parse_attribute_value();
        }
        if (!name.empty()) attrs[name] = value;
    }
    return attrs;
}

std::string HtmlParser::parse_attribute_value() {
    if (pos_ >= input_.length()) return "";
    char quote = input_[pos_];
    if (quote == '"' || quote == ''') {
        pos_++;
        std::string value;
        while (pos_ < input_.length() && input_[pos_] != quote) {
            if (input_[pos_] == '\' && pos_ + 1 < input_.length()) {
                value += input_[pos_+1];
                pos_ += 2;
            } else {
                value += input_[pos_++];
            }
        }
        if (pos_ < input_.length()) pos_++;
        return value;
    }
    std::string value;
    while (pos_ < input_.length() && !std::isspace(static_cast<unsigned char>(input_[pos_])) && input_[pos_] != '>' && input_[pos_] != '/') {
        value += input_[pos_++];
    }
    return value;
}

void HtmlParser::skip_whitespace() {
    while (pos_ < input_.length() && std::isspace(static_cast<unsigned char>(input_[pos_]))) pos_++;
}

HtmlDocument HtmlParser::parse(const std::string& html, const std::string& base_url) {
    input_ = html;
    pos_ = 0;

    auto doc = std::make_shared<DomNode>(DomNode::Document);
    std::vector<std::shared_ptr<DomNode>> stack;
    stack.push_back(doc);

    bool in_raw_text = false;
    std::string raw_tag;

    while (true) {
        auto token = next_token();
        if (token.type == Token::EOF) break;

        if (token.type == Token::Doctype) continue;

        if (token.type == Token::Comment) {
            auto node = std::make_shared<DomNode>(DomNode::Comment);
            node->text = token.text;
            stack.back()->children.push_back(node);
            node->parent = stack.back();
            continue;
        }

        if (in_raw_text) {
            if (token.type == Token::TagClose && token.tag == raw_tag) {
                in_raw_text = false;
                raw_tag.clear();
            } else {
                auto node = std::make_shared<DomNode>(DomNode::Text);
                if (token.type == Token::Text) node->text = token.text;
                else if (token.type == Token::TagOpen) {
                    node->text = "<" + token.tag;
                    for (const auto& [k, v] : token.attrs) node->text += " " + k + "=\"" + v + "\"";
                    if (token.self_closing) node->text += "/";
                    node->text += ">";
                } else if (token.type == Token::TagClose) {
                    node->text = "</" + token.tag + ">";
                }
                stack.back()->children.push_back(node);
                node->parent = stack.back();
            }
            continue;
        }

        if (token.type == Token::Text) {
            auto node = std::make_shared<DomNode>(DomNode::Text);
            node->text = token.text;
            stack.back()->children.push_back(node);
            node->parent = stack.back();
            continue;
        }

        if (token.type == Token::TagClose) {
            // Pop until we find matching tag
            while (!stack.empty() && stack.back()->tag != token.tag) {
                stack.pop_back();
            }
            if (!stack.empty()) stack.pop_back();
            continue;
        }

        if (token.type == Token::TagOpen) {
            // Check for raw text tags
            if (std::find(RAW_TEXT_TAGS.begin(), RAW_TEXT_TAGS.end(), token.tag) != RAW_TEXT_TAGS.end()) {
                in_raw_text = true;
                raw_tag = token.tag;
            }

            auto_close(stack, token.tag);

            auto node = std::make_shared<DomNode>(DomNode::Element);
            node->tag = token.tag;
            node->attributes = token.attrs;

            if (!stack.empty()) {
                stack.back()->children.push_back(node);
                node->parent = stack.back();
                // Set sibling links
                auto& siblings = stack.back()->children;
                if (siblings.size() > 1) {
                    auto prev = siblings[siblings.size() - 2];
                    prev->next_sibling = node;
                    node->prev_sibling = prev;
                }
            }

            if (!token.self_closing && !is_void_tag(token.tag)) {
                stack.push_back(node);
            }
        }
    }

    HtmlDocument result(doc);
    result.base_url = base_url;
    return result;
}

} // namespace scrapling
