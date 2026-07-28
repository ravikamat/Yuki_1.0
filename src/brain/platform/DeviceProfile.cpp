#include "src/brain/platform/DeviceProfile.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace yuki::platform {

DeviceProfile DeviceProfileDetector::detectCurrent() {
    DeviceProfile prof;
    prof.ramMb = 8192;
    prof.freeDiskMb = 50000;
    prof.cpuLoad = 0.25f;
    prof.gpuAvailable = false;
    prof.onBattery = false;
    prof.networkAvailable = true;
    prof.os = "Windows";
    prof.tier = DeviceTier::MID;

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    prof.logicalCoreCount = sysInfo.dwNumberOfProcessors;

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        prof.ramMb = static_cast<std::size_t>(memStatus.ullTotalPhys / (1024 * 1024));
        prof.availablePhysicalRamMb = static_cast<uint64_t>(memStatus.ullAvailPhys / (1024 * 1024));
        prof.cpuUsagePercent = static_cast<float>(memStatus.dwMemoryLoad); // Proxy load metric
    }

    // DXGI Intel GPU detection
    IDXGIFactory* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        IDXGIAdapter* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                // Intel Vendor ID is 0x8086
                if (desc.VendorId == 0x8086) {
                    prof.intelGpuPresent = true;
                    prof.gpuAvailable = true;
                    char descStr[128];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, descStr, sizeof(descStr), nullptr, nullptr);
                    prof.gpuName = descStr;
                }
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }
#else
    prof.logicalCoreCount = 4;
    prof.availablePhysicalRamMb = 4096;
#endif

    return prof;
}

} // namespace yuki::platform
