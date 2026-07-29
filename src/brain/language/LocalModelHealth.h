#pragma once

#include <string>
#include <cstdint>

namespace yuki::brain::language {

struct LocalModelHealthStatus {
    bool reachable{false};
    bool usable{false};
    int statusCode{0};
    int latencyMs{0};
    std::string statusText;
    std::string diagnostic;
};

class LocalModelHealth {
public:
    static LocalModelHealthStatus check(const std::string& host, uint16_t port, int timeoutMs = 3000);
    static LocalModelHealthStatus checkReadiness(const std::string& host, uint16_t port, int timeoutMs = 5000);
};

} // namespace yuki::brain::language
