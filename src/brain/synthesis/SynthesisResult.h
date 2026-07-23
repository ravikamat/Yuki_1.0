#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace yuki {
namespace synthesis {

struct SynthesisResult {
    enum class Status : uint8_t {
        PENDING = 0,
        GENERATED = 1,
        COMPILED = 2,
        TESTED = 3,
        INTEGRATED = 4,
        FAILED_GENERATION = 5,
        FAILED_COMPILATION = 6,
        FAILED_TESTS = 7,
        REJECTED_BY_SANDBOX = 8,
        COUNT = 9
    };

    Status status = Status::PENDING;
    std::string generated_header;
    std::string generated_source;
    std::string output_header_path;
    std::string output_source_path;
    bool compiled = false;
    int compile_exit_code = -1;
    std::string compile_stdout;
    std::string compile_stderr;
    bool tests_passed = false;
    int test_exit_code = -1;
    float measured_competence_delta = 0.0f;
    uint32_t error_code = 0;

    bool isSuccess() const noexcept {
        return status == Status::INTEGRATED;
    }
};

} // namespace synthesis
} // namespace yuki
