#pragma once
#include "MeaningTypes.h"
#include <string>

class InputNormalizer {
public:
    InputNormalizer();
    NormalizedInput normalize(const std::string& raw, const LanguageResult& lang);
};
