#include "src/brain/language/LocalModelHealth.h"
#include <chrono>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace yuki::brain::language {

LocalModelHealthStatus LocalModelHealth::check(const std::string& host,
                                             unsigned short port,
                                             int timeoutMs) const {
    LocalModelHealthStatus status;
    auto tStart = std::chrono::steady_clock::now();

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"YukiLocalModelHealth/1.0",
                                    WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        status.diagnostic = "WinHttpOpen failed";
        return status;
    }

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::wstring wHost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        status.diagnostic = "WinHttpConnect failed to " + host + ":" + std::to_string(port);
        return status;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/health",
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        status.diagnostic = "WinHttpOpenRequest failed";
        return status;
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        if (dwStatusCode == 200) {
            status.reachable = true;
            status.diagnostic = "Server reachable at " + host + ":" + std::to_string(port);
        } else {
            status.diagnostic = "Server returned HTTP " + std::to_string(dwStatusCode);
        }
    } else {
        status.diagnostic = "WinHttp send/receive failed or timed out";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
#else
    status.diagnostic = "LocalModelHealth non-Windows stub";
#endif

    auto tEnd = std::chrono::steady_clock::now();
    status.latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count());
    return status;
}

} // namespace yuki::brain::language
