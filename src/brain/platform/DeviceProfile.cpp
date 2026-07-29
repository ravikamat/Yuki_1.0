#include "src/brain/platform/DeviceProfile.h"
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_4.h>
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
    prof.cpuUsageKnown = true;

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    prof.logicalCoreCount = sysInfo.dwNumberOfProcessors;

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        prof.ramMb = static_cast<std::size_t>(memStatus.ullTotalPhys / (1024 * 1024));
        prof.availablePhysicalRamMb = static_cast<uint64_t>(memStatus.ullAvailPhys / (1024 * 1024));
        prof.cpuUsagePercent = static_cast<float>(memStatus.dwMemoryLoad);
    }

    // DXGI Intel GPU & Memory Query using IDXGIAdapter3
    IDXGIFactory4* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory))) {
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(pAdapter->GetDesc1(&desc))) {
                if (desc.VendorId == 0x8086) {
                    prof.intelGpuPresent = true;
                    prof.gpuAvailable = true;
                    char descStr[128];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, descStr, sizeof(descStr), nullptr, nullptr);
                    prof.gpuName = descStr;

                    std::ostringstream luidStream;
                    luidStream << std::hex << desc.AdapterLuid.HighPart << "-" << desc.AdapterLuid.LowPart;
                    prof.deviceLuid = luidStream.str();

                    IDXGIAdapter3* pAdapter3 = nullptr;
                    if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                        DXGI_QUERY_VIDEO_MEMORY_INFO nodeInfoDedicated = {};
                        if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &nodeInfoDedicated))) {
                            if (nodeInfoDedicated.Budget > 0) {
                                prof.gpuDedicatedMemoryPercent = (static_cast<float>(nodeInfoDedicated.CurrentUsage) / static_cast<float>(nodeInfoDedicated.Budget)) * 100.0f;
                                prof.gpuUsageKnown = true;
                                prof.gpuUsagePercent = prof.gpuDedicatedMemoryPercent;
                            }
                        }

                        DXGI_QUERY_VIDEO_MEMORY_INFO nodeInfoShared = {};
                        if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nodeInfoShared))) {
                            if (nodeInfoShared.Budget > 0) {
                                prof.gpuSharedMemoryPercent = (static_cast<float>(nodeInfoShared.CurrentUsage) / static_cast<float>(nodeInfoShared.Budget)) * 100.0f;
                            }
                        }
                        pAdapter3->Release();
                    }
                }
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }
#else
    prof.logicalCoreCount = 4;
    prof.availablePhysicalRamMb = 4096;
    prof.gpuUsageKnown = false;
#endif

    return prof;
}

} // namespace yuki::platform
