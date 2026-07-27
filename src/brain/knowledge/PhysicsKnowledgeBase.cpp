#include "brain/knowledge/PhysicsKnowledgeBase.h"
#include "brain/causality/CausalGraph.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace yuki::knowledge {

std::string PhysicsKnowledgeBase::extractJsonString(const std::string& json, const std::string& key) const {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos += pattern.size();
    while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == '"')) pos++;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

float PhysicsKnowledgeBase::extractJsonFloat(const std::string& json, const std::string& key, float default_val) const {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return default_val;

    pos += pattern.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    try {
        return std::stof(json.substr(pos));
    } catch (...) {
        return default_val;
    }
}

bool PhysicsKnowledgeBase::extractJsonBool(const std::string& json, const std::string& key) const {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return false;
    pos += pattern.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    return (json.compare(pos, 4, "true") == 0);
}

std::vector<std::string> PhysicsKnowledgeBase::extractJsonArray(const std::string& json, const std::string& key) const {
    std::vector<std::string> result;
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return result;

    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;

    size_t end = json.find(']', pos);
    if (end == std::string::npos) return result;

    std::string content = json.substr(pos + 1, end - pos - 1);
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t q1 = item.find('"');
        size_t q2 = item.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
            result.push_back(item.substr(q1 + 1, q2 - q1 - 1));
        }
    }
    return result;
}

bool PhysicsKnowledgeBase::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    size_t count = 0;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::string type = extractJsonString(line, "type");
        if (type == "material") {
            MaterialProperties m;
            m.name = extractJsonString(line, "name");
            m.density = extractJsonFloat(line, "density", 1000.0f);
            m.restitution = extractJsonFloat(line, "restitution", 0.5f);
            m.static_friction = extractJsonFloat(line, "static_friction", 0.3f);
            m.dynamic_friction = extractJsonFloat(line, "dynamic_friction", 0.2f);
            m.melting_point = extractJsonFloat(line, "melting_point", 0.0f);
            m.boiling_point = extractJsonFloat(line, "boiling_point", 100.0f);
            m.is_fluid = extractJsonBool(line, "is_fluid");
            m.viscosity = extractJsonFloat(line, "viscosity", 0.0f);
            if (!m.name.empty()) {
                materials_[m.name] = m;
                count++;
            }
        } else if (type == "law") {
            PhysicalLaw law;
            law.name = extractJsonString(line, "name");
            law.formula = extractJsonString(line, "formula");
            law.variables = extractJsonArray(line, "variables");
            law.applicable_domains = extractJsonArray(line, "applicable_domains");
            law.confidence = extractJsonFloat(line, "confidence", 1.0f);

            if (!law.name.empty()) {
                size_t idx = laws_.size();
                laws_.push_back(law);
                for (const auto& d : law.applicable_domains) {
                    domain_index_[d].push_back(idx);
                }
                count++;
            }
        } else if (type == "triplet") {
            PhysicsTriplet t;
            t.subject = extractJsonString(line, "subject");
            t.relation = extractJsonString(line, "relation");
            t.object = extractJsonString(line, "object");
            t.weight = extractJsonFloat(line, "weight", 1.0f);

            if (!t.subject.empty() && !t.object.empty()) {
                triplets_.push_back(t);
                count++;
            }
        }
    }

    return count > 0;
}

const MaterialProperties* PhysicsKnowledgeBase::getMaterial(const std::string& name) const {
    auto it = materials_.find(name);
    if (it != materials_.end()) return &it->second;
    return nullptr;
}

std::vector<PhysicalLaw> PhysicsKnowledgeBase::getLawsByDomain(const std::string& domain) const {
    std::vector<PhysicalLaw> result;
    auto it = domain_index_.find(domain);
    if (it != domain_index_.end()) {
        for (size_t idx : it->second) {
            result.push_back(laws_[idx]);
        }
    }
    return result;
}

void PhysicsKnowledgeBase::exportToPhysicsWorld(const std::string& config_path) const {
    std::ofstream out(config_path);
    if (!out.is_open()) return;

    for (const auto& [name, m] : materials_) {
        out << m.name << "|" << m.density << "|" << m.restitution << "|"
            << m.static_friction << "|" << m.dynamic_friction << "|"
            << m.melting_point << "|" << m.boiling_point << "|"
            << (m.is_fluid ? 1 : 0) << "|" << m.viscosity << "\n";
    }
}

void PhysicsKnowledgeBase::syncToCausalGraph(yuki::causality::CausalGraph* graph) const {
    if (!graph) return;

    std::unordered_map<std::string, uint32_t> node_map;
    for (size_t i = 0; i < graph->nodes.size(); ++i) {
        node_map[graph->nodes[i].name] = graph->nodes[i].id;
    }

    auto getOrAdd = [&](const std::string& name) -> uint32_t {
        if (node_map.count(name)) return node_map[name];
        graph->addNode(name);
        uint32_t id = static_cast<uint32_t>(graph->nodes.size() - 1);
        node_map[name] = id;
        return id;
    };

    for (const auto& t : triplets_) {
        uint32_t s_id = getOrAdd(t.subject);
        uint32_t o_id = getOrAdd(t.object);
        graph->addEdge(s_id, o_id);
    }
}

} // namespace yuki::knowledge
