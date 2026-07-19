#pragma once
#include "MeaningTypes.h"
#include <string>

class LanguageLayer {
public:
    LanguageLayer();
    LanguageResult detect(const std::string& input) const;
    LanguageResult analyse(const std::string& input) const;
    std::string adaptResponse(const std::string& englishResponse, const LanguageResult& lr) const;
    static std::string whisperLanguageParam() { return ""; }
};
