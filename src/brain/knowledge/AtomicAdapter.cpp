#include "brain/knowledge/AtomicAdapter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace yuki::knowledge {

namespace {

// Split a TSV or CSV data line. Tabs win if present (ATOMIC-2020 is TSV);
// otherwise commas are used and simple double-quote quoting is honoured.
std::vector<std::string> splitRow(const std::string& line, bool tsv) {
    std::vector<std::string> out;
    if (tsv) {
        std::string cur;
        std::istringstream ss(line);
        while (std::getline(ss, cur, '\t')) out.push_back(cur);
    } else {
        std::string cur;
        bool in_quotes = false;
        for (char c : line) {
            if (c == '"') { in_quotes = !in_quotes; continue; }
            if (c == ',' && !in_quotes) { out.push_back(cur); cur.clear(); continue; }
            cur.push_back(c);
        }
        out.push_back(cur);
    }
    for (auto& f : out) {
        while (!f.empty() && (f.back() == '\r' || f.back() == '\n' || f.back() == ' ')) f.pop_back();
        size_t b = 0;
        while (b < f.size() && f[b] == ' ') ++b;
        if (b) f.erase(0, b);
    }
    return out;
}

bool looksLikeHeader(const std::vector<std::string>& cols) {
    if (cols.size() < 3) return true;
    const std::string& a = cols[0];
    const std::string& b = cols[1];
    // ATOMIC TSV header is often "head/event/relation/..." or "head, relation, tail".
    std::string ba = a, bb = b;
    std::transform(ba.begin(), ba.end(), ba.begin(), ::tolower);
    std::transform(bb.begin(), bb.end(), bb.begin(), ::tolower);
    return (ba == "head" || ba == "event" || ba == "subject") &&
           (bb == "relation" || bb == "rel");
}

float parseConfidence(const std::vector<std::string>& cols) {
    // ATOMIC rows sometimes carry a trailing numeric confidence column.
    for (size_t i = cols.size(); i-- > 3;) {
        if (cols[i].empty()) continue;
        try {
            float v = std::stof(cols[i]);
            if (v > 0.0f && v <= 1.0f) return v;
            if (v > 1.0f && v <= 100.0f) return v / 100.0f;
        } catch (...) {
        }
        break;
    }
    return 1.0f;
}

} // namespace

std::string AtomicAdapter::normalizeConcept(const std::string& raw) {
    std::string s = raw;
    // Drop ConceptNet/ATOMIC URI decorations.
    if (auto p = s.rfind("/c/en/", 0); p == 0) s = s.substr(6);
    else if (auto p2 = s.rfind("/c/", 0); p2 == 0) {
        if (auto slash = s.find('/', 3); slash != std::string::npos) s = s.substr(slash + 1);
    }
    if (auto suffix = s.find('/'); suffix != std::string::npos) s = s.substr(0, suffix);
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '_') out.push_back(' ');
        else out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    // Collapse runs of whitespace / trim.
    std::string trimmed;
    bool prev_space = false;
    for (char c : out) {
        if (c == ' ') { if (!prev_space && !trimmed.empty()) trimmed.push_back(' '); prev_space = true; }
        else { trimmed.push_back(c); prev_space = false; }
    }
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    return trimmed;
}

std::string AtomicAdapter::normalizeRelation(const std::string& rel) {
    std::string r = rel;
    if (auto p = r.rfind("/r/", 0); p == 0) r = r.substr(3);
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // Canonical ATOMIC casing for the known relations.
    static const std::unordered_map<std::string, std::string> kCanon = {
        {"xintent", "xIntent"}, {"xreact", "xReact"}, {"xreact2", "xReact"},
        {"xeffect", "xEffect"}, {"xwant", "xWant"}, {"xneed", "xNeed"},
        {"xattr", "xAttr"}, {"xreason", "xReason"}, {"ointent", "oIntent"},
        {"oreact", "oReact"}, {"oeffect", "oEffect"}, {"owant", "oWant"},
        {"hinderedby", "HinderedBy"}, {"isafter", "isAfter"},
        {"isbefore", "isBefore"}, {"hasprerequisite", "HasPrerequisite"},
        {"causes", "Causes"}, {"causesdesire", "CausesDesire"},
        {"madeupof", "MadeUpOf"}, {"hasproperty", "HasProperty"},
        {"hassubevent", "HasSubEvent"}, {"oattr", "oAttr"},
    };
    auto it = kCanon.find(r);
    if (it != kCanon.end()) return it->second;
    return r.empty() ? std::string("unknown") : r;
}

size_t AtomicAdapter::addTriple(const std::string& subject, const std::string& relation,
                                const std::string& object, float confidence,
                                const std::string& source) {
    const size_t idx = triples_.size();
    triples_.push_back({subject, relation, object, confidence, source});
    by_subject_[subject].push_back(idx);
    by_object_[object].push_back(idx);
    by_relation_[relation].push_back(idx);
    return idx;
}

AtomicAdapter::ParseStats AtomicAdapter::parseStream(const std::string& path, const Callback& cb) {
    ParseStats stats;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return stats;

    std::string line;
    bool first_data_line = true;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const bool tsv = line.find('\t') != std::string::npos;
        auto cols = splitRow(line, tsv);
        ++stats.parsed;
        if (cols.size() < 3) { ++stats.filtered; continue; }
        if (first_data_line && looksLikeHeader(cols)) { first_data_line = false; ++stats.filtered; continue; }
        first_data_line = false;

        AtomicAssertion a;
        a.subject    = normalizeConcept(cols[0]);
        a.relation   = normalizeRelation(cols[1]);
        a.object     = normalizeConcept(cols[2]);
        a.confidence = parseConfidence(cols);
        a.source     = "atomic2020";

        if (a.subject.empty() || a.object.empty() || a.subject == a.object) { ++stats.filtered; continue; }
        ++stats.accepted;
        if (cb && !cb(a)) break;
    }
    return stats;
}

bool AtomicAdapter::loadFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    triples_.reserve(triples_.size() + 1200000);
    // parseStream is lock-free; call it on the raw file with a callback that
    // appends under our lock.
    auto stats = parseStream(path, [this](const AtomicAssertion& a) {
        addTriple(a.subject, a.relation, a.object, a.confidence, a.source);
        return true;
    });
    return stats.parsed > 0;
}

std::vector<AtomicAdapter::QueryResult> AtomicAdapter::querySubject(
    const std::string& subject, const std::string& relation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueryResult> out;
    auto it = by_subject_.find(subject);
    if (it == by_subject_.end()) return out;
    for (size_t i : it->second) {
        const auto& t = triples_[i];
        if (!relation.empty() && t.relation != relation) continue;
        out.push_back({t.subject, t.relation, t.object, t.confidence});
    }
    return out;
}

std::vector<AtomicAdapter::QueryResult> AtomicAdapter::queryObject(
    const std::string& object, const std::string& relation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueryResult> out;
    auto it = by_object_.find(object);
    if (it == by_object_.end()) return out;
    for (size_t i : it->second) {
        const auto& t = triples_[i];
        if (!relation.empty() && t.relation != relation) continue;
        out.push_back({t.subject, t.relation, t.object, t.confidence});
    }
    return out;
}

std::vector<AtomicAdapter::QueryResult> AtomicAdapter::queryRelation(const std::string& relation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueryResult> out;
    auto it = by_relation_.find(relation);
    if (it == by_relation_.end()) return out;
    for (size_t i : it->second) {
        const auto& t = triples_[i];
        out.push_back({t.subject, t.relation, t.object, t.confidence});
    }
    return out;
}

std::vector<std::string> AtomicAdapter::getCauses(const std::string& event) const {
    // "What causes X?" → incoming Causes; "What does X cause?" → use getEffects.
    std::vector<std::string> out;
    for (const auto& r : queryObject(event, "Causes")) out.push_back(r.subject);
    return out;
}

std::vector<std::string> AtomicAdapter::getEffects(const std::string& event) const {
    std::vector<std::string> out;
    for (const auto& r : querySubject(event, "Causes")) out.push_back(r.object);
    for (const auto& r : querySubject(event, "xEffect")) out.push_back(r.object);
    return out;
}

std::vector<std::string> AtomicAdapter::getWants(const std::string& entity) const {
    std::vector<std::string> out;
    for (const auto& r : querySubject(entity, "xWant")) out.push_back(r.object);
    for (const auto& r : querySubject(entity, "oWant")) out.push_back(r.object);
    return out;
}

std::vector<std::string> AtomicAdapter::getNeeds(const std::string& entity) const {
    std::vector<std::string> out;
    for (const auto& r : querySubject(entity, "xNeed")) out.push_back(r.object);
    return out;
}

std::vector<std::string> AtomicAdapter::getIntents(const std::string& entity) const {
    std::vector<std::string> out;
    for (const auto& r : querySubject(entity, "xIntent")) out.push_back(r.object);
    for (const auto& r : querySubject(entity, "oIntent")) out.push_back(r.object);
    return out;
}

std::vector<std::string> AtomicAdapter::getReactions(const std::string& entity) const {
    std::vector<std::string> out;
    for (const auto& r : querySubject(entity, "xReact")) out.push_back(r.object);
    for (const auto& r : querySubject(entity, "oReact")) out.push_back(r.object);
    return out;
}

float AtomicAdapter::computePlausibility(const std::string& subject,
                                         const std::string& relation,
                                         const std::string& object) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_subject_.find(subject);
    if (it == by_subject_.end()) return 0.0f;
    for (size_t i : it->second) {
        const auto& t = triples_[i];
        if (t.object == object && (relation.empty() || t.relation == relation)) return t.confidence;
    }
    return 0.0f;
}

bool AtomicAdapter::isPlausible(const std::string& subject, const std::string& relation,
                                const std::string& object) const {
    return computePlausibility(subject, relation, object) > 0.0f;
}

AtomicAdapter::Stats AtomicAdapter::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stats s;
    s.total_triples    = triples_.size();
    s.unique_subjects  = by_subject_.size();
    s.unique_objects   = by_object_.size();
    s.unique_relations = by_relation_.size();
    return s;
}

} // namespace yuki::knowledge
