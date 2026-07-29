#include "src/brain/language/LlamaCppSyclBackend.h"
#include "src/brain/language/LocalModelHealth.h"
#include <chrono>
#include <thread>
#include <sstream>
#include <regex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace yuki::brain::language {

LlamaCppSyclBackend::LlamaCppSyclBackend(const platform::LocalModelRuntimeConfig& config)
    : m_config(config) {
    m_lease.endpoint = "http://" + m_config.llamaCpp.host + ":" + std::to_string(m_config.llamaCpp.port);
}

LlamaCppSyclBackend::~LlamaCppSyclBackend() {
    if (m_lease.ownedByYuki && m_process.isRunning()) {
        m_process.terminate();
    }
}

bool LlamaCppSyclBackend::initialize() {
    m_attestation = LocalModelAttestation::load("data/benchmarks/local_model_sycl_baseline.json");

    auto health = LocalModelHealth::checkReadiness(m_config.llamaCpp.host, m_config.llamaCpp.port, m_config.llamaCpp.healthTimeoutMs);
    if (health.reachable && health.usable) {
        m_lease.attachedToExistingServer = true;
        m_lease.ownedByYuki = false;
        m_initialized = true;
        return true;
    }

    std::string cmd = "\"" + m_config.llamaCpp.serverExecutable + "\""
        + " -m \"" + m_config.llamaCpp.modelPath + "\""
        + " --host " + m_config.llamaCpp.host
        + " --port " + std::to_string(m_config.llamaCpp.port)
        + " -c " + std::to_string(m_config.llamaCpp.contextSize)
        + " -ngl " + std::to_string(m_config.llamaCpp.gpuLayers);

    if (!m_process.startDetached(cmd, "")) {
        return false;
    }

    m_lease.attachedToExistingServer = false;
    m_lease.ownedByYuki = true;
    m_lease.processId = m_process.getProcessId();
    m_lease.ownershipToken = "yuki-owned-" + std::to_string(m_lease.processId);

    int elapsed = 0;
    int sleepMs = 200;
    while (elapsed < m_config.llamaCpp.startupTimeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        elapsed += sleepMs;
        sleepMs = std::min(sleepMs * 2, 2000);

        auto pollHealth = LocalModelHealth::checkReadiness(m_config.llamaCpp.host, m_config.llamaCpp.port, m_config.llamaCpp.healthTimeoutMs);
        if (pollHealth.reachable && pollHealth.usable) {
            m_initialized = true;
            return true;
        }
    }

    return false;
}

bool LlamaCppSyclBackend::available() const {
    return m_initialized;
}

GenerationResult LlamaCppSyclBackend::generate(const GenerationRequest& request) {
    GenerationResult result;
    result.backendName = getBackendName();
    result.backend = getKind();
    result.backendKind = getKind();
    result.accelerated = true;
    result.deviceName = "Intel SYCL GPU";

    if (!m_initialized) {
        result.success = false;
        result.diagnostic = "Backend not initialized";
        result.failureReason = result.diagnostic;
        return result;
    }

    auto start = std::chrono::steady_clock::now();

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"YUKI-LlamaCppSycl/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        result.success = false;
        result.diagnostic = "WinHttpOpen failed";
        result.failureReason = result.diagnostic;
        return result;
    }

    WinHttpSetTimeouts(hSession,
                       m_config.llamaCpp.requestTimeoutMs,
                       m_config.llamaCpp.requestTimeoutMs,
                       m_config.llamaCpp.requestTimeoutMs,
                       m_config.llamaCpp.requestTimeoutMs);

    std::wstring wHost(m_config.llamaCpp.host.begin(), m_config.llamaCpp.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.llamaCpp.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.success = false;
        result.diagnostic = "WinHttpConnect failed";
        result.failureReason = result.diagnostic;
        return result;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/completion",
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.success = false;
        result.diagnostic = "WinHttpOpenRequest failed";
        result.failureReason = result.diagnostic;
        return result;
    }

    std::string jsonBody = "{\"prompt\":\"" + request.prompt + "\",\"n_predict\":"
        + std::to_string(request.maxTokens > 0 ? request.maxTokens : 256)
        + ",\"temperature\":" + std::to_string(request.temperature) + "}";

    LPCWSTR headers = L"Content-Type: application/json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(-1L),
                                       (LPVOID)jsonBody.c_str(), static_cast<DWORD>(jsonBody.size()),
                                       static_cast<DWORD>(jsonBody.size()), 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        std::string responseText;
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            std::vector<char> buffer(dwSize + 1, 0);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                responseText.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);

        std::regex contentRegex(R"raw("content"\s*:\s*"((?:[^"\\]|\\.)*)")raw");
        std::smatch match;
        if (std::regex_search(responseText, match, contentRegex) && match.size() > 1) {
            result.text = match[1].str();
            result.success = true;
        } else {
            result.text = responseText;
            result.success = !responseText.empty();
        }
    } else {
        result.success = false;
        result.diagnostic = "WinHttp POST /completion failed";
        result.failureReason = result.diagnostic;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
#else
    result.success = false;
    result.diagnostic = "Non-Windows platform execution stub";
    result.failureReason = result.diagnostic;
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    result.elapsedMs = static_cast<float>(elapsed);
    result.latencyMs = result.elapsedMs;

    if (elapsed > 0) {
        result.decodeTokensPerSecond = static_cast<float>(result.outputTokenCount) / (static_cast<float>(elapsed) / 1000.0f);
    }

    return result;
}

} // namespace yuki::brain::language
