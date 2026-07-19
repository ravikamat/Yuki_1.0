#include <iostream>
#include <string>

#include "scrapling/fetcher/http_fetcher.hpp"
#include "scrapling/parser/selector.hpp"

int main() {
    // Simple test: fetch a known URL and extract the title.
    std::string url = "https://example.com";
    scrapling::HttpFetcher fetcher;
    std::string html = fetcher.fetch(url);
    if (html.empty()) {
        std::cerr << "Failed to fetch content from " << url << std::endl;
        return 1;
    }
    scrapling::Selector selector(html, url);
    std::string title = selector.select("title").text();
    std::cout << "Title: " << title << std::endl;
    // Basic verification: title should contain "Example Domain".
    if (title.find("Example Domain") == std::string::npos) {
        std::cerr << "Unexpected title content" << std::endl;
        return 1;
    }
    return 0;
}
