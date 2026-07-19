#include "brain/learning/EmbeddingEngine.h"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <sstream>
#include <random>
#include <cmath>
#include <cctype>

OllamaEmbeddingEngine::OllamaEmbeddingEngine(const std::string& host, int port, const std::string& model)
    : host_(host), port_(port), model_(model), hSession_(nullptr), hConnect_(nullptr) {}

OllamaEmbeddingEngine::~OllamaEmbeddingEngine() {
    if (hConnect_) InternetCloseHandle(hConnect_);
    if (hSession_) InternetCloseHandle(hSession_);
}

bool OllamaEmbeddingEngine::init() {
    hSession_ = InternetOpenA("YukiEmbedding/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hSession_) {
        std::cerr << "[EmbeddingEngine] Failed to open WinINet session.\n";
        return false;
    }
    
    // Increase maximum connections per server to avoid blocking concurrent embeddings
    DWORD maxConns = 16;
    InternetSetOptionA(hSession_, INTERNET_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
    InternetSetOptionA(hSession_, INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));

    hConnect_ = InternetConnectA(hSession_, host_.c_str(),
                                 static_cast<INTERNET_PORT>(port_),
                                 nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect_) {
        std::cerr << "[EmbeddingEngine] Failed to connect to Ollama at " << host_ << ":" << port_ << "\n";
        return false;
    }
    return true;
}

std::vector<float> OllamaEmbeddingEngine::embed(const std::string& text) {
    std::vector<float> result;
    if (!hConnect_) return result;
    
    HINTERNET hRequest = HttpOpenRequestA(hConnect_, "POST", "/api/embeddings", nullptr, nullptr, nullptr, 
                                          INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) return result;
    
    std::string escapedText = text;
    // Basic JSON escape
    for (size_t i = 0; i < escapedText.length(); ++i) {
        if (escapedText[i] == '"' || escapedText[i] == '\\' || escapedText[i] == '\n' || escapedText[i] == '\r') {
            escapedText.insert(i, "\\");
            i++;
            if (escapedText[i] == '\n') escapedText[i] = 'n';
            if (escapedText[i] == '\r') escapedText[i] = 'r';
        }
    }
    
    std::string payload = "{\"model\": \"" + model_ + "\", \"prompt\": \"" + escapedText + "\"}";
    
    const char* headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers, static_cast<DWORD>(-1),
                                 const_cast<LPVOID>(static_cast<const void*>(payload.c_str())),
                                 static_cast<DWORD>(payload.size()));
    
    if (sent) {
        std::string response;
        char buf[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            response.append(buf, bytesRead);
        }
        
        // Very basic JSON parse to find the "embedding": [ ... ] array
        size_t start = response.find("\"embedding\":[");
        if (start != std::string::npos) {
            start += 13; // length of "\"embedding\":["
            size_t end = response.find("]", start);
            if (end != std::string::npos) {
                std::string arrayStr = response.substr(start, end - start);
                std::istringstream iss(arrayStr);
                std::string token;
                while (std::getline(iss, token, ',')) {
                    try {
                        result.push_back(std::stof(token));
                    } catch (...) {
                        // Ignore parse errors
                    }
                }
            }
        }
    }
    
    InternetCloseHandle(hRequest);

    // --- Local 24-dim fallback when Ollama is offline / returns empty ---
    if (result.empty()) {
        result.resize(24, 0.0f);
        if (!text.empty()) {
            size_t len = text.length();
            // Dims 0-3: char-level stats
            int upper = 0, digits = 0, puncts = 0;
            for (unsigned char c : text) {
                if (std::isupper(c)) ++upper;
                if (std::isdigit(c)) ++digits;
                if (std::ispunct(c)) ++puncts;
            }
            result[0] = std::clamp(static_cast<float>(len)   / 200.f, 0.f, 1.f);
            result[1] = std::clamp(static_cast<float>(upper)  / static_cast<float>(len), 0.f, 1.f);
            result[2] = std::clamp(static_cast<float>(digits) / static_cast<float>(len), 0.f, 1.f);
            result[3] = std::clamp(static_cast<float>(puncts) / static_cast<float>(len), 0.f, 1.f);

            // Dims 4-15: lexical markers
            std::string lo = text;
            for (auto& c : lo) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            auto has = [&](const char* w){ return lo.find(w) != std::string::npos ? 1.f : 0.f; };
            result[4]  = std::clamp(has("what")+has("how")+has("why")+has("?"), 0.f, 1.f); // question
            result[5]  = std::clamp(has("do")+has("make")+has("run")+has("build"), 0.f, 1.f); // command
            result[6]  = std::clamp(has("feel")+has("happy")+has("sad")+has("angry"), 0.f, 1.f); // emotion
            result[7]  = std::clamp(has("code")+has("bug")+has("error")+has("compile"), 0.f, 1.f); // technical
            result[8]  = std::clamp(has("urgent")+has("asap")+has("now")+has("immediately"), 0.f, 1.f);
            result[9]  = std::clamp(has("hello")+has("hi")+has("hey"), 0.f, 1.f); // greeting
            result[10] = has("?");  result[11] = has("!");
            result[12] = has("http") || has("www") ? 1.f : 0.f;
            result[13] = has("@");  result[14] = has("#");
            result[15] = has("```") || has("code") ? 1.f : 0.f;

            // Dims 16-23: seeded n-gram hash buckets (deterministic)
            std::seed_seq seed(text.begin(), text.end());
            std::mt19937 gen(seed);
            std::uniform_real_distribution<float> dist(0.f, 1.f);
            for (int i = 16; i < 24; ++i) result[i] = dist(gen);

            // L2 normalise
            float norm = 0.f;
            for (float x : result) norm += x * x;
            norm = std::sqrt(norm);
            if (norm > 1e-6f) for (auto& x : result) x /= norm;
        }
    }

    return result;
}
