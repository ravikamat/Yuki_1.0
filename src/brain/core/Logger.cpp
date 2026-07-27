#include "brain/core/Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>


namespace yuki::core {

Logger::Logger() = default;

Logger::~Logger() {
    close();
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

bool Logger::init(const std::string& log_file_path) {
    std::lock_guard<std::mutex> lock(mtx_);
    log_file_path_ = log_file_path;

    std::filesystem::path p(log_file_path_);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    file_.open(log_file_path_, std::ios::app);
    if (!file_) return false;

    file_.seekp(0, std::ios::end);
    bytes_written_ = static_cast<size_t>(file_.tellp());
    return true;
}

void Logger::setMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mtx_);
    min_level_ = level;
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

void Logger::rotateIfNeeded() {
    if (bytes_written_ >= kMaxLogSize && !log_file_path_.empty()) {
        file_.close();
        std::string rotated = log_file_path_ + ".1";
        std::filesystem::rename(log_file_path_, rotated);
        file_.open(log_file_path_, std::ios::out | std::ios::trunc);
        bytes_written_ = 0;
    }
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (level < min_level_) return;

    rotateIfNeeded();

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm time_info{};
#if defined(_WIN32)
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif
    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);
    std::ostringstream oss;
    oss << "[" << time_str << "] ["
        << levelToString(level) << "] [" << component << "] " << message << "\n";



    std::string line = oss.str();
    if (file_.is_open()) {
        file_ << line;
        file_.flush();
        bytes_written_ += line.size();
    }
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_.is_open()) {
        file_.close();
    }
}

} // namespace yuki::core
