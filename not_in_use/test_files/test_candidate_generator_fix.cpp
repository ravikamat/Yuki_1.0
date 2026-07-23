#include <cassert>
#include <string>
#include "brain/MeaningTypes.h"
#include "brain/CandidateGenerator.h"

int main() {
    CandidateGenerator generator;

    NormalizedInput input;
    input.raw_text = "test token";

    UncertaintyReport report;
    UncertaintyReport::TokenFlag flag;
    flag.token = "test_word";
    report.token_flags.push_back(flag);

    auto candidates = generator.generate(input, report);
    assert(candidates.size() == 1);
    assert(candidates[0].text == "test_word");
    assert(candidates[0].score == 1.0f);

    return 0;
}
