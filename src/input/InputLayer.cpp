// InputLayer.cpp — Input perception + body state telemetry (merged)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include "input/InputLayer.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

// ══════════════════════════════════════════════════════════════════════════════
// InputPerceptionBuilder
// ══════════════════════════════════════════════════════════════════════════════

static std::string ip_trim(const std::string& s) {
    const std::string ws = " \t\r\n";
    std::size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}
static std::string ip_toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

InputPerception InputPerceptionBuilder::analyze(const std::string& input) const {
    InputPerception p;
    p.raw_text = input;
    p.normalized_text = ip_toLower(ip_trim(input));
    if (p.normalized_text.empty()) { p.is_empty = true; return p; }
    p.char_count = p.normalized_text.size();
    std::istringstream iss(p.normalized_text); std::string chunk;
    while (iss >> chunk) p.chunks.push_back(chunk);
    p.chunk_count = p.chunks.size();
    if (p.chunk_count > 0) { p.first_chunk = p.chunks.front(); p.last_chunk = p.chunks.back(); }
    p.has_question_mark = (p.normalized_text.find('?') != std::string::npos);
    p.has_name_signal   = (p.normalized_text.find("yuki") != std::string::npos);
    const std::string actions[]   = {"open","play","run","launch","close"};
    const std::string questions[] = {"what","how","why","when","where","who"};
    for (const auto& c : p.chunks) {
        for (const auto& a : actions)   if (c==a) { p.has_action_cue=true; break; }
        for (const auto& q : questions) if (c==q) { p.has_question_cue=true; break; }
    }
    return p;
}

// ══════════════════════════════════════════════════════════════════════════════
// BodyStateReader
// ══════════════════════════════════════════════════════════════════════════════

static uint64_t filetimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

BodyStateSnapshot BodyStateReader::capture(const SubsystemControl& control) const {
    BodyStateSnapshot snap;
    SubsystemStatus status = control.getStatus(SubsystemName::BODY_STATE);
    snap.allowed=status.active; snap.subsystem_available=status.available; snap.subsystem_active=status.active;
    if (!snap.subsystem_active) { snap.summary="BodyState blocked or unavailable."; return snap; }

    MEMORYSTATUSEX memInfo; memInfo.dwLength=sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        snap.memory_load_percent          = static_cast<size_t>(memInfo.dwMemoryLoad);
        snap.total_physical_memory_mb     = memInfo.ullTotalPhys/(1024*1024);
        snap.available_physical_memory_mb = memInfo.ullAvailPhys/(1024*1024);
    }
    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExA(NULL,&freeBytes,&totalBytes,&totalFreeBytes)) {
        snap.total_storage_gb = totalBytes.QuadPart/(1024*1024*1024);
        snap.free_storage_gb  = totalFreeBytes.QuadPart/(1024*1024*1024);
    }
    DWORD flags; snap.internet_available = InternetGetConnectedState(&flags,0);

    FILETIME idle1,kernel1,user1,idle2,kernel2,user2;
    if (GetSystemTimes(&idle1,&kernel1,&user1)) {
        Sleep(100);
        if (GetSystemTimes(&idle2,&kernel2,&user2)) {
            uint64_t idleDelta  = filetimeToUint64(idle2)-filetimeToUint64(idle1);
            uint64_t kernDelta  = filetimeToUint64(kernel2)-filetimeToUint64(kernel1);
            uint64_t userDelta  = filetimeToUint64(user2)-filetimeToUint64(user1);
            uint64_t totalDelta = kernDelta+userDelta;
            if (totalDelta>0) {
                double usage = 100.0*static_cast<double>(totalDelta-idleDelta)/static_cast<double>(totalDelta);
                if (usage<0.0) usage=0.0; if (usage>100.0) usage=100.0;
                snap.cpu_usage_percent = usage;
            }
        }
    }
    snap.telemetry_available=true; snap.temperature_available=false;
    std::ostringstream ss;
    ss << (snap.cpu_usage_percent>=80.0||snap.memory_load_percent>=85?"Body under load: ":"Body stable: ");
    ss << "CPU " << std::fixed << std::setprecision(1) << snap.cpu_usage_percent << "%, ";
    ss << "RAM load " << snap.memory_load_percent << "%, ";
    ss << "storage free " << snap.free_storage_gb << " GB, ";
    ss << "internet " << (snap.internet_available?"on.":"off.");
    snap.summary = ss.str(); return snap;
}
