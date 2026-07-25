#include "brain/language/MetaphorEngine.h"
#include "brain/reasoning/AnalogicalReasoning.h"
#include "brain/emotion/ValenceArousalModel.h"
#include "brain/core/Logger.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace yuki { namespace language {

struct TemplateItem {
    std::string type; // METAPHOR or SIMILE
    std::string text;
};

class MetaphorEngine::Impl {
public:
    yuki::reasoning::AnalogicalReasoning* analogy_ = nullptr;
    yuki::emotion::ValenceArousalModel* emotion_ = nullptr;
    std::vector<TemplateItem> templates_;

    Impl() = default;

    std::string replaceSlot(const std::string& tmpl, const std::string& slot, const std::string& val) const {
        std::string res = tmpl;
        size_t pos = 0;
        std::string token = "{" + slot + "}";
        while ((pos = res.find(token, pos)) != std::string::npos) {
            res.replace(pos, token.length(), val);
            pos += val.length();
        }
        return res;
    }
};

MetaphorEngine::MetaphorEngine() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "MetaphorEngine initialized");
    loadTemplates("data/metaphor_templates.txt");
}

MetaphorEngine::~MetaphorEngine() = default;

MetaphorEngine::MetaphorEngine(MetaphorEngine&&) noexcept = default;
MetaphorEngine& MetaphorEngine::operator=(MetaphorEngine&&) noexcept = default;

void MetaphorEngine::setAnalogicalReasoning(yuki::reasoning::AnalogicalReasoning* analogy) {
    pImpl->analogy_ = analogy;
}

void MetaphorEngine::setValenceArousalModel(yuki::emotion::ValenceArousalModel* emotion) {
    pImpl->emotion_ = emotion;
}

bool MetaphorEngine::loadTemplates(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    pImpl->templates_.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t sep = line.find('|');
        if (sep != std::string::npos) {
            TemplateItem item;
            item.type = line.substr(0, sep);
            item.text = line.substr(sep + 1);
            pImpl->templates_.push_back(item);
        }
    }
    return !pImpl->templates_.empty();
}

void MetaphorEngine::clearTemplates() {
    pImpl->templates_.clear();
}

MetaphorResult MetaphorEngine::generateMetaphor(const std::string& targetConcept,
                                                 const std::string& sourceDomain) {
    MetaphorResult res;
    res.targetDomain = targetConcept;
    res.sourceDomain = sourceDomain;
    res.isSimile = false;

    std::string tmpl = "{target} = {source}";
    for (const auto& t : pImpl->templates_) {
        if (t.type == "METAPHOR") {
            tmpl = t.text;
            break;
        }
    }

    std::string expr = pImpl->replaceSlot(tmpl, "target", targetConcept);
    expr = pImpl->replaceSlot(expr, "source", sourceDomain);
    expr = pImpl->replaceSlot(expr, "reason", "shared structural pattern");

    res.expression = expr;
    res.aptness = 0.85;

    if (pImpl->emotion_) {
        auto st = pImpl->emotion_->getState();
        if (st.arousal > 0.7) res.aptness += 0.1;
    }

    return res;
}

MetaphorResult MetaphorEngine::generateSimile(const std::string& targetConcept,
                                              const std::string& sourceDomain) {
    MetaphorResult res;
    res.targetDomain = targetConcept;
    res.sourceDomain = sourceDomain;
    res.isSimile = true;

    std::string tmpl = "{target} ~ {source}";
    for (const auto& t : pImpl->templates_) {
        if (t.type == "SIMILE") {
            tmpl = t.text;
            break;
        }
    }

    std::string expr = pImpl->replaceSlot(tmpl, "target", targetConcept);
    expr = pImpl->replaceSlot(expr, "source", sourceDomain);
    expr = pImpl->replaceSlot(expr, "reason", "shared structural pattern");

    res.expression = expr;
    res.aptness = 0.80;
    return res;
}

MetaphorResult MetaphorEngine::generateFromMapping(const std::string& targetConcept,
                                                   const yuki::reasoning::Mapping& mapping,
                                                   const yuki::reasoning::Domain& source) {
    MetaphorResult res;
    res.targetDomain = targetConcept;
    res.sourceDomain = source.name;
    res.aptness = mapping.score > 0.0 ? 0.90 : 0.40;
    res.expression = targetConcept + " -> " + source.name;
    return res;
}

std::vector<uint8_t> MetaphorEngine::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x4D455448; // 'METH'

    buf.resize(4);
    std::memcpy(buf.data(), &magic, 4);

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool MetaphorEngine::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x4D455448) return false;

    return true;
}

}} // namespace yuki::language
