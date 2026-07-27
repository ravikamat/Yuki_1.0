#pragma once
// RetrievalSystem.h — Web recon + hybrid retrieval router (merged from WebReconAgent + RetrievalRouter)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include "BrainTypes.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "brain/memory/AuditSystem.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>
#include "brain/retrieval/VectorStore.h"
#include "brain/learning/EmbeddingEngine.h"

// ── §WebReconAgent ────────────────────────────────────────────────────────────

struct WebSnippet {
    std::string query;
    std::string snippet;
    std::string url;
    float       relevance = 0.0f;
};

class WebReconAgent {
public:
    WebReconAgent();
    ~WebReconAgent();
    bool init();
    void shutdown();
    bool isAvailable() const { return available_.load(); }
    std::vector<RetrievalHit> fillSlots(const std::vector<std::string>& unresolvedSlots,
                                        const std::string& contextHint,
                                        int maxPerSlot = 1, int timeoutMs = 3000);
    std::vector<WebSnippet> search(const std::string& query,
                                   int maxResults = 2, int timeoutMs = 3000);
    std::future<std::vector<WebSnippet>> searchAsync(const std::string& query, int maxResults = 5);
    
    std::vector<RetrievalHit> searchConfidenceDriven(const std::string& query,
                                                      float minConfidence = 0.80f,
                                                      int maxSearches = 50,
                                                      int timeoutMs = 3000);
    
    std::vector<std::string> searchDuckDuckGoUrls(const std::string& query,
                                                  int maxResults = 3, int timeoutMs = 3000);
private:
    std::string  httpGet(const std::string& host, const std::string& path, int timeoutMs);
    static std::string stripHtml(const std::string& html);
    static float       scoreSnippet(const std::string& snippet, const std::string& query);
    static std::string urlEncode(const std::string& raw);
    HINTERNET         hSession_  = nullptr;
    std::atomic<bool> available_{false};
    std::mutex        mutex_;
};

// ── §RetrievalRouter ──────────────────────────────────────────────────────────

class RetrievalRouter {
public:
    RetrievalRouter() = default;
    void setKnowledge(KnowledgeDaemon* kd) { knowledge_  = kd; }
    void setWebRecon(WebReconAgent*   wr)  { webRecon_   = wr; }
    void setTraceStore(TraceStore*    ts)  { traceStore_ = ts; }
    void setVectorStore(VectorStore* vs)   { vectorStore_ = vs; }
    void setEmbeddingEngine(EmbeddingEngine* ee) { embeddingEngine_ = ee; }

    std::vector<RetrievalHit> searchInternal(const PatternFrame& frame,
                                              const std::vector<std::string>& zones,
                                              int timeoutMs = 400) const;
    std::vector<RetrievalHit> searchCode(const PatternFrame& frame) const;
    std::vector<RetrievalHit> searchGraph(const PatternFrame& frame) const;
    std::vector<RetrievalHit> searchTraces(const PatternFrame& frame, int maxResults = 3) const;
    std::vector<RetrievalHit> searchWeb(const PatternFrame& frame,
                                        const std::vector<std::string>& unresolvedSlots,
                                        int timeoutMs = 3000) const;
    std::vector<RetrievalHit> runHybrid(const PatternFrame& frame,
                                        const std::vector<std::string>& unresolvedSlots,
                                        float coverageThreshold = 0.65f) const;
    std::vector<RetrievalHit> searchVectorIndex(const PatternFrame& frame) const;

private:
    static float keywordOverlap(const std::string& a, const std::string& b);
    static bool  isStopWord(const std::string& w);

    KnowledgeDaemon* knowledge_  = nullptr;
    WebReconAgent*   webRecon_   = nullptr;
    TraceStore*      traceStore_ = nullptr;
    VectorStore*     vectorStore_ = nullptr;
    EmbeddingEngine* embeddingEngine_ = nullptr;

    static constexpr const char* kGraphPath = "data/knowledge/graph.json";
};
