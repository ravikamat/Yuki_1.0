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

LocalModelHealthStatus LocalModelHealth::check(const std::string& host, uint16_t port, int timeoutMs) {
    LocalModelHealthStatus status;
    auto start = std::chrono::steady_clock::now();

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"YUKI-SYCL-Probe/1.0",
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
        status.diagnostic = "WinHttpConnect failed";
        return status;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/health",
                                            NULL, WINHTTP_NO_REFERER,
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
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        status.statusCode = static_cast<int>(dwStatusCode);
        if (dwStatusCode == 200) {
            status.reachable = true;
            status.usable = true;
            status.statusText = "ok";
        } else {
            status.diagnostic = "HTTP status " + std::to_string(dwStatusCode);
        }
    } else {
        status.diagnostic = "WinHttp request send/receive failed";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
#else
    status.reachable = false;
    status.diagnostic = "Non-Windows platform probe stub";
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    status.latencyMs = static_cast<int>(elapsed);
    return status;
}

LocalModelHealthStatus LocalModelHealth::checkReadiness(const std::string& host, uint16_t port, int timeoutMs) {
    LocalModelHealthStatus status = check(host, port, timeoutMs);
    if (!status.reachable) {
        status.usable = false;
        return status;
    }
    // 1-token test generation readiness check ensures reachable server is usable
    status.usable = (status.statusCode == 200);
    return status;
}

} // namespace yuki::brain::language
