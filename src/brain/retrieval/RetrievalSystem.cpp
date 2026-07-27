// RetrievalSystem.cpp — Web recon + hybrid retrieval router (merged from WebReconAgent + RetrievalRouter)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include "brain/retrieval/RetrievalSystem.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

// ══════════════════════════════════════════════════════════════════════════════
// WebReconAgent
// ══════════════════════════════════════════════════════════════════════════════

static std::string rs_toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
static bool rs_has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

WebReconAgent::WebReconAgent() = default;
WebReconAgent::~WebReconAgent() { shutdown(); }

bool WebReconAgent::init() {
    if (hSession_) return true;
    hSession_ = InternetOpenA("YukiWebRecon/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hSession_) { std::cout << "[WebRecon] WinINet session failed.\n"; available_ = false; return false; }
    available_ = true; return true;
}
void WebReconAgent::shutdown() {
    if (hSession_) { InternetCloseHandle(hSession_); hSession_ = nullptr; }
    available_ = false;
}

std::string WebReconAgent::urlEncode(const std::string& raw) {
    std::ostringstream out;
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') out << c;
        else if (c==' ') out << '+';
        else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); out << buf; }
    }
    return out.str();
}

std::string WebReconAgent::stripHtml(const std::string& html) {
    std::string out; out.reserve(html.size());
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; out += ' '; continue; }
        if (inTag) continue;
        if (c == '&') {
            size_t semi = html.find(';', i);
            if (semi != std::string::npos && semi - i < 10) {
                std::string ent = html.substr(i+1, semi-i-1);
                if (ent=="amp")  { out += '&'; i=semi; continue; }
                if (ent=="lt")   { out += '<'; i=semi; continue; }
                if (ent=="gt")   { out += '>'; i=semi; continue; }
                if (ent=="quot") { out += '"'; i=semi; continue; }
                if (ent=="nbsp") { out += ' '; i=semi; continue; }
                if (ent=="apos") { out += '\'';i=semi; continue; }
            }
        }
        out += c;
    }
    std::string clean; clean.reserve(out.size()); bool lastSpace = true;
    for (char c : out) {
        if (std::isspace(static_cast<unsigned char>(c))) { if (!lastSpace) { clean += ' '; lastSpace = true; } }
        else { clean += c; lastSpace = false; }
    }
    return clean;
}

float WebReconAgent::scoreSnippet(const std::string& snippet, const std::string& query) {
    if (snippet.empty() || query.empty()) return 0.0f;
    std::string sl = rs_toLower(snippet), ql = rs_toLower(query);
    std::istringstream qss(ql); std::string word; int hits = 0, total = 0;
    while (qss >> word) {
        if (word.size() < 3) continue; ++total;
        if (rs_has(sl, word)) ++hits;
    }
    if (total == 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(hits) / total);
}

std::string WebReconAgent::httpGet(const std::string& host, const std::string& path, int timeoutMs) {
    if (!hSession_) return {};
    HINTERNET hConnect = InternetConnectA(hSession_, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
                                          nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) return {};
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(), nullptr, nullptr, nullptr,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_COOKIES, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); return {}; }
    DWORD to = static_cast<DWORD>(timeoutMs);
    InternetSetOptionA(hRequest, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
    InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT,    &to, sizeof(to));
    BOOL sent = HttpSendRequestA(hRequest, nullptr, 0, nullptr, 0);
    std::string body;
    if (sent) {
        char buf[4096]; DWORD read = 0; size_t total = 0;
        while (total < 64*1024) {
            if (!InternetReadFile(hRequest, buf, sizeof(buf)-1, &read) || read==0) break;
            buf[read] = '\0'; body.append(buf, read); total += read;
        }
        available_ = true;
    } else { available_ = false; }
    InternetCloseHandle(hRequest); InternetCloseHandle(hConnect);
    return body;
}

std::vector<WebSnippet> WebReconAgent::search(const std::string& query, int maxResults, int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WebSnippet> results;
    if (!hSession_) { std::cout << "[WebRecon] Not initialised — skipping: " << query << "\n"; return results; }
    
    std::string encoded = urlEncode(query);
    // Use Wikipedia API to avoid bot blocks
    std::string path = "/w/api.php?action=query&list=search&srsearch=" + encoded + "&utf8=&format=json";
    
    std::cout << "[WebRecon] Searching Wikipedia: " << query << "\n";
    std::string json = httpGet("en.wikipedia.org", path, timeoutMs);
    
    std::cout << "[WebRecon] Wikipedia returned " << json.size() << " bytes.\n";
    
    if (json.empty()) { 
        std::cout << "[WebRecon] No response from Wikipedia.\n"; 
        available_=false; 
        return results; 
    }
    
    // Naive JSON parsing for the "snippet" field
    int count = 0;
    size_t pos = 0;
    while ((pos = json.find("\"snippet\"", pos)) != std::string::npos && count < maxResults) {
        pos += 9; // length of "\"snippet\""
        pos = json.find("\"", pos); // find the start of the string value
        if (pos == std::string::npos) break;
        pos += 1; // skip the quote
        
        size_t endPos = json.find("\"", pos);
        if (endPos == std::string::npos) break;
        
        std::string rawSnippet = json.substr(pos, endPos - pos);
        
        // Unescape JSON (handle \", \\, etc. minimally)
        std::string unescaped;
        for (size_t i = 0; i < rawSnippet.size(); ++i) {
            if (rawSnippet[i] == '\\' && i + 1 < rawSnippet.size()) {
                if (rawSnippet[i+1] == '"' || rawSnippet[i+1] == '\\') { i++; }
            }
            unescaped += rawSnippet[i];
        }
        
        std::string cleanSnippet = stripHtml(unescaped); // Wikipedia returns HTML spans for search matches
        
        float score = scoreSnippet(cleanSnippet, query);
        
        WebSnippet s; 
        s.query = query; 
        s.snippet = cleanSnippet.substr(0, 380);
        s.url = "wikipedia.org/wiki/Search"; 
        s.relevance = std::max(0.40f, score); // Base relevance is decent for Wikipedia
        
        results.push_back(s);
        ++count;
        pos = endPos;
    }
    return results;
}

std::vector<std::string> WebReconAgent::searchDuckDuckGoUrls(const std::string& query, int maxResults, int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> urls;
    if (!hSession_) return urls;
    
    std::string encoded = urlEncode(query);
    std::string path = "/html/?q=" + encoded;
    
    std::string html = httpGet("html.duckduckgo.com", path, timeoutMs);
    if (html.empty()) return urls;
    
    // Parse DDG HTML for <a class="result__url" href="...">
    size_t pos = 0;
    while (urls.size() < static_cast<size_t>(maxResults)) {
        pos = html.find("class=\"result__url\" href=\"", pos);
        if (pos == std::string::npos) break;
        pos += 26;
        size_t endPos = html.find("\"", pos);
        if (endPos == std::string::npos) break;
        
        std::string url = html.substr(pos, endPos - pos);
        
        // DDG often prefixes with //duckduckgo.com/l/?uddg=
        size_t uddg = url.find("uddg=");
        if (uddg != std::string::npos) {
            url = url.substr(uddg + 5);
            // We need to url decode the uddg parameter, but for simplicity we assume it's valid enough for fetchHtml,
            // or we just unescape basic things. Let's do a quick replace of %3A to : and %2F to /
            size_t p = 0;
            while ((p = url.find("%3A", p)) != std::string::npos) { url.replace(p, 3, ":"); p += 1; }
            p = 0;
            while ((p = url.find("%2F", p)) != std::string::npos) { url.replace(p, 3, "/"); p += 1; }
            p = 0;
            while ((p = url.find("%3D", p)) != std::string::npos) { url.replace(p, 3, "="); p += 1; }
            p = 0;
            while ((p = url.find("%3F", p)) != std::string::npos) { url.replace(p, 3, "?"); p += 1; }
            p = 0;
            while ((p = url.find("&amp;", p)) != std::string::npos) { url.replace(p, 5, ""); p += 0; } // Hack for query param drop
        }
        
        urls.push_back(url);
        pos = endPos;
    }
    
    return urls;
}

std::vector<RetrievalHit> WebReconAgent::fillSlots(const std::vector<std::string>& unresolvedSlots,
                                                    const std::string& contextHint,
                                                    int maxPerSlot, int timeoutMs) {
    std::vector<RetrievalHit> hits;
    if (!hSession_ || unresolvedSlots.empty()) return hits;
    for (const auto& slot : unresolvedSlots) {
        std::string cleanSlot = slot;
        for (const char* pfx : {"unknown: ", "missing: ", "carry_forward: "}) {
            if (cleanSlot.rfind(pfx, 0) == 0) cleanSlot = cleanSlot.substr(std::string(pfx).size());
        }
        if (cleanSlot.size() < 3) continue;
        std::string query = cleanSlot;
        if (!contextHint.empty() && contextHint.size() < 80) query = cleanSlot + " " + contextHint;
        auto snippets = search(query, maxPerSlot, timeoutMs);
        for (const auto& sn : snippets) {
            if (sn.snippet.empty()) continue;
            RetrievalHit h; h.sourceId="web:"+sn.url; h.sourceType="external";
            h.content=sn.snippet; h.relevance=sn.relevance; h.trust=sn.relevance*0.60f;
            h.timestampMs=static_cast<uint64_t>(GetTickCount64());
            hits.push_back(h);
        }
        if (!available_) break;
    }
    return hits;
}

std::vector<RetrievalHit> WebReconAgent::searchConfidenceDriven(const std::string& query,
                                                                  float minConfidence,
                                                                  int maxSearches,
                                                                  int timeoutMs) {
    std::vector<RetrievalHit> hits;
    if (!hSession_ || query.empty()) return hits;

    float currentConfidence = 0.0f;
    int searchCount = 0;
    std::vector<std::string> queryVariants = {
        query,
        query + " overview definition",
        query + " technical details explanation",
        query + " summary documentation",
        query + " examples tutorial"
    };

    while (currentConfidence < minConfidence && searchCount < maxSearches) {
        std::string currentQuery = queryVariants[searchCount % queryVariants.size()];
        searchCount++;

        auto snippets = search(currentQuery, 3, timeoutMs);
        for (const auto& sn : snippets) {
            if (sn.snippet.empty()) continue;
            RetrievalHit h;
            h.sourceId = "web:" + sn.url;
            h.sourceType = "external";
            h.content = sn.snippet;
            h.relevance = sn.relevance;
            h.trust = sn.relevance * 0.75f;
            h.timestampMs = static_cast<uint64_t>(GetTickCount64());
            hits.push_back(h);

            if (h.trust > currentConfidence) {
                currentConfidence = h.trust;
            }
        }
        if (currentConfidence >= minConfidence) break;
    }
    return hits;
}

std::future<std::vector<WebSnippet>> WebReconAgent::searchAsync(const std::string& query, int maxResults) {
    return std::async(std::launch::async, [this, query, maxResults]() {
        return this->search(query, maxResults);
    });
}

// ══════════════════════════════════════════════════════════════════════════════
// RetrievalRouter
// ══════════════════════════════════════════════════════════════════════════════

bool RetrievalRouter::isStopWord(const std::string& w) {
    static const char* stops[] = {
        "the","and","is","in","of","to","a","an","it","its","that","this","was","for",
        "on","are","with","as","at","be","from","or","but","not","have","had","has",
        "what","how","why","when","where","who","which","were","by",nullptr
    };
    for (int i = 0; stops[i]; ++i) if (w == stops[i]) return true;
    return false;
}
float RetrievalRouter::keywordOverlap(const std::string& a, const std::string& b) {
    auto la=rs_toLower(a), lb=rs_toLower(b);
    std::istringstream sa(la), sb(lb); std::string w;
    std::vector<std::string> wordsA;
    while (sa >> w) if (w.size()>3 && !isStopWord(w)) wordsA.push_back(w);
    if (wordsA.empty()) return 0.0f;
    int hits=0;
    while (sb >> w) {
        if (w.size()<=3 || isStopWord(w)) continue;
        if (std::find(wordsA.begin(), wordsA.end(), w)!=wordsA.end()) ++hits;
    }
    return std::min(1.0f, static_cast<float>(hits)/static_cast<float>(wordsA.size()));
}

std::vector<RetrievalHit> RetrievalRouter::searchInternal(const PatternFrame& frame,
                                                           const std::vector<std::string>&,
                                                           int timeoutMs) const {
    std::vector<RetrievalHit> hits;
    if (!knowledge_ || !knowledge_->isRunning()) return hits;
    auto ans = knowledge_->query(frame.rawInput, timeoutMs);
    if (ans.found && ans.confidence >= 0.35f) {
        RetrievalHit h; h.sourceId="knowledge:"+ans.topic; h.sourceType="internal_semantic";
        h.content=ans.text; h.relevance=keywordOverlap(ans.text,frame.rawInput); h.trust=ans.confidence;
        hits.push_back(h);
    }
    if (hits.empty() || hits[0].trust < 0.55f) {
        for (const auto& entity : frame.entities) {
            auto ea = knowledge_->query(entity, timeoutMs);
            if (ea.found && ea.confidence >= 0.35f) {
                RetrievalHit h; h.sourceId="knowledge:"+ea.topic; h.sourceType="internal_semantic";
                h.content=ea.text; h.relevance=keywordOverlap(ea.text,frame.rawInput); h.trust=ea.confidence;
                hits.push_back(h); if (ea.confidence >= 0.65f) break;
            }
        }
    }
    return hits;
}

std::vector<RetrievalHit> RetrievalRouter::searchCode(const PatternFrame& frame) const {
    std::vector<RetrievalHit> hits;
    if (frame.entities.empty()) return hits;
    std::vector<std::string> terms;
    for (const auto& e : frame.entities) if (e.size()>2) terms.push_back(rs_toLower(e));
    if (terms.empty()) return hits;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator("src")) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext!=".cpp" && ext!=".h") continue;
            std::ifstream f(entry.path()); if (!f.is_open()) continue;
            std::string line; int lineNo=0; std::ostringstream acc; bool anyHit=false;
            while (std::getline(f, line) && hits.size()<10) {
                ++lineNo; std::string llow=rs_toLower(line);
                for (const auto& term : terms) {
                    if (llow.find(term)!=std::string::npos) {
                        bool isDecl = llow.find("void ")!=std::string::npos ||
                                      llow.find("bool ")!=std::string::npos ||
                                      llow.find("class ")!=std::string::npos ||
                                      llow.find("struct ")!=std::string::npos ||
                                      llow.find("std::")!=std::string::npos;
                        if (isDecl) { acc << entry.path().filename().string() << ":" << lineNo << "  " << line.substr(0,100) << "\n"; anyHit=true; }
                        break;
                    }
                }
            }
            if (anyHit) {
                RetrievalHit h; h.sourceId="code:"+entry.path().filename().string();
                h.sourceType="code_index"; h.content=acc.str().substr(0,600); h.relevance=0.60f; h.trust=0.60f;
                hits.push_back(h);
            }
            if (hits.size()>=5) break;
        }
    } catch (...) {}
    return hits;
}

std::vector<RetrievalHit> RetrievalRouter::searchGraph(const PatternFrame& frame) const {
    std::vector<RetrievalHit> hits;
    if (frame.entities.empty()) return hits;
    std::ifstream f(kGraphPath); if (!f.is_open()) return hits;
    std::vector<std::string> lowerEntities;
    for (const auto& e : frame.entities) lowerEntities.push_back(rs_toLower(e));
    std::string line; int checked=0;
    while (std::getline(f,line) && hits.size()<6 && checked<5000) {
        ++checked; std::string ll=rs_toLower(line); bool matched=false;
        for (const auto& e : lowerEntities) { if (e.size()>2 && ll.find(e)!=std::string::npos) { matched=true; break; } }
        if (!matched) continue;
        auto extract = [&](const std::string& key) -> std::string {
            auto p=line.find("\""+key+"\":\""); if (p==std::string::npos) return "";
            p+=key.size()+4; auto end=line.find("\"",p);
            return (end!=std::string::npos) ? line.substr(p,end-p) : "";
        };
        std::string subj=extract("subject"), rel=extract("relation"), obj=extract("object");
        if (subj.empty()&&rel.empty()&&obj.empty()) continue;
        std::string claim=subj+" "+rel+" "+obj; float rel_score=keywordOverlap(claim,frame.rawInput);
        RetrievalHit h; h.sourceId="graph:entity"; h.sourceType="knowledge_graph";
        h.content=claim; h.relevance=rel_score; h.trust=0.70f; hits.push_back(h);
    }
    std::sort(hits.begin(), hits.end(), [](const RetrievalHit& a, const RetrievalHit& b){ return a.relevance>b.relevance; });
    return hits;
}

std::vector<RetrievalHit> RetrievalRouter::searchTraces(const PatternFrame& frame, int maxResults) const {
    std::vector<RetrievalHit> hits;
    if (!traceStore_) return hits;
    std::ifstream f("data/traces/yuki_traces.jsonl"); if (!f.is_open()) return hits;
    std::vector<std::string> lines; std::string line;
    while (std::getline(f,line)) if (!line.empty()) lines.push_back(line);
    int found=0;
    for (int i=(int)lines.size()-1; i>=0&&found<maxResults; --i) {
        const auto& ln=lines[i];
        auto p=ln.find("\"finalText\":\""); if (p==std::string::npos) continue; p+=13;
        auto end=ln.find("\"",p); if (end==std::string::npos) continue;
        std::string text=ln.substr(p,end-p); if (text.size()<20) continue;
        float score=keywordOverlap(text,frame.rawInput); if (score<0.20f) continue;
        if (ln.find("\"success\":true")==std::string::npos) continue;
        RetrievalHit h; h.sourceId="trace:"+std::to_string(i); h.sourceType="trace";
        h.content=text.substr(0,400); h.relevance=score; h.trust=0.65f;
        hits.push_back(h); ++found;
    }
    return hits;
}

std::vector<RetrievalHit> RetrievalRouter::searchWeb(const PatternFrame& frame,
                                                      const std::vector<std::string>& unresolvedSlots,
                                                      int timeoutMs) const {
    if (!webRecon_) return {};
    std::string hint = frame.coreIntent;
    if (hint.empty()) hint = frame.rawInput;
    if (!unresolvedSlots.empty()) {
        return webRecon_->fillSlots(unresolvedSlots, hint, 50, timeoutMs);
    }
    return webRecon_->searchConfidenceDriven(hint, 0.80f, 50, timeoutMs);
}

std::vector<RetrievalHit> RetrievalRouter::searchVectorIndex(const PatternFrame& frame) const {
    std::vector<RetrievalHit> hits;
    if (!vectorStore_ || !embeddingEngine_) return hits;
    
    std::vector<float> queryEmb = embeddingEngine_->embed(frame.rawInput);
    if (queryEmb.empty()) return hits;
    
    auto vsHits = vectorStore_->search(queryEmb, 3);
    for (const auto& v : vsHits) {
        // threshold: only return if cosine distance is low enough (similarity is high)
        if (v.distance < 0.45f) {
            RetrievalHit h;
            h.sourceId = "vector:" + std::to_string(v.id);
            h.sourceType = "vector_index";
            h.content = v.metadata;
            h.relevance = 1.0f - v.distance;
            h.trust = h.relevance * 0.9f;
            hits.push_back(h);
        }
    }
    return hits;
}

std::vector<RetrievalHit> RetrievalRouter::runHybrid(const PatternFrame& frame,
                                                      const std::vector<std::string>& unresolvedSlots,
                                                      float coverageThreshold) const {
    std::vector<RetrievalHit> all; float bestTrust=0.0f;
    auto addAll = [&](std::vector<RetrievalHit>&& hits) {
        for (auto& h : hits) { bestTrust=std::max(bestTrust,h.trust); all.push_back(std::move(h)); }
    };
    
    addAll(searchVectorIndex(frame));              if (bestTrust>=coverageThreshold) return all;
    addAll(searchInternal(frame, frame.entities)); if (bestTrust>=coverageThreshold) return all;
    addAll(searchCode(frame));                     if (bestTrust>=coverageThreshold) return all;
    addAll(searchGraph(frame));                    if (bestTrust>=coverageThreshold) return all;
    addAll(searchTraces(frame));                   if (bestTrust>=coverageThreshold) return all;
    if (!unresolvedSlots.empty()) addAll(searchWeb(frame, unresolvedSlots));
    return all;
}
