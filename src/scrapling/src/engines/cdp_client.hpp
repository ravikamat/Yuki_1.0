#pragma once
#include "../fetcher/response.hpp"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace scrapling {

// Chrome DevTools Protocol client
class CdpClient {
public:
    struct Config {
        std::string host = "127.0.0.1";
        int port = 9222;
        std::string target_id; // empty = auto-connect to first available
        int timeout_ms = 30000;
        bool headless = true;
        std::vector<std::string> extra_args;
        std::string user_data_dir;
        std::string proxy;
    };

    struct BrowserInfo {
        std::string version;
        std::string user_agent;
        std::vector<std::map<std::string, std::string>> targets;
    };

    CdpClient(Config config = {});
    ~CdpClient();

    // Connection
    void connect();
    void disconnect();
    bool connected() const;

    // Navigation
    Response navigate(const std::string& url, int wait_ms = 0);
    void reload(bool ignore_cache = false);
    void go_back();
    void go_forward();

    // Page interaction
    void click(const std::string& selector);
    void type(const std::string& selector, const std::string& text);
    void clear(const std::string& selector);
    void select(const std::string& selector, const std::string& value);
    void scroll_to(const std::string& selector);
    void scroll_to(int x, int y);

    // JavaScript execution
    nlohmann::json evaluate(const std::string& script);

    // Wait conditions
    void wait_for_selector(const std::string& selector, int timeout_ms = 30000);
    void wait_for_navigation(int timeout_ms = 30000);
    void wait_for_network_idle(int idle_ms = 500, int timeout_ms = 30000);

    // Screenshot
    std::vector<uint8_t> screenshot(bool full_page = false);
    std::vector<uint8_t> screenshot_element(const std::string& selector);

    // PDF
    std::vector<uint8_t> pdf(const std::map<std::string, std::string>& options = {});

    // Content
    std::string page_source();
    std::string page_title();
    std::string current_url();

    // Cookies
    std::vector<std::map<std::string, std::string>> get_cookies();
    void set_cookie(const std::string& name, const std::string& value, 
                    const std::string& domain, const std::string& path = "/");
    void delete_cookie(const std::string& name, const std::string& domain);
    void clear_cookies();

    // Request interception (stealth)
    void block_urls(const std::vector<std::string>& patterns);
    void set_extra_headers(const std::map<std::string, std::string>& headers);
    void set_user_agent_override(const std::string& ua, const std::string& platform = "");
    void set_viewport(int width, int height, double device_scale_factor = 1.0);

    // Stealth features
    void enable_stealth();
    void disable_webgl();
    void hide_canvas();
    void block_webrtc();
    void set_timezone(const std::string& tz);
    void set_locale(const std::string& locale);

    // Events
    using EventHandler = std::function<void(const std::string& method, const nlohmann::json& params)>;
    void on_event(const std::string& method, EventHandler handler);
    void on_all_events(EventHandler handler);

    // Browser management
    static BrowserInfo get_browser_info(const std::string& host = "127.0.0.1", int port = 9222);
    static std::string launch_browser(const std::string& chrome_path = "", 
                                       const std::vector<std::string>& args = {});
    static void kill_browser(const std::string& host = "127.0.0.1", int port = 9222);

private:
    Config config_;
    void* ws_handle_ = nullptr; // WebSocket handle (opaque)
    std::atomic<bool> connected_{false};
    std::atomic<int> message_id_{0};
    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::map<int, nlohmann::json> pending_responses_;
    std::map<std::string, std::vector<EventHandler>> event_handlers_;
    EventHandler all_events_handler_;
    std::thread receive_thread_;
    std::atomic<bool> stop_receive_{false};
    std::string session_id_;
    std::string target_id_;

    void send_command(const std::string& method, const nlohmann::json& params = {});
    nlohmann::json send_command_sync(const std::string& method, const nlohmann::json& params = {});
    void receive_loop();
    void handle_message(const std::string& msg);
    std::string http_get(const std::string& url);
    std::string find_ws_url();

    // Stealth scripts
    static std::string get_stealth_script();
    static std::string get_canvas_noise_script();
    static std::string get_webdriver_hide_script();
};

} // namespace scrapling
