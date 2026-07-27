#include "brain/language/SentenceBuilder.h"
#include "brain/core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "brain/language/GrammarEngine.h"

namespace yuki {
namespace language {


SentenceBuilder::SentenceBuilder() {
    loadSlotTemplates("data/response_slots.txt");
}

void SentenceBuilder::loadSlotTemplates(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "SentenceBuilder", "Could not open " + filepath + ", using default slots.");
        slotTemplates_["CAUSAL_CHAIN"] = "{cause} leads to {effect} because {mechanism}.";

        slotTemplates_["COUNTERFACTUAL_RESULT"] = "If {intervention} had occurred, {outcome} would follow because {reason}.";
        slotTemplates_["ANALOGY_INTRO"] = "Think of it this way: {source_domain} and {target_domain} are alike in that...";
        slotTemplates_["ANALOGY_MAPPING"] = "In this analogy, {source_item} in {source_domain} corresponds to {target_item} in {target_domain} because {shared_relation}.";
        slotTemplates_["CREATURE_BLEND"] = "Imagine the '{name}'—a being with {feature_A} from the {parent_A} and {feature_B} from the {parent_B}, adapted to live in {habitat}.";
        slotTemplates_["META_THOUGHT"] = "My GlobalWorkspace is currently focused on {focus}, with observation precision at {precision}.";
        slotTemplates_["DREAM_REPORT"] = "During my offline cycle, I synthesized a scenario blending {memory_1} and {memory_2}, resulting in {dream_description}.";
        slotTemplates_["HAIKU_LINE"] = "{line_1}\n{line_2}\n{line_3}";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pipePos = line.find('|');
        if (pipePos != std::string::npos) {
            std::string key = line.substr(0, pipePos);
            std::string tmpl = line.substr(pipePos + 1);

            // trim
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            tmpl.erase(0, tmpl.find_first_not_of(" \t"));
            tmpl.erase(tmpl.find_last_not_of(" \t") + 1);

            if (!key.empty() && !tmpl.empty()) {
                slotTemplates_[key] = tmpl;
            }
        }
    }
}

std::string SentenceBuilder::buildResponse(const std::vector<std::string>& clauses) const {
    if (clauses.empty()) return "";
    std::ostringstream oss;
    for (size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) oss << " ";
        oss << clauses[i];
    }
    return oss.str();
}

std::string SentenceBuilder::addEmotionalColoring(const std::string& base, float valence, float arousal) const {
    if (base.empty()) return base;
    std::string result = base;

    if (arousal > 0.7f && base.back() == '.') {
        result.back() = '!';
    }

    if (valence > 0.5f) {
        result += " :)";
    } else if (valence < -0.5f) {
        result += " (noted)";
    }

    return result;
}

std::string SentenceBuilder::expandSlotTemplate(
    const std::string& slotKey,
    const std::unordered_map<std::string, std::string>& bindings) const {

    auto it = slotTemplates_.find(slotKey);
    if (it == slotTemplates_.end()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "SentenceBuilder", "Unknown slot key: " + slotKey);
        return "[Unable to format response for key: " + slotKey + "]";
    }


    std::string result = it->second;
    for (const auto& [key, value] : bindings) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    return result;
}

std::string SentenceBuilder::formatCausalChain(
    const std::vector<std::tuple<std::string, std::string, std::string>>& chain) {
    if (chain.empty()) return "No causal mechanism identified.";

    std::string output;
    for (const auto& [cause, effect, mechanism] : chain) {
        std::unordered_map<std::string, std::string> bindings = {
            {"cause", cause},
            {"effect", effect},
            {"mechanism", mechanism}
        };
        output += expandSlotTemplate("CAUSAL_CHAIN", bindings) + "\n";
    }
    return output;
}

std::string SentenceBuilder::formatCounterfactual(
    const std::string& intervention, const std::string& outcome, const std::string& reason) {
    std::unordered_map<std::string, std::string> bindings = {
        {"intervention", intervention},
        {"outcome", outcome},
        {"reason", reason}
    };
    return expandSlotTemplate("COUNTERFACTUAL_RESULT", bindings);
}

std::string SentenceBuilder::formatAnalogy(
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& mappings) {
    if (mappings.empty()) return "I see a connection, but I cannot articulate it precisely.";

    std::string output = expandSlotTemplate("ANALOGY_INTRO", {{"source_domain", "Source"}, {"target_domain", "Target"}});
    output += "\n";

    for (const auto& [srcItem, srcDomain, tgtItem, tgtDomain] : mappings) {
        std::unordered_map<std::string, std::string> bindings = {
            {"source_item", srcItem},
            {"source_domain", srcDomain},
            {"target_item", tgtItem},
            {"target_domain", tgtDomain},
            {"shared_relation", "they perform structural functions"}
        };
        output += "- " + expandSlotTemplate("ANALOGY_MAPPING", bindings) + "\n";
    }
    return output;
}

std::string SentenceBuilder::formatCreativeBlend(
    const std::string& name,
    const std::vector<std::string>& features,
    const std::string& habitat) {
    std::string fA = (features.size() > 0) ? features[0] : "blended form";
    std::string fB = (features.size() > 1) ? features[1] : "adaptive traits";

    std::unordered_map<std::string, std::string> bindings = {
        {"name", name.empty() ? "hybrid organism" : name},
        {"feature_A", fA},
        {"parent_A", "first donor"},
        {"feature_B", fB},
        {"parent_B", "second donor"},
        {"habitat", habitat.empty() ? "its natural environment" : habitat}
    };
    return expandSlotTemplate("CREATURE_BLEND", bindings);
}

std::string SentenceBuilder::formatMetacognitiveState(const std::string& focus, double precision) {
    std::ostringstream precStream;
    precStream.precision(2);
    precStream << std::fixed << precision;

    std::unordered_map<std::string, std::string> bindings = {
        {"focus", focus},
        {"precision", precStream.str()}
    };
    return expandSlotTemplate("META_THOUGHT", bindings);
}

std::string SentenceBuilder::formatDream(
    const std::vector<std::string>& memories, const std::string& description) {
    std::string m1 = (memories.size() > 0) ? memories[0] : "recent compilation";
    std::string m2 = (memories.size() > 1) ? memories[1] : "research DAGs";

    std::unordered_map<std::string, std::string> bindings = {
        {"memory_1", m1},
        {"memory_2", m2},
        {"dream_description", description.empty() ? "fluid real-time neural self-optimization" : description}
    };
    return expandSlotTemplate("DREAM_REPORT", bindings);
}

std::string SentenceBuilder::formatHaiku(const std::vector<std::string>& lines) {
    if (lines.size() < 3) {
        return "Silent distant stars,\nShining brightly in the night,\nCosmic light forever.";
    }

    std::unordered_map<std::string, std::string> bindings = {
        {"line_1", lines[0]},
        {"line_2", lines[1]},
        {"line_3", lines[2]}
    };
    return expandSlotTemplate("HAIKU_LINE", bindings);
}

std::string SentenceBuilder::formatSelfDescription(const std::vector<std::string>& traits) {
    std::string t1 = (traits.size() > 0) ? traits[0] : "autonomous";
    std::string t2 = (traits.size() > 1) ? traits[1] : "analytical";
    std::string t3 = (traits.size() > 2) ? traits[2] : "learning";

    std::unordered_map<std::string, std::string> bindings = {
        {"trait_1", t1},
        {"trait_2", t2},
        {"trait_3", t3}
    };
    return expandSlotTemplate("SELF_DESC", bindings);
}

size_t SentenceBuilder::countSyllablesInLine(const std::string& line) const {
    size_t count = 0;
    std::istringstream iss(line);
    std::string word;
    while (iss >> word) {
        count += countSyllables(word);
    }
    return count;
}

size_t SentenceBuilder::countSyllables(const std::string& word) const {
    size_t count = 0;
    bool lastWasVowel = false;
    for (char c : word) {
        bool isVowel = (std::string("aeiouyAEIOUY").find(c) != std::string::npos);
        if (isVowel && !lastWasVowel) count++;
        lastWasVowel = isVowel;
    }
    if (word.length() > 2 && (word.back() == 'e' || word.back() == 'E')) count--;
    return count > 0 ? count : 1;
}
} // namespace language
} // namespace yuki


