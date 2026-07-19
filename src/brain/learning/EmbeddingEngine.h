#pragma once
#include <vector>
#include <string>
#include <windows.h>
#include <wininet.h>

class EmbeddingEngine {
public:
    virtual ~EmbeddingEngine() = default;
    virtual bool init() = 0;
    virtual std::vector<float> embed(const std::string& text) = 0;
};

// Provides an implementation using Ollama's local REST API
class OllamaEmbeddingEngine : public EmbeddingEngine {
public:
    OllamaEmbeddingEngine(const std::string& host = "127.0.0.1", int port = 11434, const std::string& model = "nomic-embed-text");
    ~OllamaEmbeddingEngine() override;
    
    bool init() override;
    std::vector<float> embed(const std::string& text) override;
    
private:
    std::string host_;
    int port_;
    std::string model_;
    void* hSession_; // HINTERNET
    void* hConnect_; // HINTERNET
};
