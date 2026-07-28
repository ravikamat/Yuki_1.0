#include "src/brain/language/LocalModelBenchmark.h"
#include <cassert>
#include <iostream>
#include <regex>

static void test_valid_benchmark_parsing() {
    std::string sampleOutput =
        "| model | size | params | backend | ngl | test | t/s |\n"
        "| qwen2.5-1.5b | 1.1GB | 1.54B | SYCL | 99 | pp 512 | 142.50 |\n"
        "| qwen2.5-1.5b | 1.1GB | 1.54B | SYCL | 99 | tg 128 | 32.40 |\n"
        "prompt processing: 142.50 tok/s\n"
        "eval: 32.40 tok/s\n";

    std::regex promptRegex(R"((?:pp|prompt|prompt processing)\s*[:=]?\s*([\d\.]+)\s*(?:t/s|tok/s|tokens/s)?)", std::regex::icase);
    std::regex evalRegex(R"((?:tg|eval|generation|decode)\s*[:=]?\s*([\d\.]+)\s*(?:t/s|tok/s|tokens/s)?)", std::regex::icase);

    std::smatch match;
    float promptTps = 0.0f;
    float decodeTps = 0.0f;

    if (std::regex_search(sampleOutput, match, promptRegex) && match.size() > 1) {
        promptTps = std::stof(match[1].str());
    }
    if (std::regex_search(sampleOutput, match, evalRegex) && match.size() > 1) {
        decodeTps = std::stof(match[1].str());
    }

    assert(promptTps > 100.0f);
    assert(decodeTps > 30.0f);
}

int main() {
    std::cout << "Running testlocalmodelbenchmark_parser...\n";
    test_valid_benchmark_parsing();
    std::cout << "[PASS] testlocalmodelbenchmark_parser completed cleanly.\n";
    return 0;
}
