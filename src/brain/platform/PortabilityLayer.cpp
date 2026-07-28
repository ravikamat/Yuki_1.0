#include "src/brain/platform/PortabilityLayer.h"

namespace yuki::platform {

std::string PortabilityLayer::getPlatformCapabilities() {
#if defined(_WIN32) || defined(_WIN64)
    return "OS: Windows, Threads: Native, Storage: Win32/SQLite";
#elif defined(__APPLE__)
    return "OS: macOS, Threads: POSIX, Storage: SQLite";
#else
    return "OS: Linux, Threads: POSIX, Storage: SQLite";
#endif
}

bool PortabilityLayer::isWindows() {
#if defined(_WIN32) || defined(_WIN64)
    return true;
#else
    return false;
#endif
}

bool PortabilityLayer::isLinux() {
#if defined(__linux__)
    return true;
#else
    return false;
#endif
}

bool PortabilityLayer::isMacOS() {
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

} // namespace yuki::platform
