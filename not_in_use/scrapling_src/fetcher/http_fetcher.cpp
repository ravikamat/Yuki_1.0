#include "http_fetcher.hpp"
#include <curl/curl.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace scrapling {

// TLS/JA3 signature strings for fingerprint impersonation
static const std::map<TlsProfile, std::string> JA3_STRINGS = {
    {TlsProfile::Chrome110, "771,4865-4866-4867-49195-49199-49196-49200-52393-52392-49171-49172-156-157-47-53,0-23-65281-10-11-35-16-5-13-18-51-45-43-27-17513-21,29-23-24,0"},
    {TlsProfile::Chrome116, "771,4865-4866-4867-49195-49199-49196-49200-52393-52392-49171-49172-156-157-47-53,0-23-65281-10-11-35-16-5-13-18-51-45-43-27-17513-21,29-23-24-25,0"},
    {TlsProfile::Firefox117, "771,4865-4867-4866-49195-49199-52393-52392-49196-49200-49171-49172-156-157-47-53,0-23-65281-10-11-35-16-5-51-43-13-45-28-65037,29-23-24-25,0"},
    {TlsProfile::Safari16_5, "771,4865-4866-4867-49196-49195-52393-49200-49199-52392-49162-49161-49172-49171-157-156-53-47-49160-49170-10,0-23-65281-10-11-35-16-5-13-18-51-45-43-27-17513,29-23-24,0"},
    {TlsProfile::Edge117, "771,4865-4866-4867-49195-49199-49196-49200-52393-52392-49171-49172-156-157-47-53,0-23-65281-10-11-35-16-5-13-18-51-45-43-27-17513-21,29-23-24-25,0"},
};

static const std::map<TlsProfile, std::string> USER_AGENTS = {
    {TlsProfile::Chrome110, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36"},
    {TlsProfile::Chrome116, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36"},
    {TlsProfile::Firefox117, "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/117.0"},
    {TlsProfile::Safari16_5, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.5 Safari/605.1.15"},
    {TlsProfile::Edge117, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/117.0.0.0 Safari/537.36 Edg/117.0.0.0"},
};

struct CurlWriteBuffer {
    std::string data;
};

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<CurlWriteBuffer*>(userdata);
    buf->data.append(ptr, size * nmemb);
    return size * nmemb;
}

static size_t header_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(ptr, size * nmemb);

    // Parse "Name: value" format
    auto colon = line.find(':');
    if (colon != std::string::npos && colon > 0) {
        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim leading space from value
        auto start = value.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) value = value.substr(start);
        // Trim trailing newline
        auto end = value.find_last_not_of("\r\n");
        if (end != std::string::npos) value = value.substr(0, end + 1);
        (*headers)[name] = value;
    }
    return size * nmemb;
}

HttpFetcher::HttpFetcher(FetcherConfig config) : config_(std::move(config)) {
    init_curl();
    if (config_.user_agent.empty()) {
        auto it = USER_AGENTS.find(config_.tls_profile);
        if (it != USER_AGENTS.end()) config_.user_agent = it->second;
    }
}

HttpFetcher::~HttpFetcher() {
    cleanup_curl();
}

void HttpFetcher::init_curl() {
    curl_handle_ = curl_easy_init();
    if (!curl_handle_) throw std::runtime_error("Failed to initialize CURL");

    CURL* curl = static_cast<CURL*>(curl_handle_);

    // Default options
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, config_.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(config_.max_redirects));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(config_.timeout_ms));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(config_.connect_timeout_ms));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, config_.verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, config_.verify_ssl ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

    // Accept encoding
    std::string encodings;
    if (config_.brotli) encodings += "br,";
    if (config_.gzip) encodings += "gzip,";
    if (config_.deflate) encodings += "deflate,";
    if (!encodings.empty()) {
        encodings.pop_back(); // remove trailing comma
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, encodings.c_str());
    }

    // HTTP/2
    if (config_.http2) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    }

    // Cookie jar
    if (!config_.cookie_jar_file.empty()) {
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, config_.cookie_jar_file.c_str());
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, config_.cookie_jar_file.c_str());
    }

    apply_tls_profile(config_.tls_profile);
}

void HttpFetcher::cleanup_curl() {
    if (curl_handle_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
        curl_handle_ = nullptr;
    }
}

void HttpFetcher::apply_tls_profile(TlsProfile profile) {
    CURL* curl = static_cast<CURL*>(curl_handle_);

    // Set cipher list based on profile
    switch (profile) {
        case TlsProfile::Chrome110:
        case TlsProfile::Chrome116:
        case TlsProfile::Edge117:
            curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST,
                "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
                "ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:"
                "AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA:AES256-SHA");
            break;
        case TlsProfile::Firefox117:
            curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST,
                "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
                "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:"
                "ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384");
            break;
        case TlsProfile::Safari16_5:
            curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST,
                "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
                "ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:"
                "AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA:AES256-SHA");
            break;
        default:
            break;
    }

    // Set TLS version minimum
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
}

std::string HttpFetcher::build_url(const std::string& base, const std::map<std::string, std::string>& params) {
    if (params.empty()) return base;
    std::string url = base;
    url += (base.find('?') == std::string::npos) ? '?' : '&';
    bool first = true;
    for (const auto& [key, val] : params) {
        if (!first) url += '&';
        first = false;
        char* escaped = curl_easy_escape(static_cast<CURL*>(curl_handle_), key.c_str(), static_cast<int>(key.length()));
        url += escaped;
        url += '=';
        curl_free(escaped);
        escaped = curl_easy_escape(static_cast<CURL*>(curl_handle_), val.c_str(), static_cast<int>(val.length()));
        url += escaped;
        curl_free(escaped);
    }
    return url;
}

Response HttpFetcher::request(const std::string& method, const std::string& url,
                               const std::string& body,
                               const std::map<std::string, std::string>& extra_headers) {
    CURL* curl = static_cast<CURL*>(curl_handle_);

    // Reset handle for new request
    curl_easy_reset(curl);
    init_curl(); // Re-apply defaults

    // Build headers
    curl_slist* headers = nullptr;

    // Add default headers
    if (!config_.user_agent.empty()) {
        std::string ua = "User-Agent: " + config_.user_agent;
        headers = curl_slist_append(headers, ua.c_str());
    }
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.5");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br");
    headers = curl_slist_append(headers, "DNT: 1");
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
    headers = curl_slist_append(headers, "Sec-Fetch-Dest: document");
    headers = curl_slist_append(headers, "Sec-Fetch-Mode: navigate");
    headers = curl_slist_append(headers, "Sec-Fetch-Site: none");
    headers = curl_slist_append(headers, "Sec-Fetch-User: ?1");
    headers = curl_slist_append(headers, "Cache-Control: max-age=0");

    // Add configured headers
    for (const auto& [name, value] : config_.headers) {
        std::string h = name + ": " + value;
        headers = curl_slist_append(headers, h.c_str());
    }

    // Add extra headers
    for (const auto& [name, value] : extra_headers) {
        std::string h = name + ": " + value;
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Method
    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.length()));
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    // Proxy
    if (!config_.proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, config_.proxy.c_str());
    }

    // Response buffers
    CurlWriteBuffer body_buf;
    std::map<std::string, std::string> response_headers;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

    // Execute
    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count() / 1000.0;

    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }

    // Get response info
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    char* final_url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
    std::string effective_url = final_url ? final_url : url;

    return Response(static_cast<int>(status_code), effective_url, body_buf.data, 
                    response_headers, elapsed, config_.selector_config);
}

Response HttpFetcher::get(const std::string& url, const std::map<std::string, std::string>& params) {
    return request("GET", build_url(url, params), "", {});
}

Response HttpFetcher::post(const std::string& url, const std::string& body, const std::string& content_type) {
    auto headers = std::map<std::string, std::string>{{"Content-Type", content_type}};
    return request("POST", url, body, headers);
}

Response HttpFetcher::post_json(const std::string& url, const nlohmann::json& json) {
    return request("POST", url, json.dump(), {{"Content-Type", "application/json"}});
}

Response HttpFetcher::put(const std::string& url, const std::string& body) {
    return request("PUT", url, body, {});
}

Response HttpFetcher::del(const std::string& url) {
    return request("DELETE", url, "", {});
}

Response HttpFetcher::head(const std::string& url) {
    return request("HEAD", url, "", {});
}

Response HttpFetcher::patch(const std::string& url, const std::string& body) {
    return request("PATCH", url, body, {});
}

void HttpFetcher::set_cookie(const std::string& name, const std::string& value, const std::string& domain) {
    CURL* curl = static_cast<CURL*>(curl_handle_);
    std::string cookie = name + "=" + value;
    if (!domain.empty()) cookie += "; domain=" + domain;
    curl_easy_setopt(curl, CURLOPT_COOKIE, cookie.c_str());
}

void HttpFetcher::clear_cookies() {
    CURL* curl = static_cast<CURL*>(curl_handle_);
    curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL");
}

void HttpFetcher::save_cookies(const std::string& path) {
    CURL* curl = static_cast<CURL*>(curl_handle_);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, path.c_str());
}

void HttpFetcher::load_cookies(const std::string& path) {
    CURL* curl = static_cast<CURL*>(curl_handle_);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, path.c_str());
}

void HttpFetcher::set_header(const std::string& name, const std::string& value) {
    config_.headers[name] = value;
}

void HttpFetcher::remove_header(const std::string& name) {
    config_.headers.erase(name);
}

void HttpFetcher::set_proxy(const std::string& proxy_url) {
    config_.proxy = proxy_url;
}

void HttpFetcher::set_timeout(int ms) {
    config_.timeout_ms = ms;
}

void HttpFetcher::set_user_agent(const std::string& ua) {
    config_.user_agent = ua;
}

// Static methods
Response HttpFetcher::fetch(const std::string& method, const std::string& url, const FetcherConfig& config) {
    HttpFetcher fetcher(config);
    return fetcher.request(method, url, "", {});
}

Response HttpFetcher::get(const std::string& url, const FetcherConfig& config) {
    return fetch("GET", url, config);
}

Response HttpFetcher::post(const std::string& url, const std::string& body, const FetcherConfig& config) {
    HttpFetcher fetcher(config);
    return fetcher.post(url, body);
}

} // namespace scrapling
