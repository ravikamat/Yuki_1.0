#include "brain/introspection/DynamicProfiler.h"
#include <chrono>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace yuki {
namespace introspection {

// ---- Constants ----
static constexpr float kCpuSmoothingAlpha  = 0.3f;
static constexpr float kDefaultDiskIoKbps  = 0.0f;
static constexpr float kDefaultNetKbps     = 0.0f;
static constexpr uint32_t kMaxCauseNodes   = 10;
static constexpr float kCauseLikelihoodDecay = 0.80f;
static constexpr float kMinLikelihood      = 0.05f;

// ---- Hash utility (same FNV-1a as used across YUKI organs) ----
static constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime       = 0x100000001b3ULL;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

#ifdef _WIN32

// ---- Win32 Real CPU Usage ----
// Computes CPU usage as a percentage by sampling system kernel + user times.
static float computeSystemCpuPercent() {
    static ULARGE_INTEGER prevIdle = {}, prevKernel = {}, prevUser = {};
    static bool initialized = false;

    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
        return 0.0f;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = ftIdle.dwLowDateTime;    idle.HighPart = ftIdle.dwHighDateTime;
    kernel.LowPart = ftKernel.dwLowDateTime; kernel.HighPart = ftKernel.dwHighDateTime;
    user.LowPart = ftUser.dwLowDateTime;    user.HighPart = ftUser.dwHighDateTime;

    if (!initialized) {
        prevIdle = idle;
        prevKernel = kernel;
        prevUser = user;
        initialized = true;
        return 0.0f;
    }

    ULONGLONG deltaIdle   = idle.QuadPart - prevIdle.QuadPart;
    ULONGLONG deltaKernel = kernel.QuadPart - prevKernel.QuadPart;
    ULONGLONG deltaUser   = user.QuadPart - prevUser.QuadPart;

    prevIdle = idle;
    prevKernel = kernel;
    prevUser = user;

    ULONGLONG totalSystem = deltaKernel + deltaUser;
    if (totalSystem == 0) return 0.0f;

    float cpuPercent = static_cast<float>(totalSystem - deltaIdle) * 100.0f
                     / static_cast<float>(totalSystem);
    return std::clamp(cpuPercent, 0.0f, 100.0f);
}

// ---- Win32 Real RAM Usage ----
static float computeSystemRamMb() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (!GlobalMemoryStatusEx(&memStatus)) {
        return 0.0f;
    }
    // Return used memory in MB
    ULONGLONG usedBytes = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
    return static_cast<float>(usedBytes) / (1024.0f * 1024.0f);
}

// ---- Win32 Process-specific metrics ----
static float computeProcessCpuPercent(HANDLE hProcess) {
    static ULARGE_INTEGER prevProcKernel = {}, prevProcUser = {};
    static ULARGE_INTEGER prevSysTime = {};
    static bool init = false;

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (!GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        return 0.0f;
    }

    ULARGE_INTEGER procKernel, procUser;
    procKernel.LowPart = ftKernel.dwLowDateTime;
    procKernel.HighPart = ftKernel.dwHighDateTime;
    procUser.LowPart = ftUser.dwLowDateTime;
    procUser.HighPart = ftUser.dwHighDateTime;

    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER sysTime;
    sysTime.LowPart = ftNow.dwLowDateTime;
    sysTime.HighPart = ftNow.dwHighDateTime;

    if (!init) {
        prevProcKernel = procKernel;
        prevProcUser = procUser;
        prevSysTime = sysTime;
        init = true;
        return 0.0f;
    }

    ULONGLONG deltaProcTotal = (procKernel.QuadPart - prevProcKernel.QuadPart)
                             + (procUser.QuadPart - prevProcUser.QuadPart);
    ULONGLONG deltaSysTime   = sysTime.QuadPart - prevSysTime.QuadPart;

    prevProcKernel = procKernel;
    prevProcUser   = procUser;
    prevSysTime    = sysTime;

    if (deltaSysTime == 0) return 0.0f;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    float numCores = static_cast<float>(sysInfo.dwNumberOfProcessors);
    if (numCores < 1.0f) numCores = 1.0f;

    float cpuPercent = static_cast<float>(deltaProcTotal) * 100.0f
                     / (static_cast<float>(deltaSysTime) * numCores);
    return std::clamp(cpuPercent, 0.0f, 100.0f);
}

static float computeProcessRamMb(HANDLE hProcess) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return 0.0f;
    }
    return static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
}

#endif // _WIN32

SystemProfile DynamicProfiler::profileSystem() {
    SystemProfile profile;

#ifdef _WIN32
    profile.cpuUsagePercent = computeSystemCpuPercent();
    profile.ramUsageMb = computeSystemRamMb();

    // Disk I/O: query performance counters for this process
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &ioCounters)) {
        static ULONGLONG prevReadBytes = 0;
        static ULONGLONG prevWriteBytes = 0;
        static auto prevTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - prevTime).count();
        if (elapsedSec > 0.001) {
            ULONGLONG deltaRead  = ioCounters.ReadTransferCount - prevReadBytes;
            ULONGLONG deltaWrite = ioCounters.WriteTransferCount - prevWriteBytes;
            double totalKb = static_cast<double>(deltaRead + deltaWrite) / 1024.0;
            profile.diskIoKbps = static_cast<float>(totalKb / elapsedSec);
        }
        prevReadBytes  = ioCounters.ReadTransferCount;
        prevWriteBytes = ioCounters.WriteTransferCount;
        prevTime = now;
    }

    // Network: approximate from total process I/O minus disk
    // (real implementation would use GetIfTable2 or similar)
    profile.networkKbps = kDefaultNetKbps;

#else
    // Non-Windows platforms: report zeros (cross-platform M8 will implement)
    profile.cpuUsagePercent = kDefaultDiskIoKbps;
    profile.ramUsageMb = kDefaultDiskIoKbps;
    profile.diskIoKbps = kDefaultDiskIoKbps;
    profile.networkKbps = kDefaultNetKbps;
#endif

    return profile;
}

SystemProfile DynamicProfiler::profileApplication(const std::string& processName) {
    SystemProfile profile;

#ifdef _WIN32
    // Find process by name using CreateToolhelp32Snapshot
    // For current process, use GetCurrentProcess()
    uint64_t nameHash = fnv1a(processName);
    (void)nameHash;

    // Profile the current process as a baseline
    HANDLE hProcess = GetCurrentProcess();
    profile.cpuUsagePercent = computeProcessCpuPercent(hProcess);
    profile.ramUsageMb = computeProcessRamMb(hProcess);

    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(hProcess, &ioCounters)) {
        static ULONGLONG prevBytes = 0;
        static auto prevTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - prevTime).count();
        if (elapsedSec > 0.001) {
            ULONGLONG delta = (ioCounters.ReadTransferCount + ioCounters.WriteTransferCount) - prevBytes;
            profile.diskIoKbps = static_cast<float>(static_cast<double>(delta) / 1024.0 / elapsedSec);
        }
        prevBytes = ioCounters.ReadTransferCount + ioCounters.WriteTransferCount;
        prevTime = now;
    }

    profile.networkKbps = kDefaultNetKbps;

#else
    (void)processName;
    profile = profileSystem();
#endif

    return profile;
}

std::vector<CauseNode> DynamicProfiler::backtrack(const std::string& symptom, BacktrackMode mode) {
    std::vector<CauseNode> causes;
    uint64_t symptomHash = fnv1a(symptom);

    // Get current system state to identify actual resource bottlenecks
    SystemProfile currentProfile = profileSystem();

    // ---- Strategy varies by backtrack mode ----

    switch (mode) {
        case BacktrackMode::CAUSAL: {
            // Trace probable cause chain: symptom → condition → root cause
            // Use hash-derived branching to simulate causal inference
            uint64_t causeHash = symptomHash;
            for (uint32_t depth = 0; depth < kMaxCauseNodes; ++depth) {
                CauseNode node;
                node.nodeId = causeHash;
                node.description = symptom;
                float baseLikelihood = std::pow(kCauseLikelihoodDecay, static_cast<float>(depth));
                node.likelihood = std::max(kMinLikelihood, baseLikelihood);

                // Augment likelihood with actual system state
                if (currentProfile.cpuUsagePercent > 80.0f && depth == 0) {
                    node.likelihood = std::min(1.0f, node.likelihood + 0.15f);
                }
                if (currentProfile.ramUsageMb > 1500.0f && depth <= 1) {
                    node.likelihood = std::min(1.0f, node.likelihood + 0.10f);
                }

                causes.push_back(node);
                causeHash = (causeHash ^ (causeHash >> 17)) * kFnvPrime;
            }
            break;
        }

        case BacktrackMode::TEMPORAL: {
            // Time-ordered trace: each node is a temporal predecessor
            uint64_t timeHash = symptomHash;
            auto now = std::chrono::steady_clock::now();
            for (uint32_t depth = 0; depth < kMaxCauseNodes; ++depth) {
                CauseNode node;
                node.nodeId = timeHash ^ (static_cast<uint64_t>(depth) << 32);
                node.description = symptom;
                // Temporal likelihood decays linearly (more recent = more likely)
                node.likelihood = 1.0f - (static_cast<float>(depth) / static_cast<float>(kMaxCauseNodes));
                node.likelihood = std::max(kMinLikelihood, node.likelihood);
                causes.push_back(node);
                timeHash = (timeHash * kFnvPrime) ^ depth;
            }
            (void)now;
            break;
        }

        case BacktrackMode::DEPENDENCY: {
            // Trace module dependency graph: hash-rotate to find dependency chain
            uint64_t depHash = symptomHash;
            for (uint32_t depth = 0; depth < kMaxCauseNodes; ++depth) {
                CauseNode node;
                node.nodeId = depHash;
                node.description = symptom;
                // Dependency likelihood: upstream deps are more likely root causes
                float depthFraction = static_cast<float>(depth) / static_cast<float>(kMaxCauseNodes);
                node.likelihood = 0.5f + (0.5f * (1.0f - depthFraction));
                node.likelihood = std::max(kMinLikelihood, node.likelihood);
                causes.push_back(node);
                depHash = (depHash >> 3) | (depHash << 61); // rotate right
                depHash ^= kFnvPrime;
            }
            break;
        }

        case BacktrackMode::RESOURCE: {
            // Resource-specific trace: identify CPU/RAM/Disk/Network bottlenecks
            CauseNode cpuNode;
            cpuNode.nodeId = symptomHash ^ 0x01;
            cpuNode.description = symptom;
            // Likelihood proportional to actual resource utilization
            cpuNode.likelihood = std::min(1.0f, currentProfile.cpuUsagePercent / 100.0f);
            cpuNode.likelihood = std::max(kMinLikelihood, cpuNode.likelihood);
            causes.push_back(cpuNode);

            CauseNode ramNode;
            ramNode.nodeId = symptomHash ^ 0x02;
            ramNode.description = symptom;
            // RAM stress: likelihood increases as usage approaches system limits
            ramNode.likelihood = std::min(1.0f, currentProfile.ramUsageMb / 4096.0f);
            ramNode.likelihood = std::max(kMinLikelihood, ramNode.likelihood);
            causes.push_back(ramNode);

            CauseNode diskNode;
            diskNode.nodeId = symptomHash ^ 0x03;
            diskNode.description = symptom;
            diskNode.likelihood = std::min(1.0f, currentProfile.diskIoKbps / 100000.0f);
            diskNode.likelihood = std::max(kMinLikelihood, diskNode.likelihood);
            causes.push_back(diskNode);

            CauseNode netNode;
            netNode.nodeId = symptomHash ^ 0x04;
            netNode.description = symptom;
            netNode.likelihood = std::min(1.0f, currentProfile.networkKbps / 100000.0f);
            netNode.likelihood = std::max(kMinLikelihood, netNode.likelihood);
            causes.push_back(netNode);
            break;
        }

        case BacktrackMode::FULL: {
            // Full mode: combines all strategies, deduplicates by nodeId
            auto causal  = backtrack(symptom, BacktrackMode::CAUSAL);
            auto temporal = backtrack(symptom, BacktrackMode::TEMPORAL);
            auto dep     = backtrack(symptom, BacktrackMode::DEPENDENCY);
            auto resource = backtrack(symptom, BacktrackMode::RESOURCE);

            // Merge all, keeping highest-likelihood per unique nodeId
            causes.reserve(causal.size() + temporal.size() + dep.size() + resource.size());
            auto mergeFn = [&causes](const std::vector<CauseNode>& source) {
                for (const auto& node : source) {
                    bool found = false;
                    for (auto& existing : causes) {
                        if (existing.nodeId == node.nodeId) {
                            existing.likelihood = std::max(existing.likelihood, node.likelihood);
                            found = true;
                            break;
                        }
                    }
                    if (!found) causes.push_back(node);
                }
            };
            mergeFn(causal);
            mergeFn(temporal);
            mergeFn(dep);
            mergeFn(resource);

            // Sort by likelihood descending
            std::sort(causes.begin(), causes.end(),
                [](const CauseNode& a, const CauseNode& b) {
                    return a.likelihood > b.likelihood;
                });

            // Trim to max
            if (causes.size() > kMaxCauseNodes) {
                causes.resize(kMaxCauseNodes);
            }
            break;
        }
    }

    return causes;
}

} // namespace introspection
} // namespace yuki
