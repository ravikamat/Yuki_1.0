#pragma once
// LocalLLM.h — Ollama HTTP wrapper for Yuki
// Heuristic-free. No hardcoded responses. Neural-only generation.
// Uses raw sockets (Winsock2) — lightweight, no WinHTTP dependency.

#include <string>
#include <vector>
#include <chrono>

namespace yuki {

class LocalLLM {
public:
    struct Config {
        std::string host = "127.0.0.1";  // Use IP, not hostname, for faster resolve
        int port = 11434;
        std::string model = "qwen3:1.7b";
        float default_temperature = 0.7f;
        int default_max_tokens = 512;
        int timeout_ms = 60000;  // 60s for CPU cold-start (first inference ~20-30s)
    };

    struct GenerationResult {
        std::string text;
        bool success = false;
        float latency_ms = 0.0f;
        int tokens_generated = 0;
        std::string error;
    };

    struct ChatMessage {
        std::string role;
        std::string content;
    };

    explicit LocalLLM(const Config& config = Config{});
    ~LocalLLM();

    // Check Ollama health via lightweight HTTP GET
    bool isAvailable() const;

    // Single-turn generation
    GenerationResult generate(const std::string& prompt,
                               float temperature = -1.0f,
                               int max_tokens = -1) const;

    // Multi-turn chat completion
    GenerationResult chat(const std::vector<ChatMessage>& messages,
                           float temperature = -1.0f,
                           int max_tokens = -1) const;

    // Build a Yuki-specific prompt with system context
    std::string buildPrompt(const std::string& user_input,
                            const std::string& retrieved_context = "",
                            const std::string& intent_label = "",
                            const std::string& memory_context = "") const;

    const Config& config() const { return config_; }

private:
    Config config_;
    mutable std::chrono::steady_clock::time_point last_ping_time_;
    mutable bool last_ping_result_ = false;
    mutable std::string last_ping_error_;

    // Winsock is initialized once per process via static helper
    static bool ensureWinsock();

    std::string httpRequest(const std::string& method,
                            const std::string& path,
                            const std::string& body) const;
    std::string extractJsonField(const std::string& json,
                                  const std::string& key) const;
    std::string escapeJson(const std::string& s) const;
};

} // namespace yuki
