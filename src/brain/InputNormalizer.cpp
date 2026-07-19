#include "InputNormalizer.h"

InputNormalizer::InputNormalizer() {}

NormalizedInput InputNormalizer::normalize(const std::string& raw, const LanguageResult& lang) {
    NormalizedInput norm;
    norm.raw_text = raw;
    norm.clean_text = raw;
    norm.canonical_text = raw;
    norm.lang_code = lang.code;
    return norm;
}
