// SmartScraper.cpp — Yuki_1.0
// HTTP via libcurl (vcpkg). HTML parsing via regex. Zero scrapling C++ headers.
#define NOMINMAX
#define CURL_STATICLIB
#include "SmartScraper.h"

#include <curl/curl.h>
#include <chrono>
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <sstream>

// ── libcurl write callback ────────────────────────────────────────────────────

static size_t curl_write(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ── Pimpl ─────────────────────────────────────────────────────────────────────

struct SmartScraper::Impl {
    // TLS profile_id → curl SSL options mapping
    // 0=Chrome116, 1=Firefox117, 2=Safari16_5, 3=Edge117
    static constexpr const char* user_agents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:117.0) "
        "Gecko/20100101 Firefox/117.0",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 13_5) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/16.5 Safari/605.1.15",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36 Edg/117.0.0.0",
    };
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool is_blocked(const std::string& url,
                        const std::unordered_set<std::string>& blocked) {
    for (const auto& d : blocked)
        if (url.find(d) != std::string::npos) return true;
    return false;
}

// Lightweight HTML → plain text
static std::string html_to_text(const std::string& html) {
    static const std::regex script_re(
        R"(<(script|style)[^>]*>[\s\S]*?</(script|style)>)", std::regex::icase);
    static const std::regex tag_re("<[^>]+>");
    static const std::regex ws_re(R"(\s{2,})");

    std::string s = std::regex_replace(html, script_re, " ");
    s = std::regex_replace(s, tag_re, " ");
    s = std::regex_replace(s, ws_re, " ");

    for (auto& [ent, ch] : std::vector<std::pair<std::string,std::string>>{
            {"&amp;","&"},{"&lt;","<"},{"&gt;",">"},
            {"&quot;","\""},{"&apos;","'"},{"&nbsp;"," "}})
    {
        std::string::size_type p = 0;
        while ((p = s.find(ent, p)) != std::string::npos) {
            s.replace(p, ent.size(), ch); p += ch.size();
        }
    }
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    return (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
}

static std::string extract_tag(const std::string& html, const char* tag) {
    std::string open = std::string("<") + tag;
    std::string close = std::string("</") + tag + ">";
    auto start = html.find(open);
    if (start == std::string::npos) return {};
    start = html.find('>', start);
    if (start == std::string::npos) return {};
    ++start;
    auto end = html.find(close, start);
    if (end == std::string::npos) return {};
    return html_to_text(html.substr(start, end - start));
}

static std::vector<std::string> extract_attrs(const std::string& html,
                                               const char* tag,
                                               const char* attr) {
    std::vector<std::string> results;
    std::string pattern = std::string(tag) + R"([^>]*\s)" + attr + R"(\s*=\s*["']([^"']+)["'])";
    std::regex re(pattern, std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), re), end; it != end; ++it)
        results.push_back((*it)[1].str());
    return results;
}

// ── HTTP fetch via libcurl ────────────────────────────────────────────────────

static std::string curl_fetch(const std::string& url, int timeout_ms,
                               const char* ua) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      ua);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,     (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)(timeout_ms / 3));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS,      5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // ignore cert errors
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // auto gzip/br

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        std::cerr << "[SmartScraper] curl error: " << curl_easy_strerror(res) << "\n";

    curl_easy_cleanup(curl);
    return body;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

SmartScraper::SmartScraper() : impl_(std::make_unique<Impl>()) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

SmartScraper::~SmartScraper() {
    curl_global_cleanup();
}

// ── Configuration ─────────────────────────────────────────────────────────────

void SmartScraper::set_tls_profile(int profile_id) {
    tls_profile_id_ = (profile_id >= 0 && profile_id < 4) ? profile_id : 0;
}

void SmartScraper::add_blocked_domain(const std::string& domain) {
    blocked_domains_.insert(domain);
}

// ── scrape() ──────────────────────────────────────────────────────────────────

ScrapedPage SmartScraper::scrape(const std::string& url, int timeout_ms) {
    ScrapedPage page;
    if (is_blocked(url, blocked_domains_)) {
        std::cerr << "[SmartScraper] Blocked: " << url << "\n";
        return page;
    }

    auto start = std::chrono::steady_clock::now();

    const char* ua = SmartScraper::Impl::user_agents[tls_profile_id_];
    std::string html = curl_fetch(url, timeout_ms, ua);
    if (html.empty()) return page;

    page.title      = extract_tag(html, "title");
    page.clean_text = html_to_text(html);
    page.links      = extract_attrs(html, "a",   "href");
    page.images     = extract_attrs(html, "img", "src");

    // JSON-LD
    static const std::regex jsonld_re(
        R"(<script[^>]+type\s*=\s*["']application/ld\+json["'][^>]*>([\s\S]*?)</script>)",
        std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), jsonld_re), end; it != end; ++it)
        page.json_ld += (*it)[1].str();

    auto end_t = std::chrono::steady_clock::now();
    page.elapsed_ms = std::chrono::duration<double, std::milli>(end_t - start).count();
    return page;
}

ScrapedPage SmartScraper::scrape_with_browser(const std::string& url, bool /*headless*/) {
    return scrape(url, 30000);
}

// ── Legacy DocReader API ──────────────────────────────────────────────────────

std::string SmartScraper::fetchHtml(const std::string& url, int timeout_ms) {
    if (is_blocked(url, blocked_domains_)) return {};
    const char* ua = SmartScraper::Impl::user_agents[tls_profile_id_];
    return curl_fetch(url, timeout_ms, ua);
}

std::string SmartScraper::extractSemanticText(const std::string& html) {
    return html_to_text(html);
}
