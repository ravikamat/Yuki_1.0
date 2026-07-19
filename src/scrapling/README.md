# Scrapling C++

A production-grade C++ port of [Scrapling](https://github.com/D4Vinci/Scrapling) — undetectable, high-performance web scraping for the modern web.

## Features

- **HTML5 Parser** — Custom tokenizer + DOM tree with auto-closing, raw text handling, comment preservation
- **CSS Selector Engine** — Full CSS3 selector support: tag, class, id, attribute, pseudo-classes (`:nth-child`, `:first-child`, `:last-child`, `:not()`, `:has()`), combinators (`>`, `+`, `~`, ` `)
- **XPath Subset** — Simple XPath conversion to CSS (`//tag`, `/tag`, `[@attr='val']`, `[n]`)
- **HTTP Fetcher** — `libcurl`-based with TLS/JA3 fingerprint impersonation (Chrome, Firefox, Safari, Edge profiles), HTTP/2, Brotli/Gzip/Deflate, cookie jar, proxy support
- **CDP Browser Client** — Chrome DevTools Protocol via WebSocket: full browser automation, stealth injection (canvas noise, webdriver hide, WebRTC block), screenshot, PDF, request interception
- **Type System** — `TextHandler`, `AttributesHandler`, `Selector`, `Selectors` with lazy evaluation
- **Table/JSON-LD Extraction** — Structured data parsing built-in

## Architecture

```
scrapling/
├── src/
│   ├── core/          # Types, URL utilities, Text/Attribute handlers
│   ├── parser/        # HTML parser, CSS selector engine, DOM, Selector API
│   ├── fetcher/       # HTTP fetcher (curl + TLS impersonation), Response
│   └── engines/       # CDP browser client (WebSocket + Chrome DevTools)
├── tests/             # Unit tests (parser, fetcher, CDP)
├── examples/          # Usage examples
└── CMakeLists.txt     # Build configuration
```

## Building

### Requirements

- CMake 3.16+
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- libcurl (for HTTP fetcher)
- Boost.Asio + Boost.Thread (for WebSocket/CDP)
- nlohmann/json (fetched automatically)
- WebSocket++ (fetched automatically)

### Ubuntu/Debian

```bash
sudo apt-get install cmake libcurl4-openssl-dev libboost-all-dev

git clone https://github.com/yourusername/scrapling-cpp.git
cd scrapling-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure
```

### macOS

```bash
brew install cmake curl boost

git clone https://github.com/yourusername/scrapling-cpp.git
cd scrapling-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
ctest --output-on-failure
```

### Windows (vcpkg)

```powershell
vcpkg install curl boost-asio boost-thread nlohmann-json websocketpp

git clone https://github.com/yourusername/scrapling-cpp.git
cd scrapling-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
ctest -C Release --output-on-failure
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `ENABLE_CURL` | ON | Build HTTP fetcher (requires libcurl) |
| `ENABLE_WEBSOCKET` | ON | Build CDP browser client (requires Boost + WebSocket++) |

```bash
cmake .. -DENABLE_CURL=OFF -DENABLE_WEBSOCKET=OFF  # Parser only
```

## Usage

### Static Parsing

```cpp
#include "parser/selector.hpp"

using namespace scrapling;

// Parse HTML
Selector page(html, "https://example.com/");

// CSS selectors
auto title = page.css("h1.title");
std::cout << title->text().raw() << "
";

// Multiple matches
auto items = page.css_all(".product");
for (const auto& item : items) {
    auto name = item->css(".name");
    auto price = item->css(".price");
    auto link = item->css("a");

    std::cout << name->text().raw() << ": " << price->text().raw() << "
";
    std::cout << "Link: " << link->href() << "
";
}

// Table extraction
auto table = page.css("table.specs");
auto data = table->table();  // vector<vector<string>>

// JSON-LD structured data
auto jsonld = page.json_ld();  // vector<nlohmann::json>

// XPath
auto nodes = page.xpath_all("//div[@class='item']");
```

### HTTP Fetching

```cpp
#include "fetcher/http_fetcher.hpp"

using namespace scrapling;

// Configure with Chrome TLS fingerprint
FetcherConfig config;
config.tls_profile = TlsProfile::Chrome116;
config.timeout_ms = 15000;

HttpFetcher fetcher(config);

// GET
auto resp = fetcher.get("https://example.com");
if (resp.ok()) {
    auto sel = resp.selector();
    auto title = sel->css("title");
    std::cout << title->text().raw() << "
";
}

// POST JSON
auto post_resp = fetcher.post_json("https://api.example.com/data", 
    {{"key", "value"}, {"count", 42}});

// Custom headers
fetcher.set_header("Authorization", "Bearer token123");
fetcher.set_proxy("http://proxy:8080");
```

### Browser Automation (CDP)

```cpp
#include "engines/cdp_client.hpp"

using namespace scrapling;

// Launch Chrome with remote debugging
CdpClient::launch_browser("/usr/bin/google-chrome", {"--headless"});

// Connect
CdpClient::Config config;
CdpClient browser(config);
browser.connect();

// Navigate and enable stealth
browser.navigate("https://example.com");
browser.enable_stealth();
browser.set_viewport(1920, 1080);

// Interact
browser.click("#submit-btn");
browser.type("#search", "query");
browser.wait_for_selector(".results");

// Extract
auto html = browser.page_source();
Selector page(html, browser.current_url());
auto results = page.css_all(".result");

// Screenshot
auto png = browser.screenshot(true);  // full page
std::ofstream("page.png", std::ios::binary).write(
    reinterpret_cast<const char*>(png.data()), png.size());

// Cleanup
browser.disconnect();
CdpClient::kill_browser();
```

## CSS Selector Support

| Selector | Example | Status |
|----------|---------|--------|
| Tag | `div` | ✅ |
| Class | `.item` | ✅ |
| ID | `#header` | ✅ |
| Attribute | `[href]`, `[type='text']` | ✅ |
| Attribute contains word | `[class~='active']` | ✅ |
| Attribute starts with | `[href^='https']` | ✅ |
| Attribute ends with | `[href$='.pdf']` | ✅ |
| Attribute contains | `[href*='example']` | ✅ |
| Descendant | `div p` | ✅ |
| Child | `div > p` | ✅ |
| Adjacent sibling | `h1 + p` | ✅ |
| General sibling | `h1 ~ p` | ✅ |
| `:first-child` | `li:first-child` | ✅ |
| `:last-child` | `li:last-child` | ✅ |
| `:nth-child(n)` | `li:nth-child(2)` | ✅ |
| `:nth-of-type(n)` | `p:nth-of-type(1)` | ✅ |
| `:not()` | `div:not(.exclude)` | ✅ |
| `:has()` | `div:has(a)` | ✅ |
| `:empty` | `div:empty` | ✅ |
| Multiple classes | `.a.b.c` | ✅ |
| Combinations | `div.item > p:first-child` | ✅ |

## TLS/JA3 Fingerprint Profiles

| Profile | Browser | Platform |
|-----------|---------|----------|
| `Chrome110` | Chrome 110 | Windows 10 |
| `Chrome116` | Chrome 116 | Windows 10 |
| `Firefox117` | Firefox 117 | Windows 10 |
| `Safari16_5` | Safari 16.5 | macOS |
| `Edge117` | Edge 117 | Windows 10 |

Each profile sets the correct cipher list, TLS extensions, and ALPN to match the real browser's JA3 fingerprint, bypassing TLS-level bot detection.

## Stealth Features (CDP)

- **Canvas noise** — Random pixel perturbation on `getImageData`/`toDataURL`
- **WebDriver hide** — Remove `navigator.webdriver`, CDC indicators
- **WebRTC block** — Disable `RTCPeerConnection` to prevent IP leak
- **WebGL override** — Spoof vendor/renderer strings
- **Timezone/locale** — Override browser timezone and language
- **User agent override** — Set custom UA with platform
- **Viewport** — Set device metrics
- **Request blocking** — Block URLs by pattern
- **Extra headers** — Inject custom headers on all requests

## Testing

```bash
cd build

# Parser tests (no dependencies)
./test_parser

# Fetcher tests (no network)
./test_fetcher

# CDP tests (no browser)
./test_cdp

# All tests
ctest --output-on-failure
```

## License

BSD 3-Clause — same as original Scrapling.

## Credits

Original Python Scrapling by [Karim Shoair (D4Vinci)](https://github.com/D4Vinci).
C++ port preserves the architecture, API design, and anti-detection philosophy.
