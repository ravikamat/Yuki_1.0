#include "types.hpp"
#include <nlohmann/json.hpp>

namespace scrapling {

std::string TextHandler::as_json() const {
    nlohmann::json j = text_;
    return j.dump();
}

std::string AttributesHandler::as_json() const {
    nlohmann::json j = attrs_;
    return j.dump();
}

namespace url {
    std::string domain(const std::string& url) {
        size_t protocol_end = url.find("://");
        size_t start = (protocol_end == std::string::npos) ? 0 : protocol_end + 3;
        size_t end = url.find('/', start);
        if (end == std::string::npos) end = url.find('?', start);
        if (end == std::string::npos) end = url.length();
        std::string host = url.substr(start, end - start);
        // Remove port if present
        size_t port = host.find(':');
        if (port != std::string::npos) host = host.substr(0, port);
        return host;
    }

    bool is_absolute(const std::string& url) {
        return url.find("://") != std::string::npos || url.starts_with("//");
    }

    std::string join(const std::string& base, const std::string& relative) {
        if (is_absolute(relative)) return relative;
        if (relative.starts_with("//")) {
            size_t proto = base.find("://");
            return (proto == std::string::npos ? "http:" : base.substr(0, proto+1)) + relative;
        }
        if (relative.starts_with("/")) {
            size_t proto = base.find("://");
            if (proto == std::string::npos) return relative;
            size_t host_end = base.find('/', proto + 3);
            return base.substr(0, host_end) + relative;
        }
        // Relative path
        size_t last_slash = base.rfind('/');
        if (last_slash == std::string::npos || last_slash < base.find("://") + 3) {
            return base + "/" + relative;
        }
        return base.substr(0, last_slash + 1) + relative;
    }
}

} // namespace scrapling
