#pragma once
// SmartScraper.h  — Yuki_1.0
// Pimpl wrapper around the Scrapling HTTP fetcher.
// No scrapling types appear in this header so including it never pulls
// scrapling headers into unrelated translation units.

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

struct ScrapedPage {
    std::string title;
    std::string clean_text;
    std::vector<std::string> links;
    std::vector<std::string> images;
    std::string json_ld;
    std::vector<std::string> tables;
    double elapsed_ms = 0.0;
};

class SmartScraper {
public:
    SmartScraper();
    ~SmartScraper();

    // Primary scraping API
    ScrapedPage scrape(const std::string& url, int timeout_ms = 30000);
    ScrapedPage scrape_with_browser(const std::string& url, bool headless = true);

    // Legacy DocReader compatibility API
    std::string fetchHtml(const std::string& url, int timeout_ms = 30000);
    std::string extractSemanticText(const std::string& html);

    // Configuration — profile_id: 0=Chrome116 (default), 1=Firefox117, 2=Safari16_5, 3=Edge117
    void set_tls_profile(int profile_id);
    void add_blocked_domain(const std::string& domain);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int tls_profile_id_ = 0;
    std::unordered_set<std::string> blocked_domains_;
};
