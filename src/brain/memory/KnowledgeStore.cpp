// KnowledgeStore.cpp — Stream parser + concept vault (merged from StreamParser + ConceptVault)
#define NOMINMAX
#include "brain/memory/KnowledgeStore.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>

// ══════════════════════════════════════════════════════════════════════════════
// StreamParser
// ══════════════════════════════════════════════════════════════════════════════

std::string StreamParser::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool StreamParser::has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

bool StreamParser::isStreamInput(const std::string& input) {
    std::istringstream ss(input); int words = 0; std::string w;
    while (ss >> w) ++words;
    if (words >= 35) return true;
    const std::string L = toLower(input);
    for (const char* c : {"also ", "by the way", "and also", "oh and",
                           "another thing", "plus ", "i also ", "additionally",
                           "at the same time"})
        if (has(L, c)) return true;
    if (has(input, "?") && words >= 15) {
        auto qpos = input.find('?');
        if (qpos > 10 && qpos < input.size() - 2) return true;
    }
    int commas = (int)std::count(input.begin(), input.end(), ',');
    if (commas >= 2 && words >= 20) return true;
    return false;
}

std::vector<std::string> StreamParser::splitIntoFragments(const std::string& input) const {
    const std::string L = toLower(input);
    const std::vector<std::string> delimiters = {
        "? ",  ". ",  "! ",
        ", also ",    ", and also ", ", by the way ",
        " also ",     " and also ",  " by the way ",
        " oh and ",   " additionally ", " plus, ",
        " but ",      " however ",  "; "
    };
    std::vector<std::pair<size_t, size_t>> pts;
    for (const auto& d : delimiters) {
        size_t p = 0;
        while ((p = L.find(d, p)) != std::string::npos) { pts.push_back({p, d.size()}); p++; }
    }
    std::sort(pts.begin(), pts.end());
    std::vector<std::pair<size_t,size_t>> deduped;
    for (auto& sp : pts)
        if (deduped.empty() || sp.first > deduped.back().first + 6) deduped.push_back(sp);
    std::vector<std::string> frags;
    size_t start = 0;
    for (auto& sp : deduped) {
        std::string f = input.substr(start, sp.first - start);
        while (!f.empty() && (std::isspace((unsigned char)f.front()) || f.front() == ',')) f.erase(f.begin());
        if (f.size() >= 4) frags.push_back(f);
        start = sp.first + sp.second;
    }
    if (start < input.size()) {
        std::string f = input.substr(start);
        while (!f.empty() && (std::isspace((unsigned char)f.front()) || f.front() == ',')) f.erase(f.begin());
        if (f.size() >= 4) frags.push_back(f);
    }
    if (frags.empty()) frags.push_back(input);
    return frags;
}

IntentType StreamParser::classifyFragment(const std::string& fragment) const {
    const std::string L = toLower(fragment);
    if (has(L,"no wait")||has(L,"actually no")||has(L,"i meant ")||has(L,"correction")||has(L,"ignore that")||has(L,"scratch that"))
        return IntentType::CORRECTION;
    if ((!fragment.empty()&&fragment.back()=='?')||has(L,"what is ")||has(L,"what are ")||has(L,"what does ")||
        has(L,"how does ")||has(L,"how do ")||has(L,"why is ")||has(L,"explain ")||has(L,"tell me about ")||
        has(L,"meaning of ")||has(L,"definition of")||has(L,"what's ")||has(L,"how to "))
        return IntentType::QUESTION;
    if (has(L,"want to learn")||has(L,"i want to learn")||has(L,"learn ")||has(L,"split ")||
        has(L,"break ")||has(L,"decompose")||has(L,"subtask")||has(L,"teach yourself"))
        return IntentType::LEARN;
    if (has(L,"build ")||has(L,"create ")||has(L,"make ")||has(L,"develop ")||has(L,"implement")||
        has(L,"write ")||has(L,"generate ")||has(L,"set up ")||has(L,"design ")||
        has(L,"launch ")||has(L,"deploy ")||has(L,"automate "))
        return IntentType::TASK;
    if (has(L,"i prefer")||has(L,"i like")||has(L,"i love")||has(L,"i hate")||
        has(L,"i don't like")||has(L,"my preference")||has(L,"i'd rather")||has(L,"i always")||has(L,"i enjoy"))
        return IntentType::PREFERENCE;
    if (has(L,"i am ")||has(L,"i'm ")||has(L,"i work")||has(L,"i have")||has(L,"my name")||
        has(L,"we are")||has(L,"the project")||has(L,"background")||has(L,"by the way i"))
        return IntentType::CONTEXT;
    return IntentType::UNCLEAR;
}

std::string StreamParser::extractSubject(const std::string& fragment, IntentType) const {
    const std::string L = toLower(fragment);
    for (const char* pfx : {"what is ", "what are ", "what does ", "how does ", "explain ",
                              "tell me about ", "meaning of ", "definition of ", "what's ", "how to "}) {
        auto p = L.find(pfx);
        if (p != std::string::npos) {
            std::string sub = fragment.substr(p + strlen(pfx));
            while (!sub.empty() && (sub.back()=='?'||sub.back()==' '||sub.back()=='.')) sub.pop_back();
            if (sub.size() >= 2) return sub;
        }
    }
    for (const char* pfx : {"build ", "create ", "make ", "develop ", "implement ",
                              "write ", "generate ", "set up ", "design ", "launch ", "deploy ", "automate "}) {
        auto p = L.find(pfx);
        if (p != std::string::npos) {
            std::string sub = fragment.substr(p + strlen(pfx));
            auto comma = sub.find(','); if (comma != std::string::npos) sub = sub.substr(0, comma);
            while (!sub.empty() && sub.back()==' ') sub.pop_back();
            if (sub.size() >= 2) return sub;
        }
    }
    for (const char* pfx : {"want to learn ", "i want to learn ", "learn "}) {
        auto p = L.find(pfx);
        if (p != std::string::npos) {
            std::string sub = fragment.substr(p + strlen(pfx));
            for (const char* trim : {" and learn", " then learn", " it", " please"}) {
                auto tp = toLower(sub).rfind(trim);
                if (tp != std::string::npos && tp > sub.size()/2) sub = sub.substr(0, tp);
            }
            if (sub.size() >= 2) return sub;
        }
    }
    return fragment.substr(0, std::min((size_t)40, fragment.size()));
}

std::string StreamParser::summarize(const std::vector<MiniIntent>& intents) const {
    if (intents.empty()) return "";
    if (intents.size() == 1) return intents[0].content.substr(0, 60);
    int tasks=0, questions=0, learns=0, prefs=0;
    for (const auto& i : intents) {
        if (i.type==IntentType::TASK)       tasks++;
        if (i.type==IntentType::QUESTION)   questions++;
        if (i.type==IntentType::LEARN)      learns++;
        if (i.type==IntentType::PREFERENCE) prefs++;
    }
    std::ostringstream ss;
    ss << (int)intents.size() << " things: ";
    if (tasks)     ss << tasks     << " task(s) ";
    if (questions) ss << questions << " question(s) ";
    if (learns)    ss << learns    << " learning goal(s) ";
    if (prefs)     ss << prefs     << " preference(s)";
    return ss.str();
}

StreamParseResult StreamParser::parse(const std::string& rawInput) const {
    StreamParseResult result;
    auto fragments = splitIntoFragments(rawInput);
    for (const auto& frag : fragments) {
        if (frag.size() < 4) continue;
        MiniIntent intent;
        intent.type       = classifyFragment(frag);
        intent.content    = frag;
        intent.confidence = (intent.type == IntentType::UNCLEAR) ? 0.30f : 0.78f;
        intent.subject    = extractSubject(frag, intent.type);
        result.intents.push_back(intent);
    }
    result.isMultiIntent = (result.intents.size() > 1);
    result.summary       = summarize(result.intents);
    if (!result.intents.empty()) {
        int unclear = 0;
        for (const auto& i : result.intents) if (i.type==IntentType::UNCLEAR) unclear++;
        result.clarity = 1.0f - (static_cast<float>(unclear)/static_cast<float>(result.intents.size()));
    }
    for (const auto& i : result.intents) {
        if (i.type!=IntentType::UNCLEAR && !i.subject.empty()) { result.dominantSubject = i.subject; break; }
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// ConceptVault
// ══════════════════════════════════════════════════════════════════════════════

static const std::string VAULT_FILE = "data/brain/concept_vault.json";

ConceptVault::ConceptVault() {}

std::string ConceptVault::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

std::string ConceptVault::cleanText(const std::string& s, int maxLen) {
    std::string r; r.reserve(s.size()); bool lastSpace = false;
    for (char c : s) {
        if (c=='\n'||c=='\r') { if (!lastSpace) { r+=' '; lastSpace=true; } }
        else if (c==' ') { if (!lastSpace) { r+=c; lastSpace=true; } }
        else { r+=c; lastSpace=false; }
    }
    while (!r.empty() && r.back()==' ') r.pop_back();
    if ((int)r.size() > maxLen) r = r.substr(0, maxLen) + "…";
    return r;
}

float ConceptVault::matchScore(const std::string& query, const std::string& term) const {
    std::string q=toLower(query), t=toLower(term);
    if (q==t)                         return 1.00f;
    if (t.find(q)!=std::string::npos) return 0.92f;
    if (q.find(t)!=std::string::npos) return 0.87f;
    std::istringstream sq(q), st(t);
    std::vector<std::string> qw, tw; std::string w;
    while (sq >> w) if (w.size()>2) qw.push_back(w);
    while (st >> w) if (w.size()>2) tw.push_back(w);
    if (qw.empty() || tw.empty()) return 0.0f;
    int hits=0;
    for (const auto& a : qw) for (const auto& b : tw) if (a==b) { hits++; break; }
    return static_cast<float>(hits)/static_cast<float>(std::max(qw.size(),tw.size()));
}

void ConceptVault::store(const LearnedConcept& c) {
    if (c.term.empty()||c.definition.empty()) return;
    std::lock_guard<std::mutex> lock(mu_);
    const std::string lterm = toLower(c.term);
    for (auto& existing : concepts_) {
        if (toLower(existing.term)==lterm) {
            if (c.confidence >= existing.confidence) {
                existing.definition = c.definition; existing.source = c.source;
                existing.confidence = c.confidence; existing.learnedAt = c.learnedAt;
                if (!c.domain.empty()) existing.domain = c.domain;
            }
            return;
        }
    }
    concepts_.push_back(c);
}

bool ConceptVault::recall(const std::string& query, LearnedConcept& out) const {
    std::lock_guard<std::mutex> lock(mu_);
    float bestScore = 0.45f; int bestIdx = -1;
    for (int i = 0; i < (int)concepts_.size(); ++i) {
        float s = matchScore(query, concepts_[i].term);
        if (s > bestScore) { bestScore = s; bestIdx = i; }
    }
    if (bestIdx < 0) return false;
    out = concepts_[bestIdx];
    const_cast<ConceptVault*>(this)->concepts_[bestIdx].timesAccessed++;
    return true;
}

std::string ConceptVault::define(const std::string& query) const {
    LearnedConcept c; if (!recall(query, c)) return "";
    std::ostringstream ss;
    ss << "**" << c.term << "**: " << c.definition;
    if (!c.domain.empty()) ss << "\n*(Related to: " << c.domain << ")*";
    return ss.str();
}

void ConceptVault::indexFromKnowledge(const std::string& topic, const std::string& text,
                                      float confidence, const std::string& domain) {
    if (topic.empty()||text.empty()) return;
    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    LearnedConcept c; c.term=topic; c.definition=cleanText(text,280);
    c.source="KnowledgeDaemon"; c.domain=domain; c.confidence=confidence; c.learnedAt=now;
    store(c); save();
}

void ConceptVault::indexAtoms(const std::string& domain,
                               const std::vector<std::string>& atomTopics,
                               const std::vector<std::string>& atomWhys) {
    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (size_t i = 0; i < atomTopics.size(); ++i) {
        LearnedConcept c; c.term=atomTopics[i]; c.domain=domain;
        c.source="TaskDecomposer"; c.learnedAt=now; c.confidence=0.70f;
        c.definition = (i<atomWhys.size())
            ? atomTopics[i]+": "+atomWhys[i]+" (from "+domain+" learning plan)"
            : atomTopics[i]+" — part of the "+domain+" learning plan.";
        store(c);
    }
    save();
}

int ConceptVault::count() const { std::lock_guard<std::mutex> lock(mu_); return (int)concepts_.size(); }

std::string ConceptVault::listTopics(int maxItems) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (concepts_.empty()) return "I haven't learned any specific concepts yet.";
    std::ostringstream ss;
    ss << "I know about " << concepts_.size() << " concepts. ";
    int n = std::min(maxItems,(int)concepts_.size()); ss << "Most recent: ";
    for (int i=(int)concepts_.size()-1; i>=(int)concepts_.size()-n && i>=0; --i)
        ss << concepts_[i].term << (i>(int)concepts_.size()-n ? ", " : ".");
    return ss.str();
}

static std::string cv_escJson(const std::string& s) {
    std::string r; r.reserve(s.size()+8);
    for (char c : s) {
        if      (c=='"')  r += "\\\"";
        else if (c=='\\') r += "\\\\";
        else if (c=='\n') r += "\\n";
        else if (c=='\r') r += "\\r";
        else              r += c;
    }
    return r;
}

void ConceptVault::save() const {
    try {
        std::filesystem::create_directories("data/brain");
        std::ofstream f(VAULT_FILE); if (!f.is_open()) return;
        std::lock_guard<std::mutex> lock(mu_);
        f << "[\n";
        for (size_t i=0; i<concepts_.size(); ++i) {
            const auto& c = concepts_[i];
            f << "  {\"term\":\""         << cv_escJson(c.term)       << "\","
              <<  "\"definition\":\""     << cv_escJson(c.definition) << "\","
              <<  "\"source\":\""         << cv_escJson(c.source)     << "\","
              <<  "\"domain\":\""         << cv_escJson(c.domain)     << "\","
              <<  "\"confidence\":"       << c.confidence             << ","
              <<  "\"learnedAt\":"        << c.learnedAt              << ","
              <<  "\"timesAccessed\":"    << c.timesAccessed          << "}";
            if (i+1<concepts_.size()) f << ","; f << "\n";
        }
        f << "]\n";
    } catch (const std::exception& e) { std::cerr << "[ConceptVault] Save error: " << e.what() << "\n"; }
}

void ConceptVault::load() {
    if (!std::filesystem::exists(VAULT_FILE)) return;
    std::ifstream f(VAULT_FILE); if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    auto strField = [&](const std::string& key, size_t start) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto p = content.find(search, start); if (p==std::string::npos) return "";
        p += search.size(); std::string val;
        while (p<content.size() && content[p]!='"') {
            if (content[p]=='\\' && p+1<content.size()) {
                char n=content[p+1];
                if (n=='"')  { val+='"';  p+=2; continue; }
                if (n=='\\') { val+='\\'; p+=2; continue; }
                if (n=='n')  { val+=' ';  p+=2; continue; }
            }
            val+=content[p++];
        }
        return val;
    };
    auto fltField = [&](const std::string& key, size_t start) -> float {
        std::string s="\""+key+"\":"; auto p=content.find(s,start);
        if (p==std::string::npos) return 0.0f; p+=s.size();
        try { return std::stof(content.substr(p,16)); } catch(...) { return 0.0f; }
    };
    auto i64Field = [&](const std::string& key, size_t start) -> int64_t {
        std::string s="\""+key+"\":"; auto p=content.find(s,start);
        if (p==std::string::npos) return 0; p+=s.size();
        try { return std::stoll(content.substr(p,20)); } catch(...) { return 0; }
    };
    std::lock_guard<std::mutex> lock(mu_); concepts_.clear();
    size_t pos=0;
    while ((pos=content.find("{\"term\":", pos)) != std::string::npos) {
        LearnedConcept c;
        c.term=strField("term",pos); c.definition=strField("definition",pos);
        c.source=strField("source",pos); c.domain=strField("domain",pos);
        c.confidence=(float)fltField("confidence",pos); c.learnedAt=i64Field("learnedAt",pos);
        c.timesAccessed=(int)i64Field("timesAccessed",pos);
        if (!c.term.empty()) concepts_.push_back(c); pos++;
    }
    if (!concepts_.empty()) std::cout << "[ConceptVault] Loaded " << concepts_.size() << " concepts\n";
}
