// MobileServer.cpp — Lightweight Winsock HTTP server for mobile/browser access
// All Windows/Winsock headers are isolated here; NONE bleed into other TUs.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "MobileServer.h"
#include "core/ResponseResolver.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>

// ── Cast helpers (bridge header SOCKET to unsigned long long) ─────────────────
static inline SOCKET toSOCKET(unsigned long long v) {
    return static_cast<SOCKET>(v);
}
static inline unsigned long long fromSOCKET(SOCKET s) {
    return static_cast<unsigned long long>(s);
}
static const unsigned long long INVALID_ULL = fromSOCKET(INVALID_SOCKET);

bool MobileServer::wsaInitialized_ = false;

// ── Constructor / Destructor ──────────────────────────────────────────────────

MobileServer::MobileServer() {
    if (!wsaInitialized_) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) wsaInitialized_ = true;
    }
}

MobileServer::~MobileServer() { stop(); }

// ── Start / Stop ──────────────────────────────────────────────────────────────

bool MobileServer::start(int port) {
    if (running_) return true;
    port_ = port;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        std::cerr << "[MobileServer] socket() failed\n"; return false;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<u_short>(port_));

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(s, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[MobileServer] bind/listen failed on port " << port_ << "\n";
        closesocket(s); return false;
    }

    listenSock_ = fromSOCKET(s);
    running_    = true;
    acceptThread_ = std::thread(&MobileServer::acceptLoop, this);

    std::string ip = localIp();
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║  Yuki Mobile Access — READY                  ║\n";
    std::cout << "║                                              ║\n";
    std::cout << "║  Open on your phone / browser:               ║\n";
    std::cout << "║  http://" << ip << ":" << port_
              << std::string(std::max(0, 35 - static_cast<int>(ip.size())), ' ') << "║\n";
    std::cout << "║                                              ║\n";
    std::cout << "║  POST /message  {\"text\":\"hello yuki\"}        ║\n";
    std::cout << "║  GET  /status  /skills  /concepts            ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n\n";
    return true;
}

void MobileServer::stop() {
    running_ = false;
    SOCKET s = toSOCKET(listenSock_);
    if (s != INVALID_SOCKET) {
        closesocket(s);
        listenSock_ = INVALID_ULL;
    }
    if (acceptThread_.joinable()) acceptThread_.join();

    // Wait for any in-flight client handler threads to finish before the
    // object (and its handler state) is destroyed. Clients are short-lived
    // (Connection: close), so this is bounded in practice.
    {
        std::unique_lock<std::mutex> l(clientsMu_);
        clientsCv_.wait_for(l, std::chrono::seconds(5),
                             [this] { return activeClients_ == 0; });
    }
}

// ── Local IP ──────────────────────────────────────────────────────────────────

std::string MobileServer::localIp() const {
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0) return "127.0.0.1";
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) return "127.0.0.1";
    char ip[INET_ADDRSTRLEN] = {};
    if (res) {
        inet_ntop(AF_INET,
                  &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr,
                  ip, sizeof(ip));
        freeaddrinfo(res);
    }
    return std::string(ip[0] ? ip : "127.0.0.1");
}

// ── Handlers ──────────────────────────────────────────────────────────────────

void MobileServer::setMessageHandler(MessageHandler fn)
    { std::lock_guard<std::mutex> l(handlerMu_); msgHandler_  = std::move(fn); }
void MobileServer::setStatusHandler(StatusHandler fn)
    { std::lock_guard<std::mutex> l(handlerMu_); statusHandler_  = std::move(fn); }
void MobileServer::setSkillsHandler(SkillsHandler fn)
    { std::lock_guard<std::mutex> l(handlerMu_); skillsHandler_  = std::move(fn); }
void MobileServer::setConceptsHandler(ConceptsHandler fn)
    { std::lock_guard<std::mutex> l(handlerMu_); conceptsHandler_ = std::move(fn); }

// ── Accept loop ───────────────────────────────────────────────────────────────

void MobileServer::acceptLoop() {
    SOCKET ls = toSOCKET(listenSock_);
    while (running_) {
        sockaddr_in ca{};
        int len = sizeof(ca);
        SOCKET cs = accept(ls, reinterpret_cast<sockaddr*>(&ca), &len);
        if (cs == INVALID_SOCKET) break;

        // Enforce the concurrent-client cap. If we are already saturated,
        // shed the connection with a 503 rather than unboundedly spawning
        // threads (audit §3). Wait briefly for a slot to free up first so
        // short bursts are still served instead of rejected outright.
        {
            std::unique_lock<std::mutex> l(clientsMu_);
            if (activeClients_ >= kMaxClients) {
                clientsCv_.wait_for(l, std::chrono::milliseconds(250), [this] {
                    return activeClients_ < kMaxClients || !running_.load();
                });
            }
            if (!running_) { closesocket(cs); break; }
            if (activeClients_ >= kMaxClients) {
                const std::string busyBody =
                    "{\"error\":\"server busy, retry shortly\",\"ok\":false}";
                const std::string busy =
                    "HTTP/1.1 503 Service Unavailable\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Type: application/json\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + std::to_string(busyBody.size()) +
                    "\r\n\r\n" + busyBody;
                ::send(cs, busy.c_str(), static_cast<int>(busy.size()), 0);
                closesocket(cs);
                continue;
            }
            ++activeClients_;
        }

        unsigned long long csULL = fromSOCKET(cs);
        std::thread([this, csULL]() {
            handleClient(csULL);
            std::lock_guard<std::mutex> l(clientsMu_);
            --activeClients_;
            clientsCv_.notify_one();
        }).detach();
    }
}

// ── Handle one client ─────────────────────────────────────────────────────────

void MobileServer::handleClient(unsigned long long sockULL) {
    SOCKET sock = toSOCKET(sockULL);
    std::string raw;
    char buf[4096];
    int  received;
    while ((received = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[received] = '\0';
        raw += buf;
        if (raw.find("\r\n\r\n") != std::string::npos) {
            auto clpos = raw.find("Content-Length: ");
            if (clpos == std::string::npos) break;
            int cl = 0;
            try { cl = std::stoi(raw.substr(clpos + 16)); } catch (...) { break; }
            auto sep    = raw.find("\r\n\r\n");
            int bodyLen = static_cast<int>(raw.size()) - static_cast<int>(sep + 4);
            if (bodyLen >= cl) break;
        }
    }
    if (!raw.empty()) {
        HttpRequest req  = parseRequest(raw);
        std::string resp = dispatch(req);
        ::send(sock, resp.c_str(), static_cast<int>(resp.size()), 0);
    }
    closesocket(sock);
}

// ── HTTP parsing ──────────────────────────────────────────────────────────────

MobileServer::HttpRequest MobileServer::parseRequest(const std::string& raw) const {
    HttpRequest req;
    std::istringstream ss(raw);
    std::string line;
    if (std::getline(ss, line)) {
        std::istringstream ls(line);
        std::string ver;
        ls >> req.method >> req.path >> ver;
        while (!req.path.empty() &&
               (req.path.back() == '\r' || req.path.back() == '\n'))
            req.path.pop_back();
    }
    auto sep = raw.find("\r\n\r\n");
    if (sep != std::string::npos) req.body = raw.substr(sep + 4);
    return req;
}

// ── Routing ───────────────────────────────────────────────────────────────────

std::string MobileServer::dispatch(const HttpRequest& req) {
    if (req.method == "OPTIONS")
        return "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "Content-Length: 0\r\n\r\n";

    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html"))
        return httpOk(chatHtml(localIp(), port_), "text/html");

    if (req.method == "POST" && req.path == "/message") {
        std::string text = jsonGetText(req.body);
        if (text.empty()) {
            std::string msg = ResponseResolver::instance().resolve("mobile.empty_message");
            return httpOk("{\"response\":\"" + jsonEsc(msg) + "\",\"ok\":false}");
        }
        // Snapshot the handler under the lock, then run the (potentially slow)
        // cognitive turn outside it: a single slow turn must not serialize
        // /status, /skills and /concepts for every other client.
        MessageHandler handler;
        { std::lock_guard<std::mutex> l(handlerMu_); handler = msgHandler_; }
        std::string reply = handler ? handler(text)
                                    : ResponseResolver::instance().resolve("mobile.not_ready");
          
        if (reply.empty()) {
            std::string msg = ResponseResolver::instance().resolve("mobile.system_error");
            return httpOk("{\"response\":\"" + jsonEsc(msg) + "\",\"ok\":false}");
        }
          
        return httpOk("{\"response\":\"" + jsonEsc(reply) + "\",\"ok\":true}");
    }

    if (req.method == "GET" && req.path == "/status") {
        StatusHandler handler;
        { std::lock_guard<std::mutex> l(handlerMu_); handler = statusHandler_; }
        std::string s = handler ? handler() : std::string{};
        return httpOk("{\"status\":\"" + jsonEsc(s) + "\",\"ok\":true}");
    }

    if (req.method == "GET" && req.path == "/skills") {
        SkillsHandler handler;
        { std::lock_guard<std::mutex> l(handlerMu_); handler = skillsHandler_; }
        std::string s = handler ? handler() : std::string{};
        return httpOk("{\"skills\":\"" + jsonEsc(s) + "\",\"ok\":true}");
    }

    if (req.method == "GET" && req.path == "/concepts") {
        // Snapshot the handler under the lock, then invoke it outside so a slow
        // handler cannot serialize other clients (same pattern as /status).
        ConceptsHandler handler;
        { std::lock_guard<std::mutex> l(handlerMu_); handler = conceptsHandler_; }
        std::string s = handler ? handler(30) : std::string{};
        return httpOk("{\"concepts\":\"" + jsonEsc(s) + "\",\"ok\":true}");
    }

    return httpNotFound();
}

// ── HTTP response helpers ─────────────────────────────────────────────────────

std::string MobileServer::httpOk(const std::string& body, const std::string& ct) {
    std::ostringstream ss;
    ss << "HTTP/1.1 200 OK\r\n"
       << "Content-Type: " << ct << "; charset=utf-8\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n" << body;
    return ss.str();
}

std::string MobileServer::httpNotFound() {
    std::string msg = ResponseResolver::instance().resolve("mobile.not_found");
    std::string body = "{\"error\":\"" + jsonEsc(msg) + "\",\"ok\":false}";
    return "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: application/json\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n\r\n" + body;
}

std::string MobileServer::jsonEsc(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') {}
        else if (c == '\t') r += " ";
        else                r += c;
    }
    return r;
}

std::string MobileServer::jsonGetText(const std::string& json) {
    if (json.empty()) return "";
    try {
        auto parsed = nlohmann::json::parse(json);
        if (parsed.is_object() && parsed.contains("text") && parsed["text"].is_string()) {
            return parsed["text"].get<std::string>();
        }
    } catch (const nlohmann::json::exception&) {
        // Fallback for non-JSON or raw payload
    }
    return "";
}

// ── Embedded chat UI ──────────────────────────────────────────────────────────
// Uses R"HTML(...)HTML" delimiter — )"  inside JS does NOT close raw string.

std::string MobileServer::chatHtml(const std::string& ip, int port) {
    std::string url = "http://" + ip + ":" + std::to_string(port);
    auto& res = ResponseResolver::instance();

    std::string html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Yuki</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0f0f13;color:#e8e8f0;font-family:system-ui,sans-serif;
     display:flex;flex-direction:column;height:100dvh}
header{padding:14px 18px;background:#1a1a26;border-bottom:1px solid #2a2a40;
       font-size:18px;font-weight:700;display:flex;align-items:center;gap:10px}
.dot{width:10px;height:10px;border-radius:50%;background:#7c6ffa;
     animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
#chat{flex:1;overflow-y:auto;padding:16px;display:flex;flex-direction:column;gap:10px}
.msg{max-width:82%;padding:10px 14px;border-radius:14px;line-height:1.5;
     font-size:15px;white-space:pre-wrap;word-break:break-word}
.user{align-self:flex-end;background:#3a3a60;border-bottom-right-radius:4px}
.yuki{align-self:flex-start;background:#1e1e30;border-bottom-left-radius:4px;
      border-left:3px solid #7c6ffa}
.thinking{color:#7c6ffa;font-size:13px;font-style:italic}
footer{padding:12px 14px;background:#1a1a26;border-top:1px solid #2a2a40;
       display:flex;gap:8px}
#inp{flex:1;background:#0f0f18;border:1px solid #2a2a40;border-radius:10px;
     padding:10px 14px;color:#e8e8f0;font-size:15px;outline:none;resize:none;
     max-height:120px}
#inp:focus{border-color:#7c6ffa}
button{background:#7c6ffa;color:#fff;border:none;border-radius:10px;
       padding:10px 18px;cursor:pointer;font-size:15px;font-weight:600}
button:active{background:#6558e8}
</style>
</head>
<body>
<header><div class="dot"></div>Yuki</header>
<div id="chat">
  <div class="msg yuki">)HTML" + res.resolve("mobile.chat_welcome") + R"HTML(</div>
</div>
<footer>
  <textarea id="inp" rows="1" placeholder=")HTML" + res.resolve("mobile.input_placeholder") + R"HTML("></textarea>
  <button id="sendbtn">Send</button>
</footer>
<script>
var chat=document.getElementById('chat');
var inp=document.getElementById('inp');
var API=')HTML" + url + R"HTML(';
document.getElementById('sendbtn').onclick=send;
inp.addEventListener('keydown',function(e){
  if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();send();}
});
inp.addEventListener('input',function(){
  inp.style.height='auto';
  inp.style.height=Math.min(inp.scrollHeight,120)+'px';
});
function addMsg(text,cls){
  var d=document.createElement('div');
  d.className='msg '+cls;
  d.textContent=text;
  chat.appendChild(d);
  chat.scrollTop=chat.scrollHeight;
  return d;
}
function send(){
  var text=inp.value.trim();
  if(!text)return;
  inp.value='';inp.style.height='auto';
  addMsg(text,'user');
  var tk=addMsg(')HTML" + res.resolve("mobile.thinking") + R"HTML(','yuki thinking');
  var xhr=new XMLHttpRequest();
  xhr.open('POST',API+'/message',true);
  xhr.setRequestHeader('Content-Type','application/json');
  xhr.onreadystatechange=function(){
    if(xhr.readyState===4){
      tk.remove();
      try{
        var j=JSON.parse(xhr.responseText);
        addMsg(j.response||')HTML" + res.resolve("mobile.no_response") + R"HTML(','yuki');
      }catch(e){addMsg(')HTML" + res.resolve("mobile.error_prefix") + R"HTML('+xhr.status,'yuki');}
    }
  };
  xhr.send(JSON.stringify({text:text}));
}
</script>
</body>
</html>)HTML";

    return html;
}
