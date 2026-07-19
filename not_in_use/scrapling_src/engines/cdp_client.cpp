#include "cdp_client.hpp"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <random>
#include <iomanip>

namespace scrapling {

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

struct WsConnection {
    ws_client::connection_ptr con;
    ws_client* client;
    std::mutex send_mutex;
};

static size_t curl_write_string(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

std::string CdpClient::http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init CURL");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("HTTP GET failed: ") + curl_easy_strerror(res));
    }
    return response;
}

CdpClient::BrowserInfo CdpClient::get_browser_info(const std::string& host, int port) {
    std::string url = "http://" + host + ":" + std::to_string(port) + "/json/version";
    auto json = nlohmann::json::parse(http_get(url), nullptr, false);

    BrowserInfo info;
    if (json.contains("Browser")) info.version = json["Browser"];
    if (json.contains("User-Agent")) info.user_agent = json["User-Agent"];

    // Get targets
    url = "http://" + host + ":" + std::to_string(port) + "/json/list";
    auto targets = nlohmann::json::parse(http_get(url), nullptr, false);
    if (targets.is_array()) {
        for (const auto& t : targets) {
            std::map<std::string, std::string> target;
            if (t.contains("id")) target["id"] = t["id"];
            if (t.contains("type")) target["type"] = t["type"];
            if (t.contains("url")) target["url"] = t["url"];
            if (t.contains("title")) target["title"] = t["title"];
            if (t.contains("webSocketDebuggerUrl")) target["ws_url"] = t["webSocketDebuggerUrl"];
            info.targets.push_back(target);
        }
    }
    return info;
}

std::string CdpClient::find_ws_url() {
    auto info = get_browser_info(config_.host, config_.port);

    if (!config_.target_id.empty()) {
        for (const auto& t : info.targets) {
            if (t.count("id") && t.at("id") == config_.target_id) {
                return t.at("ws_url");
            }
        }
        throw std::runtime_error("Target not found: " + config_.target_id);
    }

    // Find first page target
    for (const auto& t : info.targets) {
        if (t.count("type") && t.at("type") == "page") {
            target_id_ = t.at("id");
            return t.at("ws_url");
        }
    }

    throw std::runtime_error("No page target found");
}

CdpClient::CdpClient(Config config) : config_(std::move(config)) {}

CdpClient::~CdpClient() {
    disconnect();
}

void CdpClient::connect() {
    if (connected_) return;

    std::string ws_url = find_ws_url();

    ws_client* client = new ws_client();
    client->clear_access_channels(websocketpp::log::alevel::all);
    client->clear_error_channels(websocketpp::log::elevel::all);
    client->init_asio();

    websocketpp::lib::error_code ec;
    ws_client::connection_ptr con = client->get_connection(ws_url, ec);
    if (ec) throw std::runtime_error("Connection failed: " + ec.message());

    WsConnection* ws_conn = new WsConnection{con, client};
    ws_handle_ = ws_conn;

    con->set_open_handler([this, ws_conn](websocketpp::connection_hdl) {
        connected_ = true;
    });

    con->set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg) {
        handle_message(msg->get_payload());
    });

    con->set_close_handler([this](websocketpp::connection_hdl) {
        connected_ = false;
    });

    client->connect(con);

    // Start ASIO io_service in background
    receive_thread_ = std::thread([client]() {
        client->run();
    });

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while (!connected_) {
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() > 10) {
            throw std::runtime_error("Connection timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Enable required domains
    send_command_sync("Runtime.enable");
    send_command_sync("Page.enable");
    send_command_sync("Network.enable");
    send_command_sync("DOM.enable");

    // Create session
    auto result = send_command_sync("Target.attachToTarget", {{"targetId", target_id_}, {"flatten", true}});
    if (result.contains("sessionId")) {
        session_id_ = result["sessionId"];
    }
}

void CdpClient::disconnect() {
    if (!connected_) return;

    stop_receive_ = true;

    if (ws_handle_) {
        auto* ws_conn = static_cast<WsConnection*>(ws_handle_);
        if (ws_conn->con) {
            websocketpp::lib::error_code ec;
            ws_conn->con->close(websocketpp::close::status::normal, "Closing", ec);
        }
    }

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    if (ws_handle_) {
        auto* ws_conn = static_cast<WsConnection*>(ws_handle_);
        delete ws_conn->client;
        delete ws_conn;
        ws_handle_ = nullptr;
    }

    connected_ = false;
}

bool CdpClient::connected() const {
    return connected_;
}

void CdpClient::send_command(const std::string& method, const nlohmann::json& params) {
    if (!connected_) throw std::runtime_error("Not connected");

    int id = ++message_id_;
    nlohmann::json msg = {
        {"id", id},
        {"method", method},
        {"params", params}
    };
    if (!session_id_.empty()) {
        msg["sessionId"] = session_id_;
    }

    auto* ws_conn = static_cast<WsConnection*>(ws_handle_);
    std::lock_guard<std::mutex> lock(ws_conn->send_mutex);

    websocketpp::lib::error_code ec;
    ws_conn->client->send(ws_conn->con, msg.dump(), websocketpp::frame::opcode::text, ec);
    if (ec) throw std::runtime_error("Send failed: " + ec.message());
}

nlohmann::json CdpClient::send_command_sync(const std::string& method, const nlohmann::json& params) {
    int id = ++message_id_;

    nlohmann::json msg = {
        {"id", id},
        {"method", method},
        {"params", params}
    };
    if (!session_id_.empty()) {
        msg["sessionId"] = session_id_;
    }

    {
        std::unique_lock<std::mutex> lock(pending_mutex_);
        pending_responses_[id] = nullptr;
    }

    auto* ws_conn = static_cast<WsConnection*>(ws_handle_);
    std::lock_guard<std::mutex> lock(ws_conn->send_mutex);

    websocketpp::lib::error_code ec;
    ws_conn->client->send(ws_conn->con, msg.dump(), websocketpp::frame::opcode::text, ec);
    if (ec) throw std::runtime_error("Send failed: " + ec.message());

    // Wait for response
    std::unique_lock<std::mutex> lock(pending_mutex_);
    auto start = std::chrono::steady_clock::now();
    while (pending_responses_[id].is_null()) {
        auto status = pending_cv_.wait_for(lock, std::chrono::milliseconds(100));
        if (status == std::cv_status::timeout) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > config_.timeout_ms) {
                pending_responses_.erase(id);
                throw std::runtime_error("Command timeout: " + method);
            }
        }
    }

    auto result = pending_responses_[id];
    pending_responses_.erase(id);
    return result;
}

void CdpClient::handle_message(const std::string& msg) {
    auto json = nlohmann::json::parse(msg, nullptr, false);
    if (json.is_discarded()) return;

    if (json.contains("id")) {
        int id = json["id"];
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (pending_responses_.count(id)) {
            pending_responses_[id] = json;
            pending_cv_.notify_all();
        }
    } else if (json.contains("method")) {
        std::string method = json["method"];
        nlohmann::json params = json.value("params", nlohmann::json::object());

        if (all_events_handler_) {
            all_events_handler_(method, params);
        }

        if (event_handlers_.count(method)) {
            for (const auto& handler : event_handlers_[method]) {
                handler(method, params);
            }
        }
    }
}

void CdpClient::receive_loop() {
    // Handled by ASIO in background thread
}

Response CdpClient::navigate(const std::string& url, int wait_ms) {
    send_command_sync("Page.navigate", {{"url", url}});

    if (wait_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    } else {
        wait_for_network_idle();
    }

    return Response(200, current_url(), page_source(), {}, 0.0, {});
}

void CdpClient::reload(bool ignore_cache) {
    send_command_sync("Page.reload", {{"ignoreCache", ignore_cache}});
}

void CdpClient::go_back() {
    evaluate("history.back()");
}

void CdpClient::go_forward() {
    evaluate("history.forward()");
}

void CdpClient::click(const std::string& selector) {
    std::string script = R"(
        (function() {
            var el = document.querySelector(')" + selector + R"(');
            if (!el) throw new Error('Element not found: )" + selector + R"(');
            var rect = el.getBoundingClientRect();
            var event = new MouseEvent('click', {
                bubbles: true, cancelable: true,
                view: window, detail: 1,
                screenX: rect.left + rect.width/2 + window.screenX,
                screenY: rect.top + rect.height/2 + window.screenY,
                clientX: rect.left + rect.width/2,
                clientY: rect.top + rect.height/2
            });
            el.dispatchEvent(event);
            return true;
        })()
    )";
    evaluate(script);
}

void CdpClient::type(const std::string& selector, const std::string& text) {
    click(selector);
    std::string script = R"(
        (function() {
            var el = document.querySelector(')" + selector + R"(');
            if (!el) throw new Error('Element not found');
            el.focus();
            el.value = ')" + text + R"(';
            el.dispatchEvent(new Event('input', { bubbles: true }));
            el.dispatchEvent(new Event('change', { bubbles: true }));
            return true;
        })()
    )";
    evaluate(script);
}

void CdpClient::clear(const std::string& selector) {
    type(selector, "");
}

void CdpClient::select(const std::string& selector, const std::string& value) {
    std::string script = R"(
        (function() {
            var el = document.querySelector(')" + selector + R"(');
            if (!el) throw new Error('Element not found');
            el.value = ')" + value + R"(';
            el.dispatchEvent(new Event('change', { bubbles: true }));
            return true;
        })()
    )";
    evaluate(script);
}

void CdpClient::scroll_to(const std::string& selector) {
    std::string script = R"(
        (function() {
            var el = document.querySelector(')" + selector + R"(');
            if (!el) throw new Error('Element not found');
            el.scrollIntoView({ behavior: 'instant', block: 'center' });
            return true;
        })()
    )";
    evaluate(script);
}

void CdpClient::scroll_to(int x, int y) {
    evaluate("window.scrollTo(" + std::to_string(x) + ", " + std::to_string(y) + ");");
}

nlohmann::json CdpClient::evaluate(const std::string& script) {
    auto result = send_command_sync("Runtime.evaluate", {
        {"expression", script},
        {"returnByValue", true},
        {"awaitPromise", true}
    });

    if (result.contains("result") && result["result"].contains("value")) {
        return result["result"]["value"];
    }
    if (result.contains("result") && result["result"].contains("objectId")) {
        return result["result"]; // Object reference
    }
    if (result.contains("exceptionDetails")) {
        throw std::runtime_error("JS exception: " + result["exceptionDetails"]["text"].get<std::string>());
    }
    return nullptr;
}

void CdpClient::wait_for_selector(const std::string& selector, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto result = evaluate("document.querySelector('" + selector + "') !== null");
        if (result.is_boolean() && result.get<bool>()) return;

        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_ms) {
            throw std::runtime_error("Timeout waiting for selector: " + selector);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CdpClient::wait_for_navigation(int timeout_ms) {
    std::string last_url = current_url();
    auto start = std::chrono::steady_clock::now();
    while (current_url() == last_url) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_ms) {
            throw std::runtime_error("Timeout waiting for navigation");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CdpClient::wait_for_network_idle(int idle_ms, int timeout_ms) {
    std::atomic<int> pending_requests{0};
    std::atomic<bool> idle{false};

    on_event("Network.requestWillBeSent", [&pending_requests](const auto&, const auto&) {
        pending_requests++;
    });
    on_event("Network.loadingFinished", [&pending_requests, &idle, idle_ms](const auto&, const auto&) {
        pending_requests--;
        if (pending_requests <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idle_ms));
            idle = true;
        }
    });

    auto start = std::chrono::steady_clock::now();
    while (!idle) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_ms) {
            throw std::runtime_error("Timeout waiting for network idle");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::vector<uint8_t> CdpClient::screenshot(bool full_page) {
    nlohmann::json params = {{"format", "png"}};
    if (full_page) {
        params["captureBeyondViewport"] = true;
        params["fromSurface"] = true;
    }

    auto result = send_command_sync("Page.captureScreenshot", params);
    if (result.contains("data")) {
        std::string b64 = result["data"];
        // Base64 decode
        static const std::string b64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<uint8_t> out;
        int val = 0, valb = -8;
        for (char c : b64) {
            if (c == '=') break;
            auto pos = b64_chars.find(c);
            if (pos == std::string::npos) continue;
            val = (val << 6) + static_cast<int>(pos);
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
    return {};
}

std::vector<uint8_t> CdpClient::screenshot_element(const std::string& selector) {
    scroll_to(selector);
    std::string script = R"(
        (function() {
            var el = document.querySelector(')" + selector + R"(');
            if (!el) throw new Error('Element not found');
            var rect = el.getBoundingClientRect();
            return {x: rect.x, y: rect.y, width: rect.width, height: rect.height};
        })()
    )";
    auto clip = evaluate(script);

    auto result = send_command_sync("Page.captureScreenshot", {
        {"format", "png"},
        {"clip", {
            {"x", clip.value("x", 0.0)},
            {"y", clip.value("y", 0.0)},
            {"width", clip.value("width", 1.0)},
            {"height", clip.value("height", 1.0)},
            {"scale", 1.0}
        }}
    });

    if (result.contains("data")) {
        std::string b64 = result["data"];
        std::vector<uint8_t> out;
        static const std::string b64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int val = 0, valb = -8;
        for (char c : b64) {
            if (c == '=') break;
            auto pos = b64_chars.find(c);
            if (pos == std::string::npos) continue;
            val = (val << 6) + static_cast<int>(pos);
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
    return {};
}

std::vector<uint8_t> CdpClient::pdf(const std::map<std::string, std::string>& options) {
    nlohmann::json params = {{"printBackground", true}};
    for (const auto& [k, v] : options) {
        if (k == "landscape" || k == "displayHeaderFooter" || k == "printBackground") {
            params[k] = (v == "true");
        } else if (k == "paperWidth" || k == "paperHeight" || k == "scale" || 
                   k == "marginTop" || k == "marginBottom" || k == "marginLeft" || k == "marginRight") {
            params[k] = std::stod(v);
        } else {
            params[k] = v;
        }
    }

    auto result = send_command_sync("Page.printToPDF", params);
    if (result.contains("data")) {
        std::string b64 = result["data"];
        std::vector<uint8_t> out;
        static const std::string b64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int val = 0, valb = -8;
        for (char c : b64) {
            if (c == '=') break;
            auto pos = b64_chars.find(c);
            if (pos == std::string::npos) continue;
            val = (val << 6) + static_cast<int>(pos);
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
    return {};
}

std::string CdpClient::page_source() {
    auto result = send_command_sync("Runtime.evaluate", {
        {"expression", "document.documentElement.outerHTML"},
        {"returnByValue", true}
    });
    if (result.contains("result") && result["result"].contains("value")) {
        return result["result"]["value"].get<std::string>();
    }
    return "";
}

std::string CdpClient::page_title() {
    auto result = evaluate("document.title");
    if (result.is_string()) return result.get<std::string>();
    return "";
}

std::string CdpClient::current_url() {
    auto result = evaluate("location.href");
    if (result.is_string()) return result.get<std::string>();
    return "";
}

std::vector<std::map<std::string, std::string>> CdpClient::get_cookies() {
    auto result = send_command_sync("Network.getCookies");
    std::vector<std::map<std::string, std::string>> cookies;
    if (result.contains("cookies") && result["cookies"].is_array()) {
        for (const auto& c : result["cookies"]) {
            std::map<std::string, std::string> cookie;
            for (const auto& [k, v] : c.items()) {
                if (v.is_string()) cookie[k] = v;
                else if (v.is_number()) cookie[k] = std::to_string(v.get<double>());
                else if (v.is_boolean()) cookie[k] = v.get<bool>() ? "true" : "false";
            }
            cookies.push_back(cookie);
        }
    }
    return cookies;
}

void CdpClient::set_cookie(const std::string& name, const std::string& value, 
                             const std::string& domain, const std::string& path) {
    send_command_sync("Network.setCookie", {
        {"name", name},
        {"value", value},
        {"domain", domain},
        {"path", path}
    });
}

void CdpClient::delete_cookie(const std::string& name, const std::string& domain) {
    send_command_sync("Network.deleteCookies", {{"name", name}, {"domain", domain}});
}

void CdpClient::clear_cookies() {
    send_command_sync("Network.clearBrowserCookies");
}

void CdpClient::block_urls(const std::vector<std::string>& patterns) {
    send_command_sync("Network.setBlockedURLs", {{"urls", patterns}});
}

void CdpClient::set_extra_headers(const std::map<std::string, std::string>& headers) {
    nlohmann::json h = nlohmann::json::object();
    for (const auto& [k, v] : headers) {
        h[k] = v;
    }
    send_command_sync("Network.setExtraHTTPHeaders", {{"headers", h}});
}

void CdpClient::set_user_agent_override(const std::string& ua, const std::string& platform) {
    nlohmann::json params = {{"userAgent", ua}};
    if (!platform.empty()) params["platform"] = platform;
    send_command_sync("Network.setUserAgentOverride", params);
}

void CdpClient::set_viewport(int width, int height, double device_scale_factor) {
    send_command_sync("Emulation.setDeviceMetricsOverride", {
        {"width", width},
        {"height", height},
        {"deviceScaleFactor", device_scale_factor},
        {"mobile", false}
    });
}

void CdpClient::enable_stealth() {
    evaluate(get_stealth_script());
    evaluate(get_canvas_noise_script());
    evaluate(get_webdriver_hide_script());
}

void CdpClient::disable_webgl() {
    evaluate(R"(
        Object.defineProperty(WebGLRenderingContext.prototype, 'getParameter', {
            value: function(param) {
                if (param === 37445) return 'Intel Inc.';
                if (param === 37446) return 'Intel Iris OpenGL Engine';
                return getParameter(param);
            }
        });
        Object.defineProperty(WebGL2RenderingContext.prototype, 'getParameter', {
            value: function(param) {
                if (param === 37445) return 'Intel Inc.';
                if (param === 37446) return 'Intel Iris OpenGL Engine';
                return getParameter(param);
            }
        });
    )");
}

void CdpClient::hide_canvas() {
    evaluate(get_canvas_noise_script());
}

void CdpClient::block_webrtc() {
    evaluate(R"(
        Object.defineProperty(window, 'RTCPeerConnection', {
            value: undefined
        });
        Object.defineProperty(window, 'webkitRTCPeerConnection', {
            value: undefined
        });
    )");
}

void CdpClient::set_timezone(const std::string& tz) {
    send_command_sync("Emulation.setTimezoneOverride", {{"timezoneId", tz}});
}

void CdpClient::set_locale(const std::string& locale) {
    evaluate("Object.defineProperty(navigator, 'language', { value: '" + locale + "' });");
    evaluate("Object.defineProperty(navigator, 'languages', { value: ['" + locale + "', 'en-US', 'en'] });");
}

void CdpClient::on_event(const std::string& method, EventHandler handler) {
    event_handlers_[method].push_back(handler);
}

void CdpClient::on_all_events(EventHandler handler) {
    all_events_handler_ = handler;
}

std::string CdpClient::launch_browser(const std::string& chrome_path, 
                                       const std::vector<std::string>& args) {
    std::string cmd = chrome_path.empty() ? "google-chrome" : chrome_path;

    std::vector<std::string> all_args = {
        "--remote-debugging-port=9222",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-background-timer-throttling",
        "--disable-backgrounding-occluded-windows",
        "--disable-renderer-backgrounding",
        "--disable-features=TranslateUI",
        "--disable-component-extensions-with-background-pages",
        "--disable-default-apps",
        "--mute-audio",
        "--no-sandbox"
    };

    for (const auto& a : args) all_args.push_back(a);

    std::string full_cmd = cmd;
    for (const auto& a : all_args) full_cmd += " " + a;

    #ifdef _WIN32
        full_cmd = "start /B " + full_cmd;
    #else
        full_cmd += " &";
    #endif

    std::system(full_cmd.c_str());

    // Wait for browser to start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return "http://127.0.0.1:9222";
}

void CdpClient::kill_browser(const std::string& host, int port) {
    try {
        auto info = get_browser_info(host, port);
        for (const auto& t : info.targets) {
            if (t.count("id")) {
                std::string url = "http://" + host + ":" + std::to_string(port) + "/json/close/" + t.at("id");
                http_get(url);
            }
        }
    } catch (...) {
        // Browser may already be dead
    }
}

std::string CdpClient::get_stealth_script() {
    return R"(
        (function() {
            // Override navigator.webdriver
            Object.defineProperty(navigator, 'webdriver', { get: () => undefined });

            // Override permissions
            const originalQuery = window.navigator.permissions.query;
            window.navigator.permissions.query = (parameters) => (
                parameters.name === 'notifications' ?
                    Promise.resolve({ state: Notification.permission }) :
                    originalQuery(parameters)
            );

            // Hide automation flags
            window.chrome = { runtime: {} };

            // Override plugins
            Object.defineProperty(navigator, 'plugins', {
                get: function() {
                    return [
                        {name: 'Chrome PDF Plugin', filename: 'internal-pdf-viewer'},
                        {name: 'Chrome PDF Viewer', filename: 'mhjfbmdgcfjbbpaeojofohoefgiehjai'},
                        {name: 'Native Client', filename: 'internal-nacl-plugin'}
                    ];
                }
            });

            // Override mimeTypes
            Object.defineProperty(navigator, 'mimeTypes', {
                get: function() {
                    return [
                        {type: 'application/pdf', suffixes: 'pdf', description: ''},
                        {type: 'application/x-google-chrome-pdf', suffixes: 'pdf', description: ''}
                    ];
                }
            });

            // Override notification
            if (window.Notification) {
                Object.defineProperty(Notification, 'permission', { get: () => 'default' });
            }

            // Override battery
            if (navigator.getBattery) {
                navigator.getBattery = () => Promise.resolve({
                    charging: true,
                    chargingTime: 0,
                    dischargingTime: Infinity,
                    level: 1.0
                });
            }

            // Override device memory
            Object.defineProperty(navigator, 'deviceMemory', { get: () => 8 });

            // Override hardware concurrency
            Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 4 });

            return 'stealth enabled';
        })()
    )";
}

std::string CdpClient::get_canvas_noise_script() {
    return R"(
        (function() {
            const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
            const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;
            const originalGetLineDash = CanvasRenderingContext2D.prototype.getLineDash;

            const noise = () => {
                const val = Math.floor(Math.random() * 10) - 5;
                return val === 0 ? 1 : val;
            };

            CanvasRenderingContext2D.prototype.getImageData = function(x, y, w, h) {
                const imageData = originalGetImageData.call(this, x, y, w, h);
                for (let i = 0; i < imageData.data.length; i += 4) {
                    imageData.data[i] += noise();
                    imageData.data[i + 1] += noise();
                    imageData.data[i + 2] += noise();
                }
                return imageData;
            };

            HTMLCanvasElement.prototype.toDataURL = function() {
                const ctx = this.getContext('2d');
                if (ctx) {
                    const imageData = ctx.getImageData(0, 0, this.width, this.height);
                    for (let i = 0; i < imageData.data.length; i += 4) {
                        imageData.data[i] += noise();
                        imageData.data[i + 1] += noise();
                        imageData.data[i + 2] += noise();
                    }
                    ctx.putImageData(imageData, 0, 0);
                }
                return originalToDataURL.call(this);
            };

            return 'canvas noise enabled';
        })()
    )";
}

std::string CdpClient::get_webdriver_hide_script() {
    return R"(
        (function() {
            const newProto = navigator.__proto__;
            delete newProto.webdriver;
            navigator.__proto__ = newProto;

            Object.defineProperty(navigator, 'webdriver', {
                get: () => false,
                configurable: true
            });

            // Remove CDC indicators
            const chromeObj = window.chrome || {};
            if (chromeObj.csi) delete chromeObj.csi;
            if (chromeObj.loadTimes) delete chromeObj.loadTimes;

            return 'webdriver hidden';
        })()
    )";
}

} // namespace scrapling
