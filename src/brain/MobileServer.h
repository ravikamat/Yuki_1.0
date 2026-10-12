#pragma once
// MobileServer.h — Lightweight HTTP server for mobile/browser access.
// NOTE: No Windows/Winsock headers here — they live in MobileServer.cpp only.
//       This prevents header-order conflicts with other compilation units.

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>

// ── MobileServer ──────────────────────────────────────────────────────────────

class MobileServer {
public:
    MobileServer();
    ~MobileServer();

    bool start(int port = 8765);
    void stop();
    bool isRunning() const { return running_.load(); }
    std::string localIp()  const;
    std::string localUrl() const {
        return running_ ? ("http://" + localIp() + ":" + std::to_string(port_)) : "";
    }

    using MessageHandler  = std::function<std::string(const std::string&)>;
    using StatusHandler   = std::function<std::string()>;
    using SkillsHandler   = std::function<std::string()>;
    using ConceptsHandler = std::function<std::string(int)>;

    void setMessageHandler(MessageHandler  fn);
    void setStatusHandler(StatusHandler    fn);
    void setSkillsHandler(SkillsHandler    fn);
    void setConceptsHandler(ConceptsHandler fn);

private:
    void acceptLoop();
    void handleClient(unsigned long long sock);   // SOCKET = UINT_PTR

    struct HttpRequest {
        std::string method;
        std::string path;
        std::string body;
    };
    HttpRequest parseRequest(const std::string& raw) const;
    std::string dispatch(const HttpRequest& req);

    static std::string httpOk(const std::string& body,
                               const std::string& ct = "application/json");
    static std::string httpNotFound();
    static std::string jsonEsc(const std::string& s);
    static std::string jsonGetText(const std::string& json);
    static std::string chatHtml(const std::string& ip, int port);

    // Concurrent client handling: each accepted connection runs on its own
    // detached thread, but the total is bounded so a flood of connections
    // cannot exhaust the process' thread/handle budget. stop() drains the
    // in-flight count so no handler outlives the server object.
    static constexpr int kMaxClients = 16;
    std::mutex              clientsMu_;
    std::condition_variable clientsCv_;
    int                     activeClients_ = 0;

    unsigned long long listenSock_ = static_cast<unsigned long long>(~0ULL);
    std::thread        acceptThread_;
    std::atomic<bool>  running_{false};
    int                port_ = 8765;

    MessageHandler  msgHandler_;
    StatusHandler   statusHandler_;
    SkillsHandler   skillsHandler_;
    ConceptsHandler conceptsHandler_;
    std::mutex      handlerMu_;

    static bool wsaInitialized_;
};
