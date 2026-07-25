// UserMemory.cpp — Persistent personal memory for Yuki
// Stores name, relationships, preferences in data/brain/user_memory.json
// No external deps — uses hand-written JSON read/write.
#include "brain/memory/UserMemory.h"
#include "brain/core/ResponseResolver.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <unordered_set>

static const std::string MEM_FILE = "data/brain/user_memory.json";

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return r;
}
static bool has(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}
static int64_t nowTs() { return static_cast<int64_t>(std::time(nullptr)); }

// ── Lifecycle ─────────────────────────────────────────────────────────────────

UserMemory::UserMemory(const std::string& dbPath) : dbPath_(dbPath) {
    try { std::filesystem::create_directories("data/brain"); } catch (...) {}
    load();
    std::cout << "[UserMemory] Loaded: " << facts_.size() << " facts, "
              << relationships_.size() << " relationships\n";
}

UserMemory::~UserMemory() {
    save();
}

// ── JSON Persistence ──────────────────────────────────────────────────────────
// Simple hand-written JSON — avoids any external library dependency.

static std::string jEsc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else                r += c;
    }
    return r;
}

static std::string jGet(const std::string& line, const std::string& key) {
    auto kpos = line.find("\"" + key + "\"");
    if (kpos == std::string::npos) return "";
    auto vstart = line.find('\"', kpos + key.size() + 2);
    if (vstart == std::string::npos) return "";
    ++vstart;
    auto vend = vstart;
    while (vend < line.size()) {
        if (line[vend] == '"' && (vend == 0 || line[vend-1] != '\\')) break;
        ++vend;
    }
    return line.substr(vstart, vend - vstart);
}

void UserMemory::save() const {
    std::ofstream f(MEM_FILE);
    if (!f.is_open()) return;

    f << "{\n  \"facts\": [\n";
    bool first = true;
    for (const auto& [k, v] : facts_) {
        if (!first) f << ",\n";
        f << "    {\"key\":\"" << jEsc(k) << "\",\"value\":\"" << jEsc(v) << "\"}";
        first = false;
    }
    f << "\n  ],\n  \"relationships\": [\n";
    first = true;
    for (const auto& r : relationships_) {
        if (!first) f << ",\n";
        f << "    {\"person\":\"" << jEsc(r.personName)
          << "\",\"relation\":\"" << jEsc(r.relation)
          << "\",\"extra\":\"" << jEsc(r.extraInfo) << "\"}";
        first = false;
    }
    f << "\n  ],\n  \"interests\": [\n";
    first = true;
    for (const auto& [t, w] : interests_) {
        if (!first) f << ",\n";
        f << "    {\"topic\":\"" << jEsc(t) << "\",\"weight\":" << w << "}";
        first = false;
    }
    // Layer 1: topic curiosity history
    f << "\n  ],\n  \"topic_history\": [\n";
    first = true;
    for (const auto& th : topicHistory_) {
        if (!first) f << ",\n";
        f << "    {\"topic\":\"" << jEsc(th.topic)
          << "\",\"count\":" << th.count
          << ",\"last_ts\":" << th.lastTs << "}";
        first = false;
    }
    // Layer 1: emotional episode memory (last 5)
    f << "\n  ],\n  \"episodes\": [\n";
    first = true;
    for (const auto& ep : episodes_) {
        if (!first) f << ",\n";
        f << "    {\"mood\":\"" << jEsc(ep.mood)
          << "\",\"snippet\":\"" << jEsc(ep.snippet)
          << "\",\"ts\":" << ep.ts << "}";
        first = false;
    }
    f << "\n  ]\n}\n";
}

void UserMemory::load() {
    std::ifstream f(MEM_FILE);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    std::istringstream ss(content);
    std::string line;
    std::string section;

    while (std::getline(ss, line)) {
        // Detect section
        if (line.find("\"facts\"")         != std::string::npos) { section = "facts"; continue; }
        if (line.find("\"relationships\"") != std::string::npos) { section = "rels";  continue; }
        if (line.find("\"interests\"")     != std::string::npos) { section = "int";   continue; }
        if (line.find("\"topic_history\"") != std::string::npos) { section = "topics"; continue; }
        if (line.find("\"episodes\"")      != std::string::npos) { section = "ep";    continue; }

        if (section == "facts" && line.find("\"key\"") != std::string::npos) {
            std::string k = jGet(line, "key");
            std::string v = jGet(line, "value");
            if (!k.empty()) facts_[k] = v;
        } else if (section == "rels" && line.find("\"person\"") != std::string::npos) {
            Relationship r;
            r.personName = jGet(line, "person");
            r.relation   = jGet(line, "relation");
            r.extraInfo  = jGet(line, "extra");
            if (!r.personName.empty()) relationships_.push_back(r);
        } else if (section == "int" && line.find("\"topic\"") != std::string::npos) {
            std::string t = jGet(line, "topic");
            float w = 1.0f;
            auto wpos = line.find("\"weight\"");
            if (wpos != std::string::npos) {
                auto colon = line.find(':', wpos);
                if (colon != std::string::npos) {
                    try { w = std::stof(line.substr(colon+1)); } catch (...) {}
                }
            }
            if (!t.empty()) interests_.push_back({t, w});
        } else if (section == "topics" && line.find("\"topic\"") != std::string::npos) {
            // Layer 1: load topic curiosity history
            TopicHistory th;
            th.topic = jGet(line, "topic");
            auto cp = line.find("\"count\"");
            if (cp != std::string::npos) {
                auto col = line.find(':', cp);
                if (col != std::string::npos) try { th.count = std::stoi(line.substr(col+1)); } catch (...) {}
            }
            auto tp = line.find("\"last_ts\"");
            if (tp != std::string::npos) {
                auto col = line.find(':', tp);
                if (col != std::string::npos) try { th.lastTs = std::stoll(line.substr(col+1)); } catch (...) {}
            }
            if (!th.topic.empty()) topicHistory_.push_back(th);
        } else if (section == "ep" && line.find("\"mood\"") != std::string::npos) {
            // Layer 1: load emotional episodes
            EmotionalEpisode ep;
            ep.mood    = jGet(line, "mood");
            ep.snippet = jGet(line, "snippet");
            auto tsp = line.find("\"ts\"");
            if (tsp != std::string::npos) {
                auto col = line.find(':', tsp);
                if (col != std::string::npos) try { ep.ts = std::stoll(line.substr(col+1)); } catch (...) {}
            }
            if (!ep.mood.empty()) episodes_.push_back(ep);
        }
    }
}


// ── Extraction entry point ────────────────────────────────────────────────────

std::vector<PersonalFact> UserMemory::extractAndStore(const std::string& utterance) {
    std::vector<PersonalFact> found;
    const std::string lower = toLower(utterance);

    // ── Identity patterns ─────────────────────────────────────────────────────
    // "my name is X" etc.
    {
        const char* patterns[] = {
            "my name is ", "i am called ", "i'm called ", "call me ",
            "people call me ", "everyone calls me ", "my full name is "
        };
        for (const char* p : patterns) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + strlen(p));
                // Take first word(s) before punctuation
                auto end = rest.find_first_of(".,!?;:");
                if (end != std::string::npos) rest = rest.substr(0, end);
                // Trim
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 1 && rest.size() < 50) {
                    PersonalFact f;
                    f.category  = "identity";
                    f.key       = "name";
                    f.value     = rest;
                    f.rawSource = utterance;
                    f.confidence = 0.99f;
                    storeFact(f);
                    found.push_back(f);
                }
                break;
            }
        }
    }

    // "I am Ravi" / "I'm Ravi" — only if word starts with capital (likely a proper name)
    // Skip if next word is an article/verb which indicates occupation/action, not a name.
    if (found.empty()) {
        const char* iamPats[] = { "i am ", "i'm " };
        for (const char* p : iamPats) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                size_t nameStart = pos + strlen(p);
                std::string restLow = lower.substr(nameStart);
                std::string firstWord;
                std::stringstream ss(restLow);
                ss >> firstWord;

                // GUARD: Skip articles and common occupation/action prefixes
                static const std::unordered_set<std::string> skipWords = {
                    "a", "an", "the", "not", "very", "quite", "really",
                    "working", "studying", "learning", "trying"
                };

                if (skipWords.find(firstWord) != skipWords.end()) {
                    break;
                }

                // Check if original text starts with a capital letter at nameStart
                if (nameStart < utterance.size() && std::isupper(static_cast<unsigned char>(utterance[nameStart]))) {
                    std::string rest = utterance.substr(nameStart);
                    auto end = rest.find_first_of(".,!?;:\n");
                    if (end != std::string::npos) rest = rest.substr(0, end);
                    while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                    if (rest.size() > 1 && rest.size() < 50) {
                        PersonalFact f;
                        f.category   = "identity";
                        f.key        = "name";
                        f.value      = rest;
                        f.rawSource  = utterance;
                        f.confidence = 0.90f;
                        storeFact(f);
                        found.push_back(f);
                    }
                }
                break;
            }
        }
    }

    // "I am X years old"
    {
        auto pos = lower.find("i am ");
        if (pos != std::string::npos) {
            std::string rest = lower.substr(pos + 5);
            if (has(rest, "years old") || has(rest, "year old")) {
                // Extract digits
                std::string digits;
                for (char c : rest) if (isdigit(c)) digits += c;
                if (!digits.empty()) {
                    PersonalFact f{"identity","age",digits,utterance,0.98f};
                    storeFact(f); found.push_back(f);
                }
            }
        }
    }

    // "I work at/for X" / "I work as X"
    {
        const char* workPats[] = {"i work at ", "i work for ", "i work as ", "i am a ", "i'm a "};
        for (const char* p : workPats) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + strlen(p));
                auto end = rest.find_first_of(".,!?;:\n");
                if (end != std::string::npos) rest = rest.substr(0, end);
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 1) {
                    std::string key = (std::string(p).find("as") != std::string::npos ||
                                       std::string(p).find("a ") != std::string::npos)
                                      ? "occupation" : "employer";
                    PersonalFact f{"identity",key,rest,utterance,0.90f};
                    storeFact(f); found.push_back(f);
                    break;
                }
            }
        }
    }

    // "I live in X" / "I am from X"
    {
        const char* locPats[] = {"i live in ", "i am from ", "i'm from ", "i'm based in "};
        for (const char* p : locPats) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + strlen(p));
                auto end = rest.find_first_of(".,!?;:\n");
                if (end != std::string::npos) rest = rest.substr(0, end);
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 1) {
                    PersonalFact f{"identity","location",rest,utterance,0.92f};
                    storeFact(f); found.push_back(f);
                    break;
                }
            }
        }
    }

    // ── Relationship patterns ─────────────────────────────────────────────────
    // "my wife is X" / "my wife's name is X" / "I have a dog named X"
    {
        const std::vector<std::pair<std::string,std::string>> relPats = {
            {"my wife is ",         "wife"},
            {"my wife's name is ",  "wife"},
            {"my husband is ",      "husband"},
            {"my husband's name is ","husband"},
            {"my mother is ",       "mother"},
            {"my father is ",       "father"},
            {"my mom is ",          "mother"},
            {"my dad is ",          "father"},
            {"my sister is ",       "sister"},
            {"my brother is ",      "brother"},
            {"my son is ",          "son"},
            {"my daughter is ",     "daughter"},
            {"my friend is ",       "friend"},
            {"my best friend is ",  "best_friend"},
            {"my girlfriend is ",   "girlfriend"},
            {"my boyfriend is ",    "boyfriend"},
            {"my boss is ",         "boss"},
            {"my dog is ",          "dog"},
            {"my dog's name is ",   "dog"},
            {"my cat is ",          "cat"},
            {"my cat's name is ",   "cat"},
            {"i have a dog named ", "dog"},
            {"i have a cat named ", "cat"},
            {"i have a son named ", "son"},
            {"i have a daughter named ","daughter"},
        };
        for (const auto& [pat, rel] : relPats) {
            auto pos = lower.find(pat);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + pat.size());
                auto end = rest.find_first_of(".,!?;:\n");
                if (end != std::string::npos) rest = rest.substr(0, end);
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 0) {
                    storeRelationship(rest, rel, "");
                    PersonalFact f{"relationship", rel, rest, utterance, 0.97f};
                    found.push_back(f);
                    break;
                }
            }
        }
    }

    // ── Preference patterns ───────────────────────────────────────────────────
    // "I like X" / "I love X" / "I enjoy X" / "I hate X"
    {
        const char* likePats[] = {"i like ", "i love ", "i enjoy ", "i'm interested in ",
                                   "i am interested in ", "i prefer "};
        const char* hatePats[] = {"i hate ", "i don't like ", "i do not like ", "i dislike "};
        for (const char* p : likePats) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + strlen(p));
                auto end = rest.find_first_of(".,!?;:\n");
                if (end != std::string::npos) rest = rest.substr(0, end);
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 1 && rest.size() < 80) {
                    storeInterest(rest, 1.0f, "stated");
                    PersonalFact f{"preference","like",rest,utterance,0.85f};
                    found.push_back(f);
                    break;
                }
            }
        }
        for (const char* p : hatePats) {
            auto pos = lower.find(p);
            if (pos != std::string::npos) {
                std::string rest = utterance.substr(pos + strlen(p));
                auto end = rest.find_first_of(".,!?;:\n");
                if (end != std::string::npos) rest = rest.substr(0, end);
                while (!rest.empty() && rest.back() == ' ') rest.pop_back();
                if (rest.size() > 1 && rest.size() < 80) {
                    storeInterest(rest, -1.0f, "stated_dislike");
                    PersonalFact f{"preference","dislike",rest,utterance,0.85f};
                    found.push_back(f);
                    break;
                }
            }
        }
    }

    return found;
}

// ── Storage ───────────────────────────────────────────────────────────────────

void UserMemory::storeFact(const PersonalFact& fact) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        facts_[fact.key] = fact.value;
    }
    save();  // persist after releasing lock
}

void UserMemory::storeRelationship(const std::string& name, const std::string& relation,
                                    const std::string& extra) {
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& r : relationships_) {
            if (toLower(r.personName) == toLower(name)) {
                r.relation = relation;
                r.extraInfo = extra;
                found = true;
                break;
            }
        }
        if (!found) {
            relationships_.push_back({name, relation, extra});
        }
    }
    save();  // persist after releasing lock — same pattern as storeInterest()
}

void UserMemory::storeInterest(const std::string& topic, float weight, const std::string&) {
    bool needs_save = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [t, w] : interests_) {
            if (t == topic) { w += weight; needs_save = true; break; }
        }
        if (!needs_save) {
            interests_.push_back({topic, weight});
            std::sort(interests_.begin(), interests_.end(),
                      [](const auto& a, const auto& b){ return a.second > b.second; });
        }
    }
    save();
}

// ── Recall ────────────────────────────────────────────────────────────────────

std::string UserMemory::getUserName() const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = facts_.find("name");
    return (it != facts_.end()) ? it->second : "";
}

std::string UserMemory::getUserFact(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = facts_.find(key);
    return (it != facts_.end()) ? it->second : "";
}

Relationship UserMemory::getRelationship(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::string nl = toLower(name);
    for (const auto& r : relationships_)
        if (toLower(r.personName) == nl) return r;
    return {};
}

std::string UserMemory::getPersonByRelation(const std::string& relation) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::string rl = toLower(relation);
    for (const auto& r : relationships_)
        if (toLower(r.relation) == rl) return r.personName;
    return "";
}

std::string UserMemory::buildGreeting() const {
    std::string name = getUserName();
    if (name.empty()) return ResponseResolver::instance().resolve("memory.greeting_anonymous");
    std::unordered_map<std::string, std::string> slots;
    slots["name"] = name;
    return ResponseResolver::instance().resolve("memory.greeting_named", slots);
}

std::string UserMemory::buildContextSummary() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (facts_.empty() && relationships_.empty()) return "";

    std::ostringstream ss;
    // Identity
    auto nameIt = facts_.find("name");
    if (nameIt != facts_.end()) ss << "User: " << nameIt->second;
    auto ageIt  = facts_.find("age");
    if (ageIt != facts_.end()) ss << ", age " << ageIt->second;
    auto locIt  = facts_.find("location");
    if (locIt != facts_.end()) ss << ", from " << locIt->second;
    auto occIt  = facts_.find("occupation");
    if (occIt != facts_.end()) ss << ", " << occIt->second;
    if (!facts_.empty()) ss << ".";

    // Relationships
    if (!relationships_.empty()) {
        ss << " Relations: ";
        for (size_t i = 0; i < relationships_.size() && i < 5; ++i) {
            if (i) ss << ", ";
            ss << relationships_[i].relation << "=" << relationships_[i].personName;
        }
        ss << ".";
    }

    // Interests
    if (!interests_.empty()) {
        ss << " Interests: ";
        int shown = 0;
        for (const auto& [t, w] : interests_) {
            if (w > 0 && shown < 4) {
                if (shown) ss << ", ";
                ss << t;
                ++shown;
            }
        }
        ss << ".";
    }
    return ss.str();
}

bool UserMemory::isPersonalStatement(const std::string& utterance) const {
    const std::string lower = toLower(utterance);
    return has(lower,"my name is")||has(lower,"i am called")||has(lower,"i'm called")||
           has(lower,"call me ")||has(lower,"i am ")||has(lower,"i'm ")||has(lower,"my wife")||has(lower,"my husband")||
           has(lower,"my mother")||has(lower,"my father")||has(lower,"my sister")||
           has(lower,"my brother")||has(lower,"my son")||has(lower,"my daughter")||
           has(lower,"my dog")||has(lower,"my cat")||has(lower,"my friend")||
           has(lower,"i work at")||has(lower,"i work for")||has(lower,"i am a ")||
           has(lower,"i'm a ")||has(lower,"i live in")||has(lower,"i am from")||
           has(lower,"i like ")||has(lower,"i love ")||has(lower,"i enjoy ")||
           has(lower,"i hate ");
}


std::vector<std::string> UserMemory::getInterests(int topN) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> result;
    for (const auto& [t, w] : interests_) {
        if (w > 0) result.push_back(t);
        if ((int)result.size() >= topN) break;
    }
    return result;
}

std::string UserMemory::acknowledge(const PersonalFact& fact) const {
    std::unordered_map<std::string, std::string> slots;
    slots["value"] = fact.value;
    slots["key"] = fact.key;

    if (fact.category == "identity" && fact.key == "name")
        return ResponseResolver::instance().resolve("memory.ack_name", slots);
    if (fact.category == "relationship")
        return ResponseResolver::instance().resolve("memory.ack_relationship", slots);
    if (fact.category == "preference" && fact.key == "like")
        return ResponseResolver::instance().resolve("memory.ack_like", slots);
    if (fact.category == "identity" && fact.key == "age")
        return ResponseResolver::instance().resolve("memory.ack_age", slots);
    return ResponseResolver::instance().resolve("memory.ack_fallback", slots);
}

// ── Layer 1: Topic Curiosity Tracking ────────────────────────────────────────

void UserMemory::recordTopic(const std::string& topic) {
    if (topic.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mu_);
        // Update existing entry or create new one
        for (auto& th : topicHistory_) {
            if (toLower(th.topic) == toLower(topic)) {
                th.count++;
                th.lastTs = nowTs();
                goto boost_interest;
            }
        }
        // New topic — keep history capped at 50 most recent
        if (topicHistory_.size() >= 50)
            topicHistory_.erase(topicHistory_.begin());
        topicHistory_.push_back({topic, 1, nowTs()});
    }
    boost_interest:
    // Each query on a topic slightly boosts its interest weight
    storeInterest(topic, 0.5f, "query");
    save();
}

// ── Layer 1: Emotional Episode Memory ────────────────────────────────────────

void UserMemory::recordEmotionalEpisode(const std::string& mood,
                                         const std::string& text) {
    if (mood.empty()) return;
    std::string snippet = text.size() > 80 ? text.substr(0, 80) + "..." : text;
    {
        std::lock_guard<std::mutex> lock(mu_);
        EmotionalEpisode ep;
        ep.mood    = mood;
        ep.snippet = snippet;
        ep.ts      = nowTs();
        episodes_.push_back(ep);
        // Keep only last 5 emotional episodes
        if (episodes_.size() > 5)
            episodes_.erase(episodes_.begin());
    }
    save();
}

// ── Layer 1: Session Greeting ─────────────────────────────────────────────────

std::string UserMemory::buildSessionGreeting() {
    std::lock_guard<std::mutex> lock(mu_);

    std::string name;
    auto nit = facts_.find("name");
    if (nit != facts_.end()) name = nit->second;

    std::unordered_map<std::string, std::string> slots;
    slots["name"] = name;

    // Mark session as started so we only greet once per session
    if (sessionStarted_) {
        // After first greeting, just use name
        return name.empty() ? ResponseResolver::instance().resolve("memory.greeting_session_brief_anonymous") 
                            : ResponseResolver::instance().resolve("memory.greeting_session_brief_named", slots);
    }
    sessionStarted_ = true;

    std::string greeting;
    if (name.empty()) {
        greeting = ResponseResolver::instance().resolve("memory.greeting_session_full_anonymous");
    } else {
        greeting = ResponseResolver::instance().resolve("memory.greeting_session_full_named", slots);
    }

    // Recall most recent emotional episode (within 48 hours)
    int64_t cutoff = nowTs() - (48 * 3600);
    std::string lastMood, lastSnippet;
    int64_t bestTs = 0;
    for (const auto& ep : episodes_) {
        if (ep.ts > cutoff && ep.ts > bestTs) {
            lastMood    = ep.mood;
            lastSnippet = ep.snippet;
            bestTs      = ep.ts;
        }
    }
    if (!lastMood.empty()) {
        if (lastMood == "sad" || lastMood == "lonely" || lastMood == "heartbroken")
            greeting += " " + ResponseResolver::instance().resolve("memory.greeting_emotional_sad");
        else if (lastMood == "stressed" || lastMood == "frustrated")
            greeting += " " + ResponseResolver::instance().resolve("memory.greeting_emotional_stressed");
        else if (lastMood == "happy" || lastMood == "proud")
            greeting += " " + ResponseResolver::instance().resolve("memory.greeting_emotional_happy");
    }

    // Recall most-queried recent topic (not already referenced above)
    if (!topicHistory_.empty()) {
        // Sort by count desc, pick most queried
        auto best = topicHistory_[0];
        for (const auto& th : topicHistory_)
            if (th.count > best.count) best = th;
        if (best.count >= 2) {
            std::unordered_map<std::string, std::string> topicSlots;
            topicSlots["topic"] = best.topic;
            greeting += " " + ResponseResolver::instance().resolve("memory.greeting_topic_recall", topicSlots);
        }
    }

    return greeting;
}

// ── Layer 4: Tone Calibration ─────────────────────────────────────────────────

void UserMemory::updateToneProfile(const std::string& msg) {
    // Casual signal keywords (single scan, no allocations)
    static const char* CASUAL[] = {
        "lol","haha","hehe","hey","bro","dude","yaar","bhai",
        "nah","yeah","ok","okay","yep","nope","tbh","idk","omg",
        "btw","imo","bruh","thx","ty","wtf","wdym",nullptr
    };
    // Formal signal keywords
    static const char* FORMAL[] = {
        "please","kindly","could you","would you","i would like",
        "i am wondering","i wanted to","regarding","with respect to",
        "furthermore","therefore","however","could you please",nullptr
    };

    std::lock_guard<std::mutex> lock(mu_);
    sessionTone_.totalMsgs++;
    sessionTone_.totalLen += (int)msg.size();

    // Check for casual signals
    std::string lw;
    lw.reserve(msg.size());
    for (char c : msg) lw += (char)std::tolower((unsigned char)c);

    for (int i = 0; CASUAL[i]; ++i)
        if (lw.find(CASUAL[i]) != std::string::npos) { sessionTone_.casualCount++; break; }

    for (int i = 0; FORMAL[i]; ++i)
        if (lw.find(FORMAL[i]) != std::string::npos) { sessionTone_.formalCount++; break; }
}

std::string UserMemory::getToneHint() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (sessionTone_.totalMsgs < 3) return "neutral";

    int avgLen = sessionTone_.totalLen / sessionTone_.totalMsgs;

    // Strong casual signals override brief (short + casual = casual)
    if (sessionTone_.casualCount >= 2 &&
        sessionTone_.casualCount > sessionTone_.formalCount) return "casual";

    // Length-based hints
    if (avgLen < 20) return "brief";
    if (avgLen > 120) return "verbose";

    // Signal-based hints
    if (sessionTone_.formalCount > sessionTone_.casualCount) return "formal";
    if (sessionTone_.casualCount > sessionTone_.formalCount) return "casual";

    return "neutral";
}
