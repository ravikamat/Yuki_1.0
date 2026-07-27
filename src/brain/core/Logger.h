#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

#ifdef ERROR
#undef ERROR
#endif

#ifdef DEBUG
#undef DEBUG
#endif

namespace yuki {

namespace core {

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

class Logger {
public:
    static Logger& instance();

    bool init(const std::string& log_file_path);
    void log(LogLevel level, const std::string& component, const std::string& message);
    void log(LogLevel level, const std::string& message) { log(level, "Core", message); }

    static void debug(const std::string& msg) { instance().log(LogLevel::DEBUG, "Core", msg); }
    static void info(const std::string& msg) { instance().log(LogLevel::INFO, "Core", msg); }
    static void warn(const std::string& msg) { instance().log(LogLevel::WARN, "Core", msg); }
    static void error(const std::string& msg) { instance().log(LogLevel::ERROR, "Core", msg); }

    void setMinLevel(LogLevel level);
    LogLevel getMinLevel() const { return min_level_; }
    size_t getBytesWritten() const { return bytes_written_; }
    void close();

private:
    Logger();
    ~Logger();

    std::mutex mtx_;
    std::ofstream file_;
    std::string log_file_path_;
    LogLevel min_level_{LogLevel::INFO};
    size_t bytes_written_{0};
    static constexpr size_t kMaxLogSize = 10 * 1024 * 1024; // 10MB

    void rotateIfNeeded();
    std::string levelToString(LogLevel level) const;
};

} // namespace core
} // namespace yuki

