#pragma once
#include "../fetcher/response.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <memory>
#include <functional>

namespace scrapling {

// TLS/JA3 fingerprint impersonation profiles
enum class TlsProfile {
    Chrome110,    // Chrome 110 on Windows 10
    Chrome116,    // Chrome 116
    Firefox117,   // Firefox 117
    Safari16_5,   // Safari 16.5
    Edge117,      // Edge 117
    Custom
};

struct FetcherConfig {
    std::string user_agent;
    std::map<std::string, std::string> headers;
    std::string proxy;
    int timeout_ms = 30000;
    int connect_timeout_ms = 10000;
    bool follow_redirects = true;
    int max_redirects = 10;
    bool verify_ssl = true;
    std::string cookie_string;
    std::string cookie_jar_file;
    TlsProfile tls_profile = TlsProfile::Chrome116;
    bool http2 = true;
    bool brotli = true;
    bool gzip = true;
    bool deflate = true;
    SelectorConfig selector_config;
};

class HttpFetcher {
    FetcherConfig config_;
    void* curl_handle_ = nullptr; // CURL* (opaque to avoid curl header in public API)

public:
    explicit HttpFetcher(FetcherConfig config = {});
    ~HttpFetcher();

    // HTTP methods
    Response get(const std::string& url, const std::map<std::string, std::string>& params = {});
    Response post(const std::string& url, const std::string& body = "", 
                  const std::string& content_type = "application/x-www-form-urlencoded");
    Response post_json(const std::string& url, const nlohmann::json& json);
    Response put(const std::string& url, const std::string& body = "");
    Response del(const std::string& url);
    Response head(const std::string& url);
    Response patch(const std::string& url, const std::string& body = "");

    // Generic request
    Response request(const std::string& method, const std::string& url,
                     const std::string& body = "",
                     const std::map<std::string, std::string>& extra_headers = {});

    // Session management
    void set_cookie(const std::string& name, const std::string& value, const std::string& domain = "");
    void clear_cookies();
    void save_cookies(const std::string& path);
    void load_cookies(const std::string& path);

    // Configuration
    void set_header(const std::string& name, const std::string& value);
    void remove_header(const std::string& name);
    void set_proxy(const std::string& proxy_url);
    void set_timeout(int ms);
    void set_user_agent(const std::string& ua);

    // Static convenience methods
    static Response fetch(const std::string& method, const std::string& url,
                         const FetcherConfig& config = {});
    static Response get(const std::string& url, const FetcherConfig& config = {});
    static Response post(const std::string& url, const std::string& body = "",
                         const FetcherConfig& config = {});

private:
    void apply_tls_profile(TlsProfile profile);
    std::string build_url(const std::string& base, const std::map<std::string, std::string>& params);
    void init_curl();
    void cleanup_curl();
};

} // namespace scrapling
