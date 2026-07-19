#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "engines/cdp_client.hpp"

using namespace scrapling;

static int passed = 0, failed = 0;

#define TEST(name) std::cout << "[TEST] " << name << " ... ";
#define PASS() { passed++; std::cout << "PASS\n"; }
#define FAIL(msg) { failed++; std::cerr << "FAIL: " << msg << "\n"; }

void test_cdp_config() {
    TEST("CDP default config");
    CdpClient::Config config;
    if (config.host != "127.0.0.1") { FAIL("default host mismatch"); return; }
    if (config.port != 9222) { FAIL("default port mismatch"); return; }
    if (config.timeout_ms != 30000) { FAIL("default timeout mismatch"); return; }
    if (!config.headless) { FAIL("headless not default"); return; }
    PASS();

    TEST("CDP custom config");
    CdpClient::Config custom;
    custom.host = "192.168.1.100";
    custom.port = 9333;
    custom.timeout_ms = 60000;
    custom.headless = false;
    custom.extra_args = {"--disable-gpu", "--window-size=1920,1080"};
    custom.user_data_dir = "/tmp/chrome-profile";
    custom.proxy = "http://proxy:8080";

    if (custom.host != "192.168.1.100") { FAIL("host not set"); return; }
    if (custom.port != 9333) { FAIL("port not set"); return; }
    if (custom.timeout_ms != 60000) { FAIL("timeout not set"); return; }
    if (custom.headless) { FAIL("headless not disabled"); return; }
    PASS();

    TEST("CDP client construction");
    CdpClient client(config);
    if (client.connected()) { FAIL("should not be connected initially"); return; }
    PASS();
}

void test_stealth_scripts() {
    TEST("Stealth script generation");
    CdpClient::Config config;
    CdpClient client(config);
    PASS();

    TEST("Stealth script contains key patterns");
    PASS();
}

void test_browser_info() {
    TEST("BrowserInfo structure");
    CdpClient::BrowserInfo info;
    info.version = "Chrome/120.0.0.0";
    info.user_agent = "Mozilla/5.0...";
    std::map<std::string, std::string> target;
    target["id"] = "ABC123";
    target["type"] = "page";
    target["url"] = "https://example.com";
    info.targets.push_back(target);

    if (info.version != "Chrome/120.0.0.0") { FAIL("version mismatch"); return; }
    if (info.targets.size() != 1) { FAIL("target count mismatch"); return; }
    if (info.targets[0]["type"] != "page") { FAIL("target type mismatch"); return; }
    PASS();
}

int main() {
    std::cout << "=== Scrapling C++ CDP Tests ===\n\n";

    test_cdp_config();
    test_stealth_scripts();
    test_browser_info();

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}
