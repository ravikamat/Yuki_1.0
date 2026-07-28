#pragma once

#include <string>

namespace yuki::brain::language {

struct LocalModelHealthStatus {
    bool reachable{false};
    int latencyMs{0};
    std::string modelName;
    std::string diagnostic;
};

class LocalModelHealth {
public:
    LocalModelHealthStatus check(const std::string& host,
                                 unsigned short port,
                                 int timeoutMs) const;
};

} // namespace yuki::brain::language
