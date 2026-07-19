#include <iostream>
#include "fetcher/http_fetcher.hpp"

using namespace scrapling;

int main() {
    // Configure fetcher with Chrome TLS fingerprint
    FetcherConfig config;
    config.tls_profile = TlsProfile::Chrome116;
    config.timeout_ms = 15000;

    HttpFetcher fetcher(config);

    try {
        // GET request
        std::cout << "Fetching example.com...\n";
        auto resp = fetcher.get("https://example.com");

        std::cout << "Status: " << resp.status_code() << "\n";
        std::cout << "URL: " << resp.url() << "\n";
        std::cout << "Elapsed: " << resp.elapsed() << "s\n";

        if (resp.ok()) {
            auto sel = resp.selector();
            auto title = sel->css("title");
            if (title) {
                std::cout << "Title: " << title->text().raw() << "\n";
            }

            auto h1 = sel->css("h1");
            if (h1) {
                std::cout << "H1: " << h1->text().raw() << "\n";
            }

            auto links = sel->css_all("a");
            std::cout << "Links found: " << links.size() << "\n";
            for (const auto& link : links) {
                auto href = link->attr("href");
                auto text = link->text().raw();
                if (href) {
                    std::cout << "  " << text << " -> " << *href << "\n";
                }
            }
        }

        // POST example
        std::cout << "\nPOST example...\n";
        auto post_resp = fetcher.post("https://httpbin.org/post", 
            "name=test&value=123", "application/x-www-form-urlencoded");
        std::cout << "POST status: " << post_resp.status_code() << "\n";

        if (post_resp.ok()) {
            auto json = post_resp.json();
            if (!json.is_discarded()) {
                std::cout << "Response JSON keys: ";
                for (auto& [key, val] : json.items()) {
                    std::cout << key << " ";
                }
                std::cout << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
