#pragma once

#include "src/brain/platform/DeviceProfile.h"
#include <string>

namespace yuki::platform {

class PortabilityLayer {
public:
    static std::string getPlatformCapabilities();
    static bool isWindows();
    static bool isLinux();
    static bool isMacOS();
};

} // namespace yuki::platform
