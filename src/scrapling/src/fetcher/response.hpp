#pragma once
#include "types.hpp"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace scrapling {

class Selector;

// HTTP Response — mirrors Scrapling's Response
class Response {
    int status_code_ = 0;
    std::string url_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    double elapsed_ = 0.0;
    std::shared_ptr<Selector> selector_;
    SelectorConfig config_;

public:
    Response() = default;
    Response(int status, std::string url, std::string body, 
             std::map<std::string, std::string> headers, double elapsed,
             SelectorConfig config = {});

    int status_code() const { return status_code_; }
    const std::string& url() const { return url_; }
    const std::string& body() const { return body_; }
    const std::map<std::string, std::string>& headers() const { return headers_; }
    std::optional<std::string> header(const std::string& name) const;
    double elapsed() const { return elapsed_; }
    bool ok() const { return status_code_ >= 200 && status_code_ < 300; }

    // Lazy parser access
    std::shared_ptr<Selector> selector() const;

    // JSON parsing
    nlohmann::json json() const;

    // Text extraction (no HTML)
    std::string text() const;

    // URL helpers
    std::string absolute_url(const std::string& relative) const;
};

} // namespace scrapling
