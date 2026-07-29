#include "src/brain/language/LocalModelBenchmark.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testllamabench_versionedparser...\n";
    using namespace yuki::brain::language;

    std::string sampleOutput =
        "| model | size | params | backend | test | t/s |\n"
        "| Qwen2.5 | 1.5GB | 1.5B | SYCL | pp 512 | 45.50 |\n"
        "| Qwen2.5 | 1.5GB | 1.5B | SYCL | tg 128 | 12.80 |\n";

    LocalModelBenchmarkResult parsed = LocalModelBenchmark::parseBenchmarkOutput(sampleOutput);
    assert(parsed.success);
    assert(parsed.promptTokensPerSecond > 40.0f);
    assert(parsed.decodeTokensPerSecond > 10.0f);

    std::string garbageOutput = "llama-bench failed to run: invalid argument --foo";
    LocalModelBenchmarkResult failedParse = LocalModelBenchmark::parseBenchmarkOutput(garbageOutput);
    assert(!failedParse.success);

    std::cout << "[PASS] testllamabench_versionedparser completed cleanly.\n";
    return 0;
}
