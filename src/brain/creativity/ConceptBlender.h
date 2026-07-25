#pragma once
#include <vector>
#include <cstdint>
#include <memory>

namespace yuki { namespace creativity {

enum class BlendMode { CONVEX, MULTIPLICATIVE };

struct BlendResult {
    std::vector<double> blendVector;
    double novelty = 0.0;
    double divergence = 0.0;
    double alpha = 0.5;
    BlendMode mode = BlendMode::CONVEX;
};

class ConceptBlender {
public:
    explicit ConceptBlender(size_t embeddingDim);
    ~ConceptBlender();
    ConceptBlender(const ConceptBlender&) = delete;
    ConceptBlender& operator=(const ConceptBlender&) = delete;
    ConceptBlender(ConceptBlender&&) noexcept;
    ConceptBlender& operator=(ConceptBlender&&) noexcept;

    void setConceptLibrary(const std::vector<std::vector<double>>& library);
    void clearConceptLibrary();

    BlendResult blend(const std::vector<double>& a,
                      const std::vector<double>& b,
                      BlendMode mode = BlendMode::CONVEX,
                      double alpha = 0.5);

    std::vector<BlendResult> blendSeries(const std::vector<double>& a,
                                         const std::vector<double>& b,
                                         BlendMode mode,
                                         size_t numSteps);

    BlendResult selectMostNovel(const std::vector<BlendResult>& blends);
    BlendResult selectMostDivergent(const std::vector<BlendResult>& blends);

    // Binary serialization: magic = 0x43424C44 ('CBLD')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    size_t getEmbeddingDim() const;
    size_t getLibrarySize() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::creativity
