#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace yuki {
namespace reasoning { class AnalogicalReasoning; struct Domain; struct Mapping; }
namespace emotion { class ValenceArousalModel; }

namespace language {

struct MetaphorResult {
    std::string expression;
    std::string sourceDomain;
    std::string targetDomain;
    double aptness = 0.0;
    bool isSimile = false;
};

class MetaphorEngine {
public:
    MetaphorEngine();
    ~MetaphorEngine();
    MetaphorEngine(const MetaphorEngine&) = delete;
    MetaphorEngine& operator=(const MetaphorEngine&) = delete;
    MetaphorEngine(MetaphorEngine&&) noexcept;
    MetaphorEngine& operator=(MetaphorEngine&&) noexcept;

    void setAnalogicalReasoning(yuki::reasoning::AnalogicalReasoning* analogy);
    void setValenceArousalModel(yuki::emotion::ValenceArousalModel* emotion);

    MetaphorResult generateMetaphor(const std::string& targetConcept,
                                    const std::string& sourceDomain);

    MetaphorResult generateSimile(const std::string& targetConcept,
                                  const std::string& sourceDomain);

    MetaphorResult generateFromMapping(const std::string& targetConcept,
                                       const yuki::reasoning::Mapping& mapping,
                                       const yuki::reasoning::Domain& source);

    bool loadTemplates(const std::string& filepath);
    void clearTemplates();

    // Binary serialization: magic = 0x4D455448 ('METH')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::language
