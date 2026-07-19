#include "response.hpp"
#include "parser/selector.hpp"
#include <nlohmann/json.hpp>

namespace scrapling {

Response::Response(int status, std::string url, std::string body,
                   std::map<std::string, std::string> headers, double elapsed,
                   SelectorConfig config)
    : status_code_(status), url_(std::move(url)), body_(std::move(body)),
      headers_(std::move(headers)), elapsed_(elapsed), config_(std::move(config)) {}

std::optional<std::string> Response::header(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end()) return it->second;
    // Case-insensitive fallback
    for (const auto& [k, v] : headers_) {
        if (std::equal(k.begin(), k.end(), name.begin(), name.end(),
                       [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
            return v;
        }
    }
    return std::nullopt;
}

std::shared_ptr<Selector> Response::selector() const {
    if (!selector_) {
        selector_ = std::make_shared<Selector>(body_, url_, config_);
    }
    return selector_;
}

nlohmann::json Response::json() const {
    return nlohmann::json::parse(body_, nullptr, false);
}

std::string Response::text() const {
    return body_;
}

std::string Response::absolute_url(const std::string& relative) const {
    return url::join(url_, relative);
}

} // namespace scrapling
