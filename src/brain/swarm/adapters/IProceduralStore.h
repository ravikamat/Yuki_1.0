#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <ctime>
#include "brain/memory/ProceduralStore.h"
#include "CognitiveStore.h"
#include "SystemConfig.h"

namespace yuki {
namespace swarm {

struct SkillBlob {
    std::string hash;
    std::string type;
    std::vector<uint8_t> data;
    size_t size_bytes = 0;
};

struct AgentProficiency {
    std::string agent_id;
    std::string skill_hash;
    float proficiency_score = 0.0f;
    uint32_t execution_count = 0;
};

class IProceduralStore {
public:
    virtual ~IProceduralStore() = default;
    virtual bool storeSkill(const SkillBlob& skill) = 0;
    virtual std::optional<SkillBlob> loadSkill(const std::string& hash) = 0;
    virtual std::vector<SkillBlob> querySkillsByType(const std::string& type) = 0;
    virtual bool updateProficiency(const AgentProficiency& prof) = 0;
    virtual std::optional<AgentProficiency> getProficiency(
        const std::string& agent_id, const std::string& skill_hash) = 0;
    // --- Heuristic value extensions (HeuristicMemory layer) ---
    /// Load a scalar float via the proficiency table (agent_id="heuristic").
    virtual float getParameter(const std::string& key, float defaultValue) = 0;
    /// Persist a scalar float via the proficiency table.
    virtual void setParameter(const std::string& key, float value) = 0;
};

class LegacyProceduralStore : public IProceduralStore {
private:
    std::unique_ptr<memory::ProceduralStore> legacy_;
public:
    LegacyProceduralStore() {
        legacy_ = std::make_unique<memory::ProceduralStore>();
        legacy_->init("data/brain/procedural.db", "data/procedural/");
    }
    ~LegacyProceduralStore() override = default;

    bool storeSkill(const SkillBlob& skill) override {
        return legacy_->store(skill.hash, memory::ProceduralStore::BlobType::SKILL_COMPILED, skill.data);
    }

    std::optional<SkillBlob> loadSkill(const std::string& hash) override {
        auto blob = legacy_->retrieve(hash);
        if (!blob) return std::nullopt;
        SkillBlob sb;
        sb.hash = hash;
        sb.type = "skill_compiled";
        sb.data = *blob;
        sb.size_bytes = blob->size();
        return sb;
    }

    std::vector<SkillBlob> querySkillsByType(const std::string& type) override {
        std::vector<SkillBlob> results;
        auto ids = legacy_->list(memory::ProceduralStore::stringToType(type));
        for (const auto& id : ids) {
            auto b = legacy_->retrieve(id);
            if (b) {
                SkillBlob sb;
                sb.hash = id;
                sb.type = type;
                sb.data = *b;
                sb.size_bytes = b->size();
                results.push_back(sb);
            }
        }
        return results;
    }

    bool updateProficiency(const AgentProficiency& /*prof*/) override {
        return true;
    }

    std::optional<AgentProficiency> getProficiency(
        const std::string& agent_id, const std::string& skill_hash) override {
        AgentProficiency ap{agent_id, skill_hash, 1.0f, 1};
        return ap;
    }
    float getParameter(const std::string& key, float defaultValue) override {
        auto prof = getProficiency("heuristic", key);
        if (prof) return prof->proficiency_score;
        return defaultValue;
    }
    void setParameter(const std::string& key, float value) override {
        AgentProficiency ap;
        ap.agent_id = "heuristic";
        ap.skill_hash = key;
        ap.proficiency_score = value;
        ap.execution_count = 0;
        updateProficiency(ap);
    }
};

class SwarmProceduralStore : public IProceduralStore {
private:
    StorePtr store_;
public:
    SwarmProceduralStore() {
        auto& factory = CognitiveStoreFactory::instance();
        store_ = factory.getStore(StoreScope::SHARED_PROCEDURAL);
    }
    ~SwarmProceduralStore() override = default;

    bool storeSkill(const SkillBlob& skill) override {
        if (!store_ || !store_->raw()) return false;
        std::string hexData;
        hexData.reserve(skill.data.size() * 2);
        static const char hexChars[] = "0123456789ABCDEF";
        for (uint8_t b : skill.data) {
            hexData.push_back(hexChars[(b >> 4) & 0x0F]);
            hexData.push_back(hexChars[b & 0x0F]);
        }

        std::string sql = "INSERT OR REPLACE INTO procedural_blobs (hash, type, blob, created_ts, size_bytes, verification_count, contributor_ids) VALUES ('"
                          + store_->sanitize(skill.hash) + "', '"
                          + store_->sanitize(skill.type) + "', '"
                          + hexData + "', "
                          + std::to_string(std::time(nullptr)) + ", "
                          + std::to_string(skill.data.size()) + ", 1, '[\"agent-local\"]');";
        return store_->execute(sql);
    }

    std::optional<SkillBlob> loadSkill(const std::string& hash) override {
        if (!store_ || !store_->raw()) return std::nullopt;
        auto rows = store_->query("SELECT hash, type, blob, size_bytes FROM procedural_blobs WHERE hash = '" + store_->sanitize(hash) + "';");
        if (rows.empty() || rows[0].size() < 4) return std::nullopt;

        SkillBlob sb;
        sb.hash = rows[0][0];
        sb.type = rows[0][1];
        std::string hexData = rows[0][2];
        sb.size_bytes = std::stoull(rows[0][3]);

        sb.data.reserve(hexData.size() / 2);
        for (size_t i = 0; i < hexData.size(); i += 2) {
            std::string byteString = hexData.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::strtol(byteString.c_str(), nullptr, 16));
            sb.data.push_back(byte);
        }
        return sb;
    }

    std::vector<SkillBlob> querySkillsByType(const std::string& type) override {
        std::vector<SkillBlob> results;
        if (!store_ || !store_->raw()) return results;
        auto rows = store_->query("SELECT hash, type, blob, size_bytes FROM procedural_blobs WHERE type = '" + store_->sanitize(type) + "';");
        for (const auto& row : rows) {
            if (row.size() < 4) continue;
            SkillBlob sb;
            sb.hash = row[0];
            sb.type = row[1];
            std::string hexData = row[2];
            sb.size_bytes = std::stoull(row[3]);

            sb.data.reserve(hexData.size() / 2);
            for (size_t i = 0; i < hexData.size(); i += 2) {
                std::string byteString = hexData.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(std::strtol(byteString.c_str(), nullptr, 16));
                sb.data.push_back(byte);
            }
            results.push_back(sb);
        }
        return results;
    }

    bool updateProficiency(const AgentProficiency& prof) override {
        if (!store_ || !store_->raw()) return false;
        std::string sql = "INSERT OR REPLACE INTO agent_skill_proficiency (agent_id, skill_hash, proficiency, success_count, last_executed) VALUES ('"
                          + store_->sanitize(prof.agent_id) + "', '"
                          + store_->sanitize(prof.skill_hash) + "', "
                          + std::to_string(prof.proficiency_score) + ", "
                          + std::to_string(prof.execution_count) + ", "
                          + std::to_string(std::time(nullptr)) + ");";
        return store_->execute(sql);
    }

    std::optional<AgentProficiency> getProficiency(
        const std::string& agent_id, const std::string& skill_hash) override {
        if (!store_ || !store_->raw()) return std::nullopt;
        auto rows = store_->query("SELECT agent_id, skill_hash, proficiency, success_count FROM agent_skill_proficiency WHERE agent_id = '"
                                   + store_->sanitize(agent_id) + "' AND skill_hash = '" + store_->sanitize(skill_hash) + "';");
        if (rows.empty() || rows[0].size() < 4) return std::nullopt;

        AgentProficiency ap;
        ap.agent_id = rows[0][0];
        ap.skill_hash = rows[0][1];
        ap.proficiency_score = std::stof(rows[0][2]);
        ap.execution_count = std::stoul(rows[0][3]);
        return ap;
    }
    float getParameter(const std::string& key, float defaultValue) override {
        auto prof = getProficiency("heuristic", key);
        if (prof) return prof->proficiency_score;
        return defaultValue;
    }
    void setParameter(const std::string& key, float value) override {
        AgentProficiency ap;
        ap.agent_id = "heuristic";
        ap.skill_hash = key;
        ap.proficiency_score = value;
        ap.execution_count = 0;
        updateProficiency(ap);
    }
};

class ProceduralStoreFactory {
public:
    static std::unique_ptr<IProceduralStore> create() {
        if (SystemConfig::use_swarm_fabric) {
            CognitiveStoreFactory::instance().init(SystemConfig::brain_root);
            return std::make_unique<SwarmProceduralStore>();
        } else {
            return std::make_unique<LegacyProceduralStore>();
        }
    }
};

} // namespace swarm
} // namespace yuki
