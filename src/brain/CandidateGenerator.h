#pragma once
#include "MeaningTypes.h"
#include <string>
#include <vector>

struct CandidateResult {
    std::string text;
    float score;
    float editScore;
    float phoneticScore;
};

class CandidateGenerator {
public:
    CandidateGenerator();
    std::vector<CandidateResult> generate(const NormalizedInput& original, const UncertaintyReport& uncertainty);
private:
    float computePhoneticScore(const std::string& a, const std::string& b) const;
    std::string hinglishPhoneticHash(const std::string& word) const;
};
