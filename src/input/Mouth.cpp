// Mouth.cpp
#include "input/Mouth.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#include <mmsystem.h>

// Standalone SAPI helpers replacing sphelper.h (which requires ATL / atlbase.h)
inline HRESULT SpEnumTokens(const WCHAR* pszCategoryId, const WCHAR* pszReqAttribs,
                            const WCHAR* pszOptAttribs, IEnumSpObjectTokens** ppEnum) {
    ISpObjectTokenCategory* pCategory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                  __uuidof(ISpObjectTokenCategory), (void**)&pCategory);
    if (SUCCEEDED(hr)) {
        hr = pCategory->SetId(pszCategoryId, FALSE);
        if (SUCCEEDED(hr)) {
            hr = pCategory->EnumTokens(pszReqAttribs, pszOptAttribs, ppEnum);
        }
        pCategory->Release();
    }
    return hr;
}

inline HRESULT SpGetDescription(ISpObjectToken* pToken, LPWSTR* ppszDescription, LANGID = 0) {
    if (!pToken || !ppszDescription) return E_POINTER;
    return pToken->GetStringValue(nullptr, ppszDescription);
}
#endif
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cctype>

#pragma comment(lib, "winmm.lib")

// -------------------------------------------------
// KokoroBackend Definition & Implementation
// -------------------------------------------------
class KokoroBackend : public ITtsBackend {
public:
    KokoroBackend() : isBat_(false) {}

    void setTtsPaths(const TtsPaths& paths) {
        paths_ = paths;
    }

    bool probe() override {
        // Probe for kokoro_runner.bat or kokoro.exe in the workspace layout
        DWORD dwAttrib = GetFileAttributesA(paths_.kokoroRunnerBat.c_str());
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            runnerPath_ = paths_.kokoroRunnerBat;
            isBat_ = true;
            return true;
        }
        dwAttrib = GetFileAttributesA(paths_.kokoroExe.c_str());
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            runnerPath_ = paths_.kokoroExe;
            isBat_ = false;
            return true;
        }
        return false;
    }

    bool initialize() override {
        return probe();
    }

    void shutdown() override {}

    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) override {
        if (runnerPath_.empty()) {
            err = "Kokoro runner not found.";
            return false;
        }

        // Create output directory if it doesn't exist
        CreateDirectoryA(paths_.ttsOutputDir.c_str(), NULL);

        // Write text to a temporary file to avoid shell command quoting limitations
        std::string tempTextPath = paths_.kokoroInputTxt;
        std::ofstream out(tempTextPath);
        if (!out.is_open()) {
            err = "Failed to write temp input file for Kokoro.";
            return false;
        }
        out << text;
        out.close();

        // Build command line:
        // For bat: cmd.exe /c data\kokoro\kokoro_runner.bat "data\tts\kokoro_input.txt" "wavPath"
        // For exe: data\kokoro\kokoro.exe --input "data\tts\kokoro_input.txt" --output "wavPath"
        std::string cmd;
        if (isBat_) {
            cmd = "cmd.exe /c \"" + runnerPath_ + "\" \"" + tempTextPath + "\" \"" + wavPath + "\"";
        } else {
            cmd = "\"" + runnerPath_ + "\" --input \"" + tempTextPath + "\" --output \"" + wavPath + "\"";
        }

        std::vector<char> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back('\0');

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            DWORD waitRes = WaitForSingleObject(pi.hProcess, 8000); // 8 seconds timeout
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (waitRes == WAIT_TIMEOUT) {
                err = "Kokoro synthesis process timed out.";
                return false;
            }
        } else {
            err = "Failed to launch Kokoro synthesis process.";
            return false;
        }

        // Verify output WAV exists and is non-empty
        std::ifstream f(wavPath, std::ios::binary | std::ios::ate);
        if (f.is_open() && f.tellg() > 44) {
            return true;
        }
        err = "Kokoro output file is missing or invalid.";
        return false;
    }

    std::string backendName() const override {
        return "Kokoro";
    }

    std::string voiceName() const override {
        return "Default Assistant English Voice";
    }

    VoiceBackendType backendType() const override {
        return VoiceBackendType::KOKORO;
    }

    bool isNeural() const override {
        return true;
    }

private:
    TtsPaths paths_;
    std::string runnerPath_;
    bool isBat_;
};

// -------------------------------------------------
// PiperBackend Definition & Implementation
// -------------------------------------------------
class PiperBackend : public ITtsBackend {
public:
    void setTtsPaths(const TtsPaths& paths) {
        paths_ = paths;
    }

    bool probe() override {
        // Detect piper.exe in common locations
        std::string foundExe;
        for (const auto& path : paths_.piperExePaths) {
            DWORD dwAttrib = GetFileAttributesA(path.c_str());
            if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                foundExe = path;
                break;
            }
        }

        if (foundExe.empty()) {
            return false;
        }

        // Detect models in preference order
        std::string foundModel;
        std::string foundModelName;
        for (const auto& m : paths_.piperModels) {
            DWORD modelAttrib = GetFileAttributesA(m.second.c_str());
            if (modelAttrib != INVALID_FILE_ATTRIBUTES && !(modelAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string jsonPath = m.second + ".json";
                DWORD jsonAttrib = GetFileAttributesA(jsonPath.c_str());
                if (jsonAttrib != INVALID_FILE_ATTRIBUTES && !(jsonAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                    foundModel = m.second;
                    foundModelName = m.first;
                    break;
                }
            }
        }

        if (foundModel.empty()) {
            return false;
        }

        exePath_ = foundExe;
        modelPath_ = foundModel;
        voiceName_ = foundModelName;
        return true;
    }

    bool initialize() override {
        return probe();
    }

    void shutdown() override {}

    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) override {
        if (exePath_.empty() || modelPath_.empty()) {
            err = "Piper backend is not fully initialized.";
            return false;
        }

        // Create output directory if it doesn't exist
        CreateDirectoryA(paths_.ttsOutputDir.c_str(), NULL);

        // Use anonymous pipes for stdin/stdout instead of shell piping
        SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
        HANDLE hChildReadStdin = INVALID_HANDLE_VALUE, hParentWriteStdin = INVALID_HANDLE_VALUE;
        HANDLE hParentReadStdout = INVALID_HANDLE_VALUE, hChildWriteStdout = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&hChildReadStdin, &hParentWriteStdin, &sa, 0)) {
            err = "Failed to create stdin pipe for Piper.";
            return false;
        }
        if (!SetHandleInformation(hParentWriteStdin, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildReadStdin); CloseHandle(hParentWriteStdin);
            err = "Failed to set stdin pipe handle information.";
            return false;
        }

        if (!CreatePipe(&hParentReadStdout, &hChildWriteStdout, &sa, 0)) {
            CloseHandle(hChildReadStdin); CloseHandle(hParentWriteStdin);
            err = "Failed to create stdout pipe for Piper.";
            return false;
        }
        if (!SetHandleInformation(hParentReadStdout, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildReadStdin); CloseHandle(hParentWriteStdin);
            CloseHandle(hParentReadStdout); CloseHandle(hChildWriteStdout);
            err = "Failed to set stdout pipe handle information.";
            return false;
        }

        // Build command line without shell piping
        std::string cmd = "\"" + exePath_ + "\" -m \"" + modelPath_ + "\" --output_file \"" + wavPath + "\"";

        std::vector<char> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back('\0');

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = hChildReadStdin;
        si.hStdOutput = hChildWriteStdout;
        si.hStdError = hChildWriteStdout;

        if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(hChildReadStdin); CloseHandle(hParentWriteStdin);
            CloseHandle(hParentReadStdout); CloseHandle(hChildWriteStdout);
            err = "Failed to launch Piper synthesis process.";
            return false;
        }

        // Close child handles in parent process
        CloseHandle(hChildReadStdin);
        CloseHandle(hChildWriteStdout);

        // Write text to Piper's stdin
        DWORD written = 0;
        if (!WriteFile(hParentWriteStdin, text.c_str(), (DWORD)text.size(), &written, nullptr) || written != text.size()) {
            CloseHandle(hParentWriteStdin);
            CloseHandle(hParentReadStdout);
            WaitForSingleObject(pi.hProcess, 1000);
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            err = "Failed to write input to Piper process.";
            return false;
        }
        CloseHandle(hParentWriteStdin); // Close stdin to signal EOF

        // Wait for process to complete
        DWORD waitRes = WaitForSingleObject(pi.hProcess, 8000); // 8 seconds timeout
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hParentReadStdout);

        if (waitRes == WAIT_TIMEOUT) {
            err = "Piper synthesis process timed out.";
            return false;
        }

        std::ifstream f(wavPath, std::ios::binary | std::ios::ate);
        if (f.is_open() && f.tellg() > 44) {
            return true;
        }
        err = "Piper output file is missing or invalid.";
        return false;
    }

    std::string backendName() const override {
        return "Piper";
    }

    std::string voiceName() const override {
        return voiceName_;
    }

    VoiceBackendType backendType() const override {
        return VoiceBackendType::PIPER;
    }

    bool isNeural() const override {
        return true;
    }

private:
    TtsPaths paths_;
    std::string exePath_;
    std::string modelPath_;
    std::string voiceName_;
};

// -------------------------------------------------
// SapiBackend Definition & Implementation
// -------------------------------------------------
class SapiBackend : public ITtsBackend {
public:
    SapiBackend() : pSpVoice_(nullptr), usingFemaleVoice_(false) {}
    ~SapiBackend() {
        shutdown();
    }

    bool probe() override {
        ISpVoice* pVoice = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void**)&pVoice);
        if (SUCCEEDED(hr) && pVoice) {
            pVoice->Release();
            return true;
        }
        return false;
    }

    bool initialize() override {
        if (pSpVoice_) return true;

        HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void**)&pSpVoice_);
        if (FAILED(hr) || !pSpVoice_) {
            pSpVoice_ = nullptr;
            return false;
        }

        pSpVoice_->SetVolume(100);
        pSpVoice_->SetRate(1);

        selectBestVoice();
        return true;
    }

    void shutdown() override {
        if (pSpVoice_) {
            pSpVoice_->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
            pSpVoice_->Release();
            pSpVoice_ = nullptr;
        }
    }

    // SAPI doesn't support file output directly; returns false to trigger speakDirect fallback
    bool synthesizeToFile(const std::string&, const std::string&, std::string& err) override {
        err = "SAPI does not support file output; use speakDirect()";
        return false;
    }

    bool speakDirect(const std::string& text, uint64_t serial, std::function<bool(uint64_t)> requestStillCurrent, std::function<void(SpeakPhase, const std::string&)> setPhase) {
        if (!pSpVoice_) return false;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wlen <= 0) return false;

        wchar_t* wtext = new wchar_t[wlen];
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext, wlen);

        setPhase(SpeakPhase::SPEAKING, text);
        HRESULT hr = pSpVoice_->Speak(wtext, SPF_ASYNC | SPF_PURGEBEFORESPEAK, NULL);
        delete[] wtext;

        if (SUCCEEDED(hr)) {
            while (requestStillCurrent(serial)) {
                SPVOICESTATUS status;
                HRESULT statHr = pSpVoice_->GetStatus(&status, NULL);
                bool stillSpeaking = false;
                if (SUCCEEDED(statHr)) {
                    stillSpeaking = (status.dwRunningState == SPRS_IS_SPEAKING);
                }
                if (!stillSpeaking) {
                    break;
                }
                Sleep(20);
            }
            if (!requestStillCurrent(serial)) {
                pSpVoice_->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
                setPhase(SpeakPhase::INTERRUPTED, text);
                return false;
            }
            return true;
        }
        return false;
    }

    std::string backendName() const override {
        return "SAPI";
    }

    std::string voiceName() const override {
        return selectedVoiceName_;
    }

    VoiceBackendType backendType() const override {
        return VoiceBackendType::SAPI;
    }

    bool isNeural() const override {
        return false;
    }

    bool isFemalePreferred() const {
        return usingFemaleVoice_;
    }

private:
    std::wstring getTokenStringAttribute(ISpObjectToken* token, const wchar_t* key) const {
        if (!token) return L"";
        if (wcscmp(key, L"Name") == 0) {
            LPWSTR pDesc = nullptr;
            if (SUCCEEDED(SpGetDescription(token, &pDesc)) && pDesc) {
                std::wstring res(pDesc);
                CoTaskMemFree(pDesc);
                return res;
            }
        }
        ISpDataKey* pAttributesKey = nullptr;
        HRESULT hr = token->OpenKey(L"Attributes", &pAttributesKey);
        if (SUCCEEDED(hr) && pAttributesKey) {
            LPWSTR pValue = nullptr;
            hr = pAttributesKey->GetStringValue(key, &pValue);
            if (SUCCEEDED(hr) && pValue) {
                std::wstring res(pValue);
                CoTaskMemFree(pValue);
                pAttributesKey->Release();
                return res;
            }
            pAttributesKey->Release();
        }
        return L"";
    }

    int scoreVoiceToken(ISpObjectToken* token) {
        if (!token) return -999999;
        std::wstring name = getTokenStringAttribute(token, L"Name");
        std::wstring gender = getTokenStringAttribute(token, L"Gender");
        std::wstring age = getTokenStringAttribute(token, L"Age");
        std::wstring language = getTokenStringAttribute(token, L"Language");
        std::wstring vendor = getTokenStringAttribute(token, L"Vendor");

        auto containsI = [](const std::wstring& s, const std::wstring& part) {
            std::wstring a = s, b = part;
            std::transform(a.begin(), a.end(), a.begin(), ::towlower);
            std::transform(b.begin(), b.end(), b.begin(), ::towlower);
            return a.find(b) != std::wstring::npos;
        };

        int score = 0;
        if (containsI(gender, L"female")) score += 500;
        if (containsI(age, L"adult")) score += 120;
        if (containsI(language, L"409")) score += 220;
        if (containsI(vendor, L"microsoft")) score += 80;

        if (containsI(name, L"zira")) score += 250;
        if (containsI(name, L"aria")) score += 220;
        if (containsI(name, L"jenny")) score += 320;
        if (containsI(name, L"susan")) score += 160;
        if (containsI(name, L"hazel")) score += 120;
        if (containsI(name, L"eva")) score += 140;
        if (containsI(name, L"david")) score -= 500;
        if (containsI(name, L"male")) score -= 400;

        return score;
    }

    std::string narrowUtf8(const std::wstring& ws) const {
        if (ws.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return "";
        std::string res(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &res[0], size, nullptr, nullptr);
        return res;
    }

    bool selectBestVoice() {
        if (!pSpVoice_) return false;

        struct Candidate {
            ISpObjectToken* token = nullptr;
            int score = -999999;
            std::wstring name;
        };

        std::vector<Candidate> candidates;

        auto collect = [&](const wchar_t* required, const wchar_t* optionalBoost) {
            IEnumSpObjectTokens* pEnum = nullptr;
            HRESULT hr = SpEnumTokens(SPCAT_VOICES, required, optionalBoost, &pEnum);
            if (FAILED(hr) || !pEnum) return;

            while (true) {
                ISpObjectToken* token = nullptr;
                ULONG fetched = 0;
                hr = pEnum->Next(1, &token, &fetched);
                if (hr != S_OK || !token || fetched == 0) break;

                std::wstring tName = getTokenStringAttribute(token, L"Name");
                bool duplicate = false;
                for (const auto& c : candidates) {
                    if (c.name == tName) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    token->Release();
                    continue;
                }

                Candidate c;
                c.token = token;
                c.name = tName;
                c.score = scoreVoiceToken(token);
                candidates.push_back(c);
            }
            pEnum->Release();
        };

        collect(L"Gender=Female;Age=Adult;Language=409", nullptr);
        collect(L"Gender=Female;Language=409", nullptr);
        collect(L"Gender=Female;Age=Adult", nullptr);
        collect(L"Gender=Female", nullptr);
        collect(nullptr, nullptr);

        if (candidates.empty()) {
            selectedVoiceName_ = "Default System Voice";
            usingFemaleVoice_ = false;
            return true;
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

        Candidate best = candidates.front();
        HRESULT hr = pSpVoice_->SetVoice(best.token);
        bool success = SUCCEEDED(hr);

        if (success) {
            selectedVoiceName_ = narrowUtf8(best.name);
            usingFemaleVoice_ = (scoreVoiceToken(best.token) >= 500);
        } else {
            selectedVoiceName_ = "Default System Voice";
            usingFemaleVoice_ = false;
        }

        for (auto& c : candidates) {
            if (c.token) {
                c.token->Release();
            }
        }
        return success;
    }

private:
    ISpVoice* pSpVoice_;
    std::string selectedVoiceName_;
    bool usingFemaleVoice_;
};

// -------------------------------------------------
// EdgeTTSBackend — Microsoft Neural Voices via Python
// Uses en-US-JennyNeural — far more natural than SAPI
// -------------------------------------------------
class EdgeTTSBackend : public ITtsBackend {
public:
    EdgeTTSBackend() : hWriteStdin_(INVALID_HANDLE_VALUE), hReadStdout_(INVALID_HANDLE_VALUE), hDaemonProc_(INVALID_HANDLE_VALUE) {}
    ~EdgeTTSBackend() { shutdown(); }

    void setTtsPaths(const TtsPaths& paths) {
        paths_ = paths;
    }

    bool probe() override {
        DWORD a = GetFileAttributesA(paths_.edgeTtsSpeakPy.c_str());
        if (a != INVALID_FILE_ATTRIBUTES) return true;
        a = GetFileAttributesA(paths_.edgeTtsDaemonPy.c_str());
        return (a != INVALID_FILE_ATTRIBUTES);
    }

    bool initialize() override { return probe(); }

    void shutdown() override {
        std::lock_guard<std::mutex> lock(ttsMutex_);
        if (hWriteStdin_ != INVALID_HANDLE_VALUE) {
            const char q[] = "{\"cmd\":\"quit\"}\n";
            DWORD w = 0;
            WriteFile(hWriteStdin_, q, (DWORD)strlen(q), &w, nullptr);
            CloseHandle(hWriteStdin_);
            hWriteStdin_ = INVALID_HANDLE_VALUE;
        }
        if (hDaemonProc_ != INVALID_HANDLE_VALUE) {
            WaitForSingleObject(hDaemonProc_, 2000);
            TerminateProcess(hDaemonProc_, 0);
            CloseHandle(hDaemonProc_);
            hDaemonProc_ = INVALID_HANDLE_VALUE;
        }
        if (hReadStdout_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hReadStdout_);
            hReadStdout_ = INVALID_HANDLE_VALUE;
        }
    }

    bool ensureDaemonRunning() {
        std::lock_guard<std::mutex> lock(ttsMutex_);
        if (hDaemonProc_ != INVALID_HANDLE_VALUE && WaitForSingleObject(hDaemonProc_, 0) == WAIT_TIMEOUT) {
            return true;
        }
        
        if (hWriteStdin_ != INVALID_HANDLE_VALUE) { CloseHandle(hWriteStdin_); hWriteStdin_ = INVALID_HANDLE_VALUE; }
        if (hReadStdout_ != INVALID_HANDLE_VALUE) { CloseHandle(hReadStdout_); hReadStdout_ = INVALID_HANDLE_VALUE; }
        if (hDaemonProc_ != INVALID_HANDLE_VALUE) { CloseHandle(hDaemonProc_); hDaemonProc_ = INVALID_HANDLE_VALUE; }

        std::string scriptPath = paths_.edgeTtsSpeakPy;
        DWORD a = GetFileAttributesA(scriptPath.c_str());
        if (a == INVALID_FILE_ATTRIBUTES) {
            scriptPath = paths_.edgeTtsDaemonPy;
            a = GetFileAttributesA(scriptPath.c_str());
            if (a == INVALID_FILE_ATTRIBUTES) return false;
        }

        SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
        HANDLE hChildReadStdout = INVALID_HANDLE_VALUE, hChildWriteStdout = INVALID_HANDLE_VALUE;
        HANDLE hChildReadStdin = INVALID_HANDLE_VALUE, hChildWriteStdin = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&hChildReadStdout, &hChildWriteStdout, &sa, 0)) return false;
        if (!SetHandleInformation(hChildReadStdout, HANDLE_FLAG_INHERIT, 0)) return false;
        if (!CreatePipe(&hChildReadStdin, &hChildWriteStdin, &sa, 0)) {
            CloseHandle(hChildReadStdout); CloseHandle(hChildWriteStdout); return false;
        }
        if (!SetHandleInformation(hChildWriteStdin, HANDLE_FLAG_INHERIT, 0)) return false;

        STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; si.hStdInput = hChildReadStdin; si.hStdOutput = hChildWriteStdout;
        si.hStdError = hChildWriteStdout;

        char cmd[512];
        std::snprintf(cmd, sizeof(cmd), "python -u \"%s\" --daemon", scriptPath.c_str());
        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(hChildWriteStdout); CloseHandle(hChildReadStdin);

        if (!ok) {
            CloseHandle(hChildReadStdout); CloseHandle(hChildWriteStdin);
            return false;
        }

        hWriteStdin_ = hChildWriteStdin;
        hReadStdout_ = hChildReadStdout;
        hDaemonProc_ = pi.hProcess;
        CloseHandle(pi.hThread);
        return true;
    }

    bool readTtsResponseLine(std::string& lineOut, DWORD timeoutMs) {
        lineOut.clear();
        const DWORD start = GetTickCount();
        char ch = 0;
        DWORD bytesRead = 0;

        while (GetTickCount() - start < timeoutMs) {
            DWORD avail = 0;
            if (!PeekNamedPipe(hReadStdout_, nullptr, 0, nullptr, &avail, nullptr)) {
                return false;
            }
            if (avail == 0) {
                Sleep(5);
                continue;
            }
            BOOL ok = ReadFile(hReadStdout_, &ch, 1, &bytesRead, nullptr);
            if (!ok || bytesRead == 0) {
                return false;
            }
            if (ch == '\n') {
                if (!lineOut.empty() && lineOut.back() == '\r') {
                    lineOut.pop_back();
                }
                return true;
            }
            lineOut.push_back(ch);
        }
        return false;
    }

    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) override {
        if (!ensureDaemonRunning()) {
            err = "Failed to start TTS daemon";
            return false;
        }

        std::string escapedText;
        for (char c : text) {
            if (c == '"') escapedText += "\\\"";
            else if (c == '\\') escapedText += "\\\\";
            else if (c == '\n') escapedText += "\\n";
            else if (c == '\r') escapedText += "\\r";
            else escapedText += c;
        }
        std::string escapedWav;
        for (char c : wavPath) {
            if (c == '\\') escapedWav += "\\\\";
            else if (c == '"') escapedWav += "\\\"";
            else escapedWav += c;
        }

        std::string payload = "{\"text\":\"" + escapedText + "\",\"wav_path\":\"" + escapedWav + "\"}\n";
        DWORD written = 0;
        if (!WriteFile(hWriteStdin_, payload.data(), (DWORD)payload.size(), &written, nullptr) || written != payload.size()) {
            shutdown();
            err = "Failed to write TTS command pipe";
            return false;
        }

        std::string responseLine;
        if (!readTtsResponseLine(responseLine, 15000)) {
            shutdown();
            err = "Timed out waiting for TTS response";
            return false;
        }

        if (responseLine.find("\"ok\": true") != std::string::npos || responseLine.find("\"ok\":true") != std::string::npos) {
            std::ifstream f(wavPath, std::ios::binary | std::ios::ate);
            if (f.is_open() && f.tellg() > 44) return true;
            err = "TTS daemon returned OK but output file missing or small";
            return false;
        }

        err = "TTS daemon error: " + responseLine;
        return false;
    }

    std::string backendName() const override { return "EdgeTTS (Daemon)"; }
    std::string voiceName()   const override { return "en-US-JennyNeural"; }

    VoiceBackendType backendType() const override {
        return VoiceBackendType::EDGE_TTS;
    }

    bool isNeural() const override {
        return true;
    }

private:
    TtsPaths paths_;
    HANDLE hWriteStdin_ = INVALID_HANDLE_VALUE;
    HANDLE hReadStdout_ = INVALID_HANDLE_VALUE;
    HANDLE hDaemonProc_ = INVALID_HANDLE_VALUE;
    std::mutex ttsMutex_;
};

// -------------------------------------------------
// MouthRuntime Implementation
// -------------------------------------------------

MouthRuntime::MouthRuntime(SubsystemControl& control)
    : control_(control), requestSerial_(0), activeSerial_(0) {
    kokoroBackend_ = new KokoroBackend();
    piperBackend_  = new PiperBackend();
    sapiBackend_   = new SapiBackend();
    edgeTtsBackend_= new EdgeTTSBackend();

    // Load TTS paths from config (with defaults as fallback)
    ttsPaths_ = TtsPaths::loadFromIni("data\\config\\tts_paths.ini");

    // Load fallback chain config
    fallbackConfig_ = FallbackConfig::loadFromIni("data\\config\\tts_fallback.ini");

    // Apply paths to backends
    kokoroBackend_->setTtsPaths(ttsPaths_);
    piperBackend_->setTtsPaths(ttsPaths_);
    edgeTtsBackend_->setTtsPaths(ttsPaths_);

    std::lock_guard<std::mutex> lock(mutex_);
    backendInfo_.type = VoiceBackendType::NONE;
    backendInfo_.backendName = "Uninitialized";
    backendInfo_.voiceName = "None";
    backendInfo_.available = false;
    backendInfo_.neural = false;
    backendInfo_.detail = "Awaiting engine prewarm.";
}

// TtsPaths::loadFromIni implementation
TtsPaths TtsPaths::loadFromIni(const std::string& iniPath) {
    TtsPaths paths;
    
    // Try to load from INI file
    std::ifstream file(iniPath);
    if (!file.is_open()) {
        // Config file doesn't exist, return defaults
        return paths;
    }

    std::string line;
    std::string currentSection;
    while (std::getline(file, line)) {
        // Trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) { s.clear(); return; }
            s = s.substr(start, end - start + 1);
        };
        trim(line);
        
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }
        
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        trim(key);
        trim(value);
        
        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        if (currentSection == "Kokoro") {
            if (key == "runner_bat") paths.kokoroRunnerBat = value;
            else if (key == "exe") paths.kokoroExe = value;
            else if (key == "input_txt") paths.kokoroInputTxt = value;
        }
        else if (currentSection == "Piper") {
            if (key == "exe_paths") {
                // Comma-separated list
                std::stringstream ss(value);
                std::string item;
                paths.piperExePaths.clear();
                while (std::getline(ss, item, ',')) {
                    trim(item);
                    if (!item.empty()) paths.piperExePaths.push_back(item);
                }
            }
            else if (key == "input_txt") paths.piperInputTxt = value;
        }
        else if (currentSection == "PiperModels") {
            // Each line is a model: name=path
            paths.piperModels.emplace_back(key, value);
        }
        else if (currentSection == "EdgeTTS") {
            if (key == "speak_py") paths.edgeTtsSpeakPy = value;
            else if (key == "daemon_py") paths.edgeTtsDaemonPy = value;
        }
        else if (currentSection == "Common") {
            if (key == "tts_output_dir") paths.ttsOutputDir = value;
        }
    }
    
    return paths;
}

// FallbackConfig::loadFromIni implementation
FallbackConfig FallbackConfig::loadFromIni(const std::string& iniPath) {
    FallbackConfig config;

    // Try to load from INI file
    std::ifstream file(iniPath);
    if (!file.is_open()) {
        // Config file doesn't exist, return defaults
        return config;
    }

    std::string line;
    std::string currentSection;
    while (std::getline(file, line)) {
        // Trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) { s.clear(); return; }
            s = s.substr(start, end - start + 1);
        };
        trim(line);

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        trim(key);
        trim(value);

        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (currentSection == "Fallback") {
            if (key == "order") {
                // Comma-separated list of backend names
                std::stringstream ss(value);
                std::string item;
                config.fallbackOrder.clear();
                while (std::getline(ss, item, ',')) {
                    trim(item);
                    if (item.empty()) continue;
                    
                    // Map backend name string to enum
                    VoiceBackendType type = VoiceBackendType::NONE;
                    std::string lower = item;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "edge_tts" || lower == "edgetts") type = VoiceBackendType::EDGE_TTS;
                    else if (lower == "kokoro") type = VoiceBackendType::KOKORO;
                    else if (lower == "piper") type = VoiceBackendType::PIPER;
                    else if (lower == "sapi") type = VoiceBackendType::SAPI;
                    
                    if (type != VoiceBackendType::NONE) {
                        config.fallbackOrder.push_back(type);
                    }
                }
            }
        }
    }

    return config;
}

MouthRuntime::~MouthRuntime() {
    stop();
    delete kokoroBackend_;
    delete piperBackend_;
    delete sapiBackend_;
    delete edgeTtsBackend_;
}

bool MouthRuntime::probeOutputDevice(std::string& outName) {
    UINT numDevs = waveOutGetNumDevs();
    if (numDevs == 0) {
        outName = "No audio output hardware.";
        return false;
    }

    WAVEOUTCAPSA caps = {};
    if (waveOutGetDevCapsA(0, &caps, sizeof(WAVEOUTCAPSA)) == MMSYSERR_NOERROR) {
        outName = caps.szPname;
    } else {
        outName = "Default Audio Output";
    }
    return true;
}

void MouthRuntime::start() {
    if (workerRunning_) return;

    if (!probeOutputDevice(deviceName_)) {
        state_ = SubsystemRuntimeState::UNAVAILABLE;
        lastError_ = "No hardware";
        return;
    }

    stopRequested_ = false;
    workerRunning_ = true;
    state_ = SubsystemRuntimeState::STARTING;

    worker_ = std::thread(&MouthRuntime::workerLoop, this);
}

void MouthRuntime::stop() {
    if (!workerRunning_) return;

    stopRequested_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::pair<uint64_t, std::string>> empty;
        std::swap(speakQueue_, empty);
        activeSerial_ = 0;
    }
    cv_.notify_one();

    PlaySoundA(NULL, NULL, 0);

    if (worker_.joinable()) {
        worker_.join();
    }

    workerRunning_ = false;
    state_ = SubsystemRuntimeState::STOPPED;
    setPhase(SpeakPhase::IDLE, "");
}

SubsystemRuntimeState MouthRuntime::reportState() const {
    return state_;
}

SpeakPhase MouthRuntime::getSpeakPhase() const {
    return phase_;
}

bool MouthRuntime::isRunning() const {
    return state_ == SubsystemRuntimeState::RUNNING;
}

bool MouthRuntime::isSpeaking() const {
    return phase_ == SpeakPhase::SPEAKING;
}

std::string MouthRuntime::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

std::string MouthRuntime::getDeviceName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deviceName_;
}

std::string MouthRuntime::getBackendName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backendInfo_.backendName;
}

std::string MouthRuntime::getVoiceName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backendInfo_.voiceName;
}

bool MouthRuntime::isNeuralVoiceActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backendInfo_.neural;
}

const VoiceSelectionInfo MouthRuntime::getVoiceSelectionInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    VoiceSelectionInfo info;
    info.backend = backendInfo_.type;
    info.backendName = backendInfo_.backendName;
    info.voiceName = backendInfo_.voiceName;
    info.femalePreferred = (backendInfo_.type == VoiceBackendType::SAPI) ? sapiBackend_->isFemalePreferred() : true;
    info.fallbackActive = (backendInfo_.type == VoiceBackendType::SAPI);
    info.reason = backendInfo_.detail;
    return info;
}

void MouthRuntime::setPhaseCallback(PhaseCallback cb) {
    phaseCallback_ = cb;
}

void MouthRuntime::setPhase(SpeakPhase phase, const std::string& text) {
    phase_ = phase;
    if (phaseCallback_) {
        phaseCallback_(phase, text);
    }
}

SpeakResult MouthRuntime::speak(const std::string& text) {
    SpeakResult res;

    if (text.empty()) {
        res.reason = "Empty text";
        return res;
    }

    if (!workerRunning_ || state_ != SubsystemRuntimeState::RUNNING) {
        res.reason = "Runtime not running or failed";
        return res;
    }

    if (!control_.isActive(SubsystemName::MOUTH)) {
        res.reason = "Subsystem not active";
        setPhase(SpeakPhase::BLOCKED, text);
        return res;
    }

    const uint64_t serial = ++requestSerial_;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeSerial_ = serial;

        // Flush queue to force real-time preemption
        std::queue<std::pair<uint64_t, std::string>> empty;
        std::swap(speakQueue_, empty);

        speakQueue_.push({serial, text});
    }

    res.accepted = true;
    res.reason = "Queued latest speech request";
    setPhase(SpeakPhase::QUEUED, text);
    cv_.notify_one();

    return res;
}

bool MouthRuntime::initializeBestBackend() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 0. EdgeTTS — Microsoft Neural Voice (highest priority, requires internet + Python)
    if (edgeTtsBackend_->initialize()) {
        backendInfo_.type = VoiceBackendType::EDGE_TTS;
        backendInfo_.backendName = edgeTtsBackend_->backendName();
        backendInfo_.voiceName   = edgeTtsBackend_->voiceName();
        backendInfo_.available   = true;
        backendInfo_.neural      = true;
        backendInfo_.detail      = "EdgeTTS en-US-JennyNeural neural voice active.";
        std::cout << "[Mouth] EdgeTTS neural backend online (JennyNeural).\n";
        return true;
    }

    // 1. Try Kokoro
    if (kokoroBackend_->initialize()) {
        backendInfo_.type = VoiceBackendType::KOKORO;
        backendInfo_.backendName = kokoroBackend_->backendName();
        backendInfo_.voiceName = kokoroBackend_->voiceName();
        backendInfo_.available = true;
        backendInfo_.neural = true;
        backendInfo_.detail = "Kokoro local runner initialized successfully.";
        return true;
    }

    // 2. Try Piper
    if (piperBackend_->initialize()) {
        backendInfo_.type = VoiceBackendType::PIPER;
        backendInfo_.backendName = piperBackend_->backendName();
        backendInfo_.voiceName = piperBackend_->voiceName();
        backendInfo_.available = true;
        backendInfo_.neural = true;
        backendInfo_.detail = "Piper local ONNX neural voice initialized successfully.";
        return true;
    }

    // 3. Try SAPI Fallback
    if (sapiBackend_->initialize()) {
        backendInfo_.type = VoiceBackendType::SAPI;
        backendInfo_.backendName = sapiBackend_->backendName();
        backendInfo_.voiceName = sapiBackend_->voiceName();
        backendInfo_.available = true;
        backendInfo_.neural = false;
        backendInfo_.detail = "Neural backends offline. Fallback to SAPI voice: " + sapiBackend_->voiceName();
        return true;
    }

    backendInfo_.type = VoiceBackendType::NONE;
    backendInfo_.backendName = "None";
    backendInfo_.voiceName = "None";
    backendInfo_.available = false;
    backendInfo_.neural = false;
    backendInfo_.detail = "All text-to-speech backends failed to initialize.";
    return false;
}

void MouthRuntime::shutdownBackend() {
    kokoroBackend_->shutdown();
    piperBackend_->shutdown();
    sapiBackend_->shutdown();
}

SpeechPlan MouthRuntime::buildSpeechPlan(const std::string& text) const {
    SpeechPlan plan;
    plan.originalText = text;
    plan.normalizedText = normalizeSpeechText(text);

    const size_t len = plan.normalizedText.size();
    plan.isShortReply = len < 120;

    std::string lower = plan.normalizedText;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    plan.isStatus = lower.find("status") != std::string::npos
                 || lower.find("enabled") != std::string::npos
                 || lower.find("disabled") != std::string::npos
                 || lower.find("runtime") != std::string::npos;

    plan.isCommandAck = lower.find("done") != std::string::npos
                     || lower.find("enabled") != std::string::npos
                     || lower.find("disabled") != std::string::npos
                     || lower.find("started") != std::string::npos
                     || lower.find("stopped") != std::string::npos;

    plan.isLongForm = len >= 220;

    if (plan.isShortReply || plan.isCommandAck) {
        plan.chunks.push_back(plan.normalizedText);
        return plan;
    }

    plan.chunks = splitIntoClauses(plan.normalizedText);
    return plan;
}

std::string MouthRuntime::normalizeSpeechText(const std::string& text) const {
    if (text.empty()) return "";

    std::string clean = text;

    size_t pos = 0;
    while ((pos = clean.find("Yuki_1.0", pos)) != std::string::npos) {
        clean.replace(pos, 8, "Yuki one point zero");
        pos += 19;
    }
    pos = 0;
    while ((pos = clean.find("Yuki 1.0", pos)) != std::string::npos) {
        clean.replace(pos, 8, "Yuki one point zero");
        pos += 19;
    }

    auto replaceWord = [&](const std::string& target, const std::string& replacement) {
        size_t index = 0;
        while ((index = clean.find(target, index)) != std::string::npos) {
            bool leftBound = (index == 0 || !std::isalnum(static_cast<unsigned char>(clean[index - 1])));
            bool rightBound = (index + target.size() == clean.size() || !std::isalnum(static_cast<unsigned char>(clean[index + target.size()])));
            if (leftBound && rightBound) {
                clean.replace(index, target.size(), replacement);
                index += replacement.size();
            } else {
                index += target.size();
            }
        }
    };

    replaceWord("STT", "S T T");
    replaceWord("TTS", "T T S");
    replaceWord("COM", "C O M");
    replaceWord("SAPI", "Sappy");
    replaceWord("GDI", "G D I");
    replaceWord("BitBlt", "Bit Blit");
    replaceWord("RMS", "R M S");
    replaceWord("VAD", "V A D");
    replaceWord("Hz", "Hertz");
    replaceWord("ms", "milliseconds");
    replaceWord("px", "pixels");

    std::string result;
    for (char c : clean) {
        if (c == '_') result += ' ';
        else if (c == '&') result += " and ";
        else if (c == '%') result += " percent ";
        else if (c == '+') result += " plus ";
        else result += c;
    }
    
    return result;
}

std::vector<std::string> MouthRuntime::splitIntoClauses(const std::string& text) const {
    std::vector<std::string> rawClauses;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        current += c;
        if (c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' || c == '\n') {
            if (i + 1 == text.size() || std::isspace(static_cast<unsigned char>(text[i + 1]))) {
                while (!current.empty() && std::isspace(static_cast<unsigned char>(current.front()))) {
                    current.erase(0, 1);
                }
                while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back()))) {
                    current.pop_back();
                }
                if (!current.empty()) {
                    rawClauses.push_back(current);
                }
                current.clear();
            }
        }
    }
    while (!current.empty() && std::isspace(static_cast<unsigned char>(current.front()))) {
        current.erase(0, 1);
    }
    while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back()))) {
        current.pop_back();
    }
    if (!current.empty()) {
        rawClauses.push_back(current);
    }

    std::vector<std::string> finalClauses;
    for (const auto& clause : rawClauses) {
        if (finalClauses.empty()) {
            finalClauses.push_back(clause);
        } else {
            if (clause.size() < 12 || finalClauses.back().size() < 15) {
                finalClauses.back() += " " + clause;
            } else {
                finalClauses.push_back(clause);
            }
        }
    }
    return finalClauses;
}

bool MouthRuntime::executeSpeechPlan(const SpeechPlan& plan, uint64_t serial) {
    if (!requestStillCurrent(serial)) return false;

    if (plan.isShortReply || plan.isCommandAck) {
        return executeOneShotPlan(plan, serial);
    }
    return executeProgressivePlan(plan, serial);
}

// Unified synthesis fallback handler - uses configurable fallback chain from tts_fallback.ini
bool MouthRuntime::trySynthesizeAndPlay(const std::string& text, const std::string& wavPath, uint64_t serial, std::string& errOut) {
    errOut.clear();

    // Map backend type to backend pointer
    auto getBackend = [this](VoiceBackendType type) -> ITtsBackend* {
        switch (type) {
            case VoiceBackendType::EDGE_TTS: return edgeTtsBackend_;
            case VoiceBackendType::KOKORO:   return kokoroBackend_;
            case VoiceBackendType::PIPER:    return piperBackend_;
            case VoiceBackendType::SAPI:     return sapiBackend_;
            default: return nullptr;
        }
    };

    // Try each backend in the configured fallback order
    for (VoiceBackendType type : fallbackConfig_.fallbackOrder) {
        ITtsBackend* backend = getBackend(type);
        if (!backend) continue;

        // Ensure backend is probed/initialized
        if (!backend->probe()) {
            std::cerr << "[Mouth] " << backend->backendName() << " not available, trying next.\n";
            continue;
        }
        if (!backend->initialize()) {
            std::cerr << "[Mouth] " << backend->backendName() << " initialization failed, trying next.\n";
            continue;
        }

        // For SAPI, use speakDirect (doesn't support file output)
        if (type == VoiceBackendType::SAPI) {
            if (sapiBackend_->initialize()) {
                return sapiBackend_->speakDirect(text, serial,
                    [this](uint64_t s) { return this->requestStillCurrent(s); },
                    [this](SpeakPhase p, const std::string& t) { this->setPhase(p, t); });
            }
            continue;
        }

        // Try file-based synthesis for neural backends
        if (backend->synthesizeToFile(text, wavPath, errOut)) {
            return playWaveTruthfully(wavPath, serial);
        }

        std::cerr << "[Mouth] " << backend->backendName() << " synthesis failed: " << errOut << ". Trying next.\n";
    }

    // All backends exhausted
    errOut = "All TTS backends in fallback chain failed";
    return false;
}

bool MouthRuntime::executeOneShotPlan(const SpeechPlan& plan, uint64_t serial) {
    if (!requestStillCurrent(serial)) return false;

    std::string tempWav = "data\\tts\\temp_oneshot.wav";
    std::string err;
    return trySynthesizeAndPlay(plan.normalizedText, tempWav, serial, err);
}

bool MouthRuntime::executeProgressivePlan(const SpeechPlan& plan, uint64_t serial) {
    for (size_t i = 0; i < plan.chunks.size(); ++i) {
        if (!requestStillCurrent(serial)) return false;

        const std::string& chunk = plan.chunks[i];
        std::string tempWav = "data\\tts\\temp_chunk_" + std::to_string(i) + ".wav";
        std::string err;

        if (!trySynthesizeAndPlay(chunk, tempWav, serial, err)) {
            return false;
        }
    }
    return true;
}

uint32_t MouthRuntime::readWaveDurationMs(const std::string& wavPath) const {
    std::ifstream file(wavPath, std::ios::binary);
    if (!file.is_open()) return 0;

    char header[44];
    file.read(header, 44);
    if (file.gcount() < 44) return 0;

    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        return 0;
    }

    uint32_t byteRate = 0;
    memcpy(&byteRate, header + 28, 4);

    uint32_t subChunk2Size = 0;
    memcpy(&subChunk2Size, header + 40, 4);

    if (memcmp(header + 36, "data", 4) != 0) {
        file.seekg(12);
        char chunkId[4];
        uint32_t chunkSize = 0;
        while (file.read(chunkId, 4)) {
            file.read(reinterpret_cast<char*>(&chunkSize), 4);
            if (file.gcount() < 4) break;
            if (memcmp(chunkId, "fmt ", 4) == 0) {
                char fmtData[16];
                file.read(fmtData, 16);
                memcpy(&byteRate, fmtData + 8, 4);
                if (chunkSize > 16) {
                    file.seekg(chunkSize - 16, std::ios::cur);
                }
            } else if (memcmp(chunkId, "data", 4) == 0) {
                subChunk2Size = chunkSize;
                break;
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
            if (file.tellg() > 2048) break;
        }
    }

    if (byteRate == 0 || subChunk2Size == 0) return 0;

    double durationSec = static_cast<double>(subChunk2Size) / static_cast<double>(byteRate);
    return static_cast<uint32_t>(durationSec * 1000.0);
}

bool MouthRuntime::playWaveTruthfully(const std::string& wavPath, uint64_t serial) {
    const uint32_t durationMs = readWaveDurationMs(wavPath);
    if (durationMs == 0) return false;

    setPhase(SpeakPhase::PLAYBACK_STARTING, wavPath);

    if (!PlaySoundA(wavPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
        return false;
    }

    setPhase(SpeakPhase::SPEAKING, wavPath);

    uint32_t waited = 0;
    const uint32_t slice = 20; // 20ms preemption query rate
    while (waited < durationMs) {
        if (!requestStillCurrent(serial)) {
            PlaySoundA(NULL, NULL, 0); // Stop player instantly
            setPhase(SpeakPhase::INTERRUPTED, wavPath);
            return false;
        }
        Sleep(slice);
        waited += slice;
    }

    PlaySoundA(NULL, NULL, 0);
    return requestStillCurrent(serial);
}

void MouthRuntime::workerLoop() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        lastError_ = "COM init failed on worker thread";
        state_ = SubsystemRuntimeState::FAILED;
        return;
    }

    initializeBestBackend();

    state_ = SubsystemRuntimeState::RUNNING;
    lastError_.clear();
    setPhase(SpeakPhase::IDLE, "");

    while (!stopRequested_) {
        uint64_t serial = 0;
        std::string currentText;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stopRequested_ || !speakQueue_.empty(); });
            
            if (stopRequested_) break;

            serial = speakQueue_.front().first;
            currentText = speakQueue_.front().second;
            speakQueue_.pop();
        }

        if (!requestStillCurrent(serial)) {
            continue;
        }

        if (!control_.isActive(SubsystemName::MOUTH)) {
            setPhase(SpeakPhase::BLOCKED, currentText);
            setPhase(SpeakPhase::IDLE, "");
            continue;
        }

        setPhase(SpeakPhase::STARTING, currentText);

        SpeechPlan plan = buildSpeechPlan(currentText);

        bool success = executeSpeechPlan(plan, serial);

        if (success && requestStillCurrent(serial)) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (speakQueue_.empty()) {
                setPhase(SpeakPhase::COMPLETED, currentText);
                setPhase(SpeakPhase::IDLE, "");
            }
        }
    }

    shutdownBackend();
    CoUninitialize();
}

// -------------------------------------------------
// MouthReader Implementation
// -------------------------------------------------

MouthSnapshot MouthReader::capture(const SubsystemControl& control, const MouthRuntime& runtime) const {
    MouthSnapshot snap;

    SubsystemStatus status = control.getStatus(SubsystemName::MOUTH);
    snap.allowed = status.active;
    snap.subsystem_available = status.available;
    snap.subsystem_active = status.active;

    snap.subsystemAvailable = status.available;
    snap.subsystemActive = status.active;

    snap.text_output_ready = true;
    snap.textOutputReady = true;

    UINT numDevs = waveOutGetNumDevs();
    if (numDevs > 0) {
        snap.voice_output_ready = true;
        snap.voiceOutputReady = true;
    }

    snap.runtime_running = runtime.isRunning();
    snap.runtimeRunning = runtime.isRunning();

    snap.speak_phase = runtime.getSpeakPhase();
    snap.speakPhase = runtime.getSpeakPhase();

    snap.actively_speaking = runtime.isSpeaking();
    snap.activelySpeaking = runtime.isSpeaking();

    snap.device_name = runtime.getDeviceName();
    snap.deviceName = runtime.getDeviceName();

    snap.last_error = runtime.getLastError();
    snap.lastError = runtime.getLastError();

    snap.sapi_ready = (snap.runtime_running && snap.last_error.empty());
    snap.sapiReady = (snap.runtime_running && snap.last_error.empty());
    
    snap.output_pipeline_active = (status.runtimeState == SubsystemRuntimeState::RUNNING);
    snap.outputPipelineActive = (status.runtimeState == SubsystemRuntimeState::RUNNING);

    snap.backendName = runtime.getBackendName();
    snap.backend_name = runtime.getBackendName();

    snap.voiceName = runtime.getVoiceName();
    snap.voice_name = runtime.getVoiceName();

    snap.neuralVoiceActive = runtime.isNeuralVoiceActive();
    snap.neural_voice_active = runtime.isNeuralVoiceActive();

    // Advanced Quality Voice & Engine Metadata
    const VoiceSelectionInfo info = runtime.getVoiceSelectionInfo();
    snap.selected_backend = info.backendName;
    snap.selected_voice_name = info.voiceName;
    snap.using_female_voice = info.femalePreferred;
    snap.fallback_active = info.fallbackActive;
    snap.selection_reason = info.reason;

    if (!snap.subsystem_active) {
        snap.summary = "Mouth blocked or unavailable.";
        return snap;
    }

    std::ostringstream ss;
    ss << "Mouth ready: ";
    if (snap.text_output_ready && snap.voice_output_ready) {
        ss << "text and voice output available (Backend: " << snap.backend_name 
           << ", Voice: " << snap.voice_name 
           << ", Neural Voice Active: " << (snap.neural_voice_active ? "YES" : "NO")
           << "). Runtime: " << (snap.runtime_running ? "RUNNING" : "STOPPED");
    } else if (snap.text_output_ready) {
        ss << "text output ready, voice hardware missing.";
    } else {
        ss << "no output channels ready.";
    }
    
    if (snap.actively_speaking) {
        ss << " [Currently speaking]";
    }

    snap.summary = ss.str();
    return snap;
}
