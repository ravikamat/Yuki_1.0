#include "brain/core/Logger.h"
#include <cassert>
#include <fstream>
#include <thread>
#include <vector>
#include <cstdio>

int main() {
    auto& logger = yuki::core::Logger::instance();
    logger.init("test_yuki_system.log");
    logger.setMinLevel(yuki::core::LogLevel::INFO);

    // 1. log() writes to file
    logger.log(yuki::core::LogLevel::INFO, "TEST_COMP", "Test message 123");
    assert(logger.getBytesWritten() > 0);

    // 2. log() below min level is ignored
    size_t before = logger.getBytesWritten();
    logger.log(yuki::core::LogLevel::DEBUG, "TEST_COMP", "Debug should be ignored");
    assert(logger.getBytesWritten() == before);

    // 3. thread safety: 4 threads logging simultaneously -> no corruption
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 50; ++j) {
                logger.log(yuki::core::LogLevel::INFO, "THREAD_" + std::to_string(i), "Concurrent log entry");
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    // 4. destructor / close file handle cleanly
    logger.close();

    // 5. verify file contents
    std::ifstream in("test_yuki_system.log");
    assert(in.is_open());
    std::string first_line;
    std::getline(in, first_line);
    assert(first_line.find("INFO") != std::string::npos);
    assert(first_line.find("TEST_COMP") != std::string::npos);
    in.close();

    std::remove("test_yuki_system.log");
    return 0;
}
