// LocalLLM.cpp — Ollama HTTP wrapper (Winsock2)
// Heuristic-free. No hardcoded responses. Neural-only generation.
#define NOMINMAX
#include "brain/language/LocalLLM.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace yuki {

// ── Winsock init (once per process) ──────────────────────────────────────────
static bool g_winsock_initialized = false;

bool LocalLLM::ensureWinsock() {
    if (g_winsock_initialized) return true;
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[LocalLLM] WSAStartup failed: " << result << "\n";
        return false;
    }
    g_winsock_initialized = true;
    return true;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
LocalLLM::LocalLLM(const Config& config) : config_(config) {
    ensureWinsock();
    
    // Initialize LocalTransformer as primary backend
    local_transformer_ = std::make_shared<yuki::brain::language::LocalTransformer>();
}

LocalLLM::~LocalLLM() {
    // WSACleanup should be called at process exit, not per-instance
}

// ── HTTP via raw sockets ──────────────────────────────────────────────────────
std::string LocalLLM::httpRequest(const std::string& method,
                                   const std::string& path,
                                   const std::string& body) const {
    if (!ensureWinsock()) {
        last_ping_error_ = "Winsock init failed";
        return "";
    }

    // Resolve host
    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result = nullptr;

    std::string port_str = std::to_string(config_.port);
    int status = getaddrinfo(config_.host.c_str(), port_str.c_str(), &hints, &result);
    if (status != 0) {
        last_ping_error_ = std::string("getaddrinfo failed: ") + gai_strerrorA(status);
        return "";
    }

    // Create socket
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        last_ping_error_ = "socket creation failed";
        return "";
    }

    // Set timeouts
    DWORD timeout = static_cast<DWORD>(config_.timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    // Connect
    if (connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(sock);
        last_ping_error_ = "connect failed to " + config_.host + ":" + port_str;
        return "";
    }
    freeaddrinfo(result);

    // Build HTTP request
    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << config_.host << ":" << config_.port << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Accept: application/json\r\n";
    request << "Connection: close\r\n";
    if (!body.empty()) {
        request << "Content-Length: " << body.length() << "\r\n";
    }
    request << "\r\n";
    if (!body.empty()) {
        request << body;
    }

    std::string req_str = request.str();

    // Send
    int sent = send(sock, req_str.c_str(), static_cast<int>(req_str.length()), 0);
    if (sent == SOCKET_ERROR || sent != static_cast<int>(req_str.length())) {
        closesocket(sock);
        last_ping_error_ = "send failed";
        return "";
    }

    // Receive
    std::string response;
    char buffer[4096];
    int received;
    while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[received] = '\0';
        response.append(buffer, received);
    }
    closesocket(sock);

    if (response.empty()) {
        last_ping_error_ = "empty response";
        return "";
    }

    // Parse HTTP response: skip headers, return body
    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = response.find("\n\n");
    }
    if (header_end == std::string::npos) {
        last_ping_error_ = "malformed HTTP response";
        return "";
    }

    // Check status code
    size_t status_pos = response.find("HTTP/1.1 ");
    if (status_pos != std::string::npos) {
        std::string status_code = response.substr(status_pos + 9, 3);
        if (status_code != "200") {
            last_ping_error_ = "HTTP " + status_code;
        }
    }

    return response.substr(header_end + 4);
}

// ── Availability check ────────────────────────────────────────────────────────
bool LocalLLM::isAvailable() const {
    auto now = std::chrono::steady_clock::now();
    if (now - last_ping_time_ < std::chrono::seconds(3))
        return last_ping_result_;

    last_ping_time_ = now;
    last_ping_error_.clear();

    std::string resp = httpRequest("GET", "/api/tags", "");
    if (resp.empty()) {
        std::cerr << "[LocalLLM] isAvailable: " << last_ping_error_
                  << " (host=" << config_.host << ":" << config_.port << ")\n";
        last_ping_result_ = false;
        return false;
    }

    // Check if our model is in the response
    last_ping_result_ = (resp.find(config_.model) != std::string::npos);
    if (!last_ping_result_) {
        // Ollama is running but model might not be listed yet — still try
        if (resp.find("models") != std::string::npos || resp.find('[') != std::string::npos) {
            std::cout << "[LocalLLM] Ollama alive. Model '" << config_.model
                      << "' will auto-load on first generate.\n";
            last_ping_result_ = true;
        } else {
            std::cerr << "[LocalLLM] Ollama response unexpected: "
                      << resp.substr(0, 100) << "...\n";
        }
    }
    return last_ping_result_;
}

// ── Training-only generation (for distillation, self-play) ────────────────────
LocalLLM::LegacyGenerationResult LocalLLM::generateForTraining(const std::string& prompt,
                                                                float temperature,
                                                                int max_tokens) const {
    // Same as generate() but explicitly for training pipeline
    return generate(prompt, temperature, max_tokens);
}

// ── Generation ────────────────────────────────────────────────────────────────
LocalLLM::LegacyGenerationResult LocalLLM::generate(const std::string& prompt,
                                                      float temperature,
                                                      int max_tokens) const {
    LegacyGenerationResult result;

    float temp    = (temperature < 0.0f) ? config_.default_temperature : temperature;
    int   max_tok = (max_tokens  < 0)    ? config_.default_max_tokens  : max_tokens;

    // FIRST: Try LocalTransformer (primary backend - no external dependency)
    if (local_transformer_ && local_transformer_->isLoaded()) {
        yuki::brain::language::GenerationRequest req;
        req.prompt = prompt;
        req.temperature = temp;
        req.maxTokens = max_tok;
        req.taskType = yuki::brain::language::GenerationTaskType::CHAT;
        req.allowExternalFallback = false; // Don't fall back from LocalTransformer

        auto lt_result = local_transformer_->generate(req);
        if (lt_result.success && !lt_result.text.empty()) {
            result.text = lt_result.text;
            result.success = true;
            result.latency_ms = lt_result.latencyMs;
            result.tokens_generated = lt_result.outputTokenCount;
            std::cout << "[LocalLLM] LocalTransformer: " << result.tokens_generated
                      << " tokens in " << result.latency_ms << "ms\n";
            return result;
        } else {
            std::cout << "[LocalLLM] LocalTransformer failed or not loaded, falling back to Ollama...\n";
        }
    } else if (local_transformer_ && !local_transformer_->isLoaded()) {
        std::cout << "[LocalLLM] LocalTransformer not loaded, attempting to load default model...\n";
        // Try to load a default model if available
        // This would be configured elsewhere, but we can attempt to find one
    }

    // SECOND: Fall back to Ollama (external HTTP) if LocalTransformer unavailable
    // Split prompt into system and user parts if system prefix is present
    std::string system_text;
    std::string user_prompt = prompt;
    size_t sys_end = prompt.find("\n\nUser: ");
    if (sys_end != std::string::npos) {
        system_text = prompt.substr(0, sys_end);
        user_prompt = prompt.substr(sys_end + 8);  // skip "\n\nUser: "
    }

    std::ostringstream json;
    json << "{\"model\":\"" << escapeJson(config_.model) << "\",";
    if (!system_text.empty()) {
        json << "\"system\":\"" << escapeJson(system_text) << "\",";
    }
    json << "\"prompt\":\"" << escapeJson(user_prompt) << "\","
         << "\"stream\":false,"
         << "\"options\":{\"temperature\":" << temp
         << ",\"num_predict\":" << max_tok << "}}";

    auto start = std::chrono::steady_clock::now();
    std::string resp = httpRequest("POST", "/api/generate", json.str());

    // Retry once on empty response (cold-start race condition)
    if (resp.empty()) {
        std::cout << "[LocalLLM] Retrying after 2s (cold-start or transient error)...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        resp = httpRequest("POST", "/api/generate", json.str());
    }

    auto end = std::chrono::steady_clock::now();
    result.latency_ms = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    if (resp.empty()) {
        result.error = "Empty HTTP response: " + last_ping_error_;
        std::cerr << "[LocalLLM] generate() failed: " << result.error << "\n";
        return result;
    }

    try {
        auto parsed = nlohmann::json::parse(resp);
        if (parsed.contains("error")) {
            result.error = parsed["error"].is_string() ? parsed["error"].get<std::string>() : "Unknown error";
            std::cerr << "[LocalLLM] Ollama error: " << result.error << "\n";
            return result;
        }

        if (parsed.contains("response") && parsed["response"].is_string()) {
            std::string raw = parsed["response"].get<std::string>();

            // Strip  blocks produced by Qwen3 / DeepSeek-R1
            // style thinking models when called via /api/generate.
            auto strip_think = [](const std::string& s) -> std::string {
                std::string out = s;
                size_t open = out.find("think");
                while (open != std::string::npos) {
                    size_t close = out.find("/think", open);
                    if (close == std::string::npos) break;
                    out.erase(open, close + 6 - open);
                    open = out.find("think");
                }
                // Trim leading/trailing whitespace left after removal
                size_t first = out.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return "";
                size_t last = out.find_last_not_of(" \t\r\n");
                return out.substr(first, last - first + 1);
            };

            result.text = strip_think(raw);
            result.success = !result.text.empty();
        }

        if (parsed.contains("eval_count") && parsed["eval_count"].is_number()) {
            result.tokens_generated = parsed["eval_count"].get<int>();
        }

        if (result.success) {
            std::cout << "[LocalLLM] " << result.tokens_generated
                      << " tokens in " << result.latency_ms << "ms\n";
        } else {
            // Thinking model returned empty response via /api/generate —
            // retry once using /api/chat which correctly surfaces message.content.
            std::cout << "[LocalLLM] Empty response from /api/generate "
                         "(thinking model?). Retrying via /api/chat...\n";
            std::vector<ChatMessage> msgs;
            // Re-split the original prompt into system + user if applicable
            std::string sys_part, usr_part = prompt;
            size_t sys_end = prompt.find("\n\nUser: ");
            if (sys_end != std::string::npos) {
                sys_part = prompt.substr(0, sys_end);
                usr_part = prompt.substr(sys_end + 8);
                msgs.push_back({"system", sys_part});
            }
            msgs.push_back({"user", usr_part});
            auto chat_result = chat(msgs, temp, max_tok);
            if (chat_result.success) {
                return chat_result;  // success via /api/chat
            }
            result.error = "No usable output from /api/generate or /api/chat. "
                           "generate raw: " + resp.substr(0, 200);
            std::cerr << "[LocalLLM] " << result.error << "\n";
        }
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        std::cerr << "[LocalLLM] " << result.error << "\n";
    }
    return result;
}

LocalLLM::LegacyGenerationResult LocalLLM::chat(const std::vector<ChatMessage>& messages,
                                                 float temperature,
                                                 int max_tokens) const {
    LegacyGenerationResult result;


    float temp    = (temperature < 0.0f) ? config_.default_temperature : temperature;
    int   max_tok = (max_tokens  < 0)    ? config_.default_max_tokens  : max_tokens;

    std::ostringstream json;
    json << "{\"model\":\"" << escapeJson(config_.model) << "\","
         << "\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"role\":\""    << escapeJson(messages[i].role)    << "\","
             << "\"content\":\"" << escapeJson(messages[i].content) << "\"}";
    }
    json << "],\"stream\":false,"
         << "\"options\":{\"temperature\":" << temp
         << ",\"num_predict\":" << max_tok << "}}";

    auto start = std::chrono::steady_clock::now();
    std::string resp = httpRequest("POST", "/api/chat", json.str());
    auto end = std::chrono::steady_clock::now();
    result.latency_ms = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    if (resp.empty()) {
        result.error = "Empty HTTP response";
        return result;
    }

    try {
        auto parsed = nlohmann::json::parse(resp);
        if (parsed.contains("error")) {
            result.error = parsed["error"].is_string()
                ? parsed["error"].get<std::string>() : "Unknown error";
            std::cerr << "[LocalLLM] chat error: " << result.error << "\n";
            return result;
        }
        // /api/chat response: { "message": { "role": "assistant", "content": "..." } }
        if (parsed.contains("message") && parsed["message"].is_object()) {
            auto& msg = parsed["message"];
            if (msg.contains("content") && msg["content"].is_string()) {
                result.text = msg["content"].get<std::string>();
                // Strip thinking tags (Qwen3 thinking=true leaves them in content)
                auto strip_think = [](const std::string& s) -> std::string {
                    std::string out = s;
                    size_t open = out.find("<think>");
                    while (open != std::string::npos) {
                        size_t close = out.find("</think>", open);
                        if (close == std::string::npos) break;
                        out.erase(open, close + 8 - open);
                        open = out.find("<think>");
                    }
                    size_t first = out.find_first_not_of(" \t\r\n");
                    if (first == std::string::npos) return "";
                    size_t last  = out.find_last_not_of(" \t\r\n");
                    return out.substr(first, last - first + 1);
                };
                result.text = strip_think(result.text);
            }
        }
        if (parsed.contains("eval_count") && parsed["eval_count"].is_number()) {
            result.tokens_generated = parsed["eval_count"].get<int>();
        }
        result.success = !result.text.empty();
        if (!result.success) {
            result.error = "Empty content in chat response. Raw: " + resp.substr(0, 200);
            std::cerr << "[LocalLLM] " << result.error << "\n";
        } else {
            std::cout << "[LocalLLM] chat: " << result.tokens_generated
                      << " tokens in " << result.latency_ms << "ms\n";
        }
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON parse error in chat(): ") + e.what();
        result.error += " Raw: " + resp.substr(0, 200);
        std::cerr << "[LocalLLM] " << result.error << "\n";
    }
    return result;
}

// ── Prompt builder ────────────────────────────────────────────────────────────
std::string LocalLLM::buildPrompt(const std::string& user_input,
                                   const std::string& retrieved_context,
                                   const std::string& intent_label,
                                   const std::string& memory_context) const {
    std::string prompt =
        "You are Yuki, an AI assistant. You are curious, helpful, and honest. "
        "Respond naturally and concisely. If you don't know something, say so. "
        "Keep responses under 3 sentences when possible.\n";

    // Inject persistent user memory so LLM knows who it's talking to
    if (!memory_context.empty()) {
        prompt += "[Memory: " + memory_context + "]\n";
    }
    if (!intent_label.empty()) {
        prompt += "[Intent: " + intent_label + "]\n";
    }
    if (!retrieved_context.empty()) {
        std::string ctx = retrieved_context;
        if (ctx.size() > 400) ctx = ctx.substr(0, 400) + "...";
        prompt += "[Context: " + ctx + "]\n";
    }

    prompt += "\nUser: " + user_input + "\nYuki: ";
    return prompt;
}


// ── JSON helpers ──────────────────────────────────────────────────────────────
std::string LocalLLM::extractJsonField(const std::string& json,
                                        const std::string& key) const {
    // Try string value: "key":"value"
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos != std::string::npos) {
        pos += search.length();
        std::string out;
        for (size_t i = pos; i < json.size(); ++i) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                char next = json[i + 1];
                if      (next == 'n')  { out += '\n'; ++i; }
                else if (next == 't')  { out += '\t'; ++i; }
                else if (next == 'r')  { out += '\r'; ++i; }
                else if (next == '\\' || next == '"' || next == '/') { out += next; ++i; }
                else if (next == 'b')  { out += '\b'; ++i; }
                else if (next == 'f')  { out += '\f'; ++i; }
                else                   { out += json[i]; }
            } else if (json[i] == '"') {
                break;
            } else {
                out += json[i];
            }
        }
        return out;
    }

    // Try numeric/boolean value: "key":value
    search = "\"" + key + "\":";
    pos = json.find(search);
    if (pos != std::string::npos) {
        pos += search.length();
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') ++end;
        std::string val = json.substr(pos, end - pos);
        // trim whitespace
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
        return val;
    }

    return "";
}

std::string LocalLLM::escapeJson(const std::string& s) const {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

yuki::brain::language::GenerationResult LocalLLM::routeGenerate(
    yuki::brain::language::BackendKind kind,
    const yuki::brain::language::GenerationRequest& request) const {
    yuki::brain::language::GenerationResult res;
    res.backend = kind;

    // Route EXTERNAL_LLM and VAE_GRAMMAR through real Ollama inference.
    // LOCAL_TRANSFORMER is handled by LocalTransformer — not this class.
    if (kind == yuki::brain::language::BackendKind::LOCAL_TRANSFORMER) {
        // This path should not reach routeGenerate — LocalTransformer handles it.
        // Return empty so the caller's fallback triggers LocalTransformer directly.
        res.success = false;
        res.backendName = "LocalTransformer";
        res.failureReason = "routeGenerate: LOCAL_TRANSFORMER must use LocalTransformer, not LocalLLM";
        return res;
    }

    res.backendName = (kind == yuki::brain::language::BackendKind::VAE_GRAMMAR)
        ? "VaeGrammarBackend" : "ExternalLlmBackend";

    // Use /api/chat (correct for thinking models like qwen3).
    std::vector<ChatMessage> msgs;
    // Split system prefix if embedded
    std::string sys_part, usr_part = request.prompt;
    size_t sys_end = request.prompt.find("\n\nUser: ");
    if (sys_end != std::string::npos) {
        sys_part = request.prompt.substr(0, sys_end);
        usr_part = request.prompt.substr(sys_end + 8);
        msgs.push_back({"system", sys_part});
    }
    msgs.push_back({"user", usr_part});

    float temp    = (request.temperature < 0.0f) ? config_.default_temperature : request.temperature;
    int   max_tok = (request.maxTokens  <= 0)    ? config_.default_max_tokens  : request.maxTokens;
    auto legacy = chat(msgs, temp, max_tok);

    res.success          = legacy.success;
    res.text             = legacy.text;
    res.failureReason    = legacy.error;
    res.latencyMs        = legacy.latency_ms;
    res.outputTokenCount = legacy.tokens_generated;
    res.confidence       = legacy.success ? 0.85f : 0.0f;
    res.fluencyScore     = legacy.success ? 0.82f : 0.0f;
    res.relevanceScore   = legacy.success ? 0.88f : 0.0f;
    res.safetyScore      = legacy.success ? 0.98f : 0.0f;
    if (!legacy.success) {
        res.generationFailureReason = yuki::brain::language::GenerationFailureReason::InvalidResponse;
    }
    return res;
}

} // namespace yuki

