// Mouth.cpp
#include "input/Mouth.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <mmsystem.h>
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
class KokoroBackend {
public:
    KokoroBackend() : isBat_(false) {}

    bool probe() {
        // Probe for kokoro_runner.bat or kokoro.exe in the workspace layout
        DWORD dwAttrib = GetFileAttributesA("data\\kokoro\\kokoro_runner.bat");
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            runnerPath_ = "data\\kokoro\\kokoro_runner.bat";
            isBat_ = true;
            return true;
        }
        dwAttrib = GetFileAttributesA("data\\kokoro\\kokoro.exe");
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            runnerPath_ = "data\\kokoro\\kokoro.exe";
            isBat_ = false;
            return true;
        }
        return false;
    }

    bool initialize() {
        return probe();
    }

    void shutdown() {}

    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) {
        if (runnerPath_.empty()) {
            err = "Kokoro runner not found.";
            return false;
        }

        // Create data/tts directory if it doesn't exist
        CreateDirectoryA("data", NULL);
        CreateDirectoryA("data\\tts", NULL);

        // Write text to a temporary file to avoid shell command quoting limitations
        std::string tempTextPath = "data\\tts\\kokoro_input.txt";
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

    std::string backendName() const {
        return "Kokoro";
    }

    std::string voiceName() const {
        return "Default Assistant English Voice";
    }

private:
    std::string runnerPath_;
    bool isBat_;
};

// -------------------------------------------------
// PiperBackend Definition & Implementation
// -------------------------------------------------
class PiperBackend {
public:
    bool probe() {
        // Detect piper.exe in common locations
        const std::vector<std::string> exePaths = {
            "third_party\\piper\\piper.exe",
            "tools\\piper\\piper.exe",
            "bin\\piper\\piper.exe",
            "data\\tts\\piper.exe"
        };

        std::string foundExe;
        for (const auto& path : exePaths) {
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
        const std::vector<std::pair<std::string, std::string>> models = {
            { "en_US-lessac-medium", "data\\voices\\piper\\en_US-lessac-medium.onnx" },
            { "en_US-amy-medium", "data\\voices\\piper\\en_US-amy-medium.onnx" },
            { "en_GB-alan-medium", "data\\voices\\piper\\en_GB-alan-medium.onnx" },
            { "en_US-lessac-medium (legacy)", "data\\tts\\en_US-lessac-medium.onnx" }
        };

        std::string foundModel;
        std::string foundModelName;
        for (const auto& m : models) {
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

    bool initialize() {
        return probe();
    }

    void shutdown() {}

    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) {
        if (exePath_.empty() || modelPath_.empty()) {
            err = "Piper backend is not fully initialized.";
            return false;
        }

        CreateDirectoryA("data", NULL);
        CreateDirectoryA("data\\tts", NULL);

        std::string tempTextPath = "data\\tts\\piper_input.txt";
        std::ofstream out(tempTextPath);
        if (!out.is_open()) {
            err = "Failed to write temp input file for Piper.";
            return false;
        }
        out << text;
        out.close();

        // Piper CLI: cmd.exe /c type "tempTextPath" | "piper.exe" -m "model.onnx" --output_file "wav"
        std::string cmd = "cmd.exe /c type \"" + tempTextPath + "\" | \"" + exePath_ + "\" -m \"" + modelPath_ + "\" --output_file \"" + wavPath + "\"";

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
                err = "Piper synthesis process timed out.";
                return false;
            }
        } else {
            err = "Failed to launch Piper synthesis process.";
            return false;
        }

        std::ifstream f(wavPath, std::ios::binary | std::ios::ate);
        if (f.is_open() && f.tellg() > 44) {
            return true;
        }
        err = "Piper output file is missing or invalid.";
        return false;
    }

    std::string backendName() const {
        return "Piper";
    }

    std::string voiceName() const {
        return voiceName_;
    }

private:
    std::string exePath_;
    std::string modelPath_;
    std::string voiceName_;
};

// -------------------------------------------------
// SapiBackend Definition & Implementation
// -------------------------------------------------
class SapiBackend {
public:
    SapiBackend() : pSpVoice_(nullptr), usingFemaleVoice_(false) {}
    ~SapiBackend() {
        shutdown();
    }

    bool probe() {
        ISpVoice* pVoice = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void**)&pVoice);
        if (SUCCEEDED(hr) && pVoice) {
            pVoice->Release();
            return true;
        }
        return false;
    }

    bool initialize() {
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

    void shutdown() {
        if (pSpVoice_) {
            pSpVoice_->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
            pSpVoice_->Release();
            pSpVoice_ = nullptr;
        }
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

    std::string backendName() const {
        return "SAPI";
    }

    std::string voiceName() const {
        return selectedVoiceName_;
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
class EdgeTTSBackend {
public:
    EdgeTTSBackend() {}

    bool probe() {
        // Check yuki_tts_server.py exists
        DWORD a = GetFileAttributesA("data\\vision\\yuki_tts_server.py");
        if (a == INVALID_FILE_ATTRIBUTES) return false;
        // Silently verify Python is reachable (no console window)
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        char cmdProbe[] = "python --version";
        if (!CreateProcessA(NULL, cmdProbe, NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            return false;
        WaitForSingleObject(pi.hProcess, 4000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return (exitCode == 0);
    }

    bool initialize() { return probe(); }
    void shutdown() {}

    // Synthesize text -> WAV by calling the standalone yuki_edge_speak.py helper.
    // This avoids fragile inline Python string generation entirely.
    // yuki_edge_speak.py handles: EdgeTTS -> MP3 -> WAV via pydub (confirmed working).
    bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) {
        CreateDirectoryA("data", NULL);
        CreateDirectoryA("data\\tts", NULL);

        // Write text to input file (UTF-8, no BOM) so Python reads it cleanly
        std::string tmpIn = "data\\tts\\edge_input.txt";
        {
            HANDLE hFile = CreateFileA(tmpIn.c_str(), GENERIC_WRITE, 0, NULL,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                err = "Cannot create EdgeTTS input file."; return false;
            }
            DWORD written = 0;
            WriteFile(hFile, text.c_str(), (DWORD)text.size(), &written, NULL);
            CloseHandle(hFile);
        }

        // Call: python "data\tts\yuki_edge_speak.py" "<tmpIn>" "<wavPath>"
        std::string helperScript = "data\\tts\\yuki_edge_speak.py";
        std::string cmd = "python \"" + helperScript + "\" \"" + tmpIn + "\" \"" + wavPath + "\"";
        std::vector<char> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back('\0');

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            err = "Failed to launch EdgeTTS helper process.";
            return false;
        }

        // 35s timeout (EdgeTTS network round-trip + pydub decode)
        DWORD waitRes = WaitForSingleObject(pi.hProcess, 35000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

        if (waitRes == WAIT_TIMEOUT) {
            err = "EdgeTTS timed out (35s). Check internet connection.";
            return false;
        }
        if (exitCode != 0) {
            err = "EdgeTTS helper failed (exit " + std::to_string(exitCode) + ")";
            return false;
        }

        // Verify WAV output
        std::ifstream f(wavPath, std::ios::binary | std::ios::ate);
        if (f.is_open() && f.tellg() > 44) return true;

        err = "EdgeTTS: WAV output missing or too small.";
        return false;
    }

    std::string backendName() const { return "EdgeTTS"; }
    std::string voiceName()   const { return "en-US-JennyNeural"; }
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

    std::lock_guard<std::mutex> lock(mutex_);
    backendInfo_.type = VoiceBackendType::NONE;
    backendInfo_.backendName = "Uninitialized";
    backendInfo_.voiceName = "None";
    backendInfo_.available = false;
    backendInfo_.neural = false;
    backendInfo_.detail = "Awaiting engine prewarm.";
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
        backendInfo_.type = VoiceBackendType::KOKORO; // re-use enum slot for now
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

bool MouthRuntime::executeOneShotPlan(const SpeechPlan& plan, uint64_t serial) {
    if (!requestStillCurrent(serial)) return false;

    std::string tempWav = "data\\tts\\temp_oneshot.wav";
    std::string err;

    // EdgeTTS is highest priority (Microsoft Neural Voice)
    if (backendInfo_.backendName == "EdgeTTS") {
        if (edgeTtsBackend_->synthesizeToFile(plan.normalizedText, tempWav, err)) {
            return playWaveTruthfully(tempWav, serial);
        }
        std::cerr << "[Mouth] EdgeTTS failed: " << err << ". Falling back to SAPI.\n";
    } else if (backendInfo_.type == VoiceBackendType::KOKORO) {
        if (kokoroBackend_->synthesizeToFile(plan.normalizedText, tempWav, err)) {
            return playWaveTruthfully(tempWav, serial);
        } else {
            std::cerr << "[Mouth] Kokoro synthesis failed: " << err << ". Trying Piper...\n";
            if (piperBackend_->probe() && piperBackend_->synthesizeToFile(plan.normalizedText, tempWav, err)) {
                return playWaveTruthfully(tempWav, serial);
            }
        }
    } else if (backendInfo_.type == VoiceBackendType::PIPER) {
        if (piperBackend_->synthesizeToFile(plan.normalizedText, tempWav, err)) {
            return playWaveTruthfully(tempWav, serial);
        } else {
            std::cerr << "[Mouth] Piper synthesis failed: " << err << ". Trying SAPI...\n";
        }
    }

    // SAPI final fallback
    if (sapiBackend_->initialize()) {
        return sapiBackend_->speakDirect(plan.normalizedText, serial,
            [this](uint64_t s) { return this->requestStillCurrent(s); },
            [this](SpeakPhase p, const std::string& t) { this->setPhase(p, t); });
    }
    return false;
}

bool MouthRuntime::executeProgressivePlan(const SpeechPlan& plan, uint64_t serial) {
    for (size_t i = 0; i < plan.chunks.size(); ++i) {
        if (!requestStillCurrent(serial)) return false;

        const std::string& chunk = plan.chunks[i];
        std::string tempWav = "data\\tts\\temp_chunk_" + std::to_string(i) + ".wav";
        std::string err;
        bool played = false;

        // EdgeTTS first
        if (backendInfo_.backendName == "EdgeTTS") {
            if (edgeTtsBackend_->synthesizeToFile(chunk, tempWav, err)) {
                played = playWaveTruthfully(tempWav, serial);
            } else {
                std::cerr << "[Mouth] EdgeTTS chunk failed: " << err << "\n";
            }
        } else if (backendInfo_.type == VoiceBackendType::KOKORO) {
            if (kokoroBackend_->synthesizeToFile(chunk, tempWav, err)) {
                played = playWaveTruthfully(tempWav, serial);
            } else {
                std::cerr << "[Mouth] Kokoro chunk synthesis failed: " << err << ". Trying Piper...\n";
                if (piperBackend_->probe() && piperBackend_->synthesizeToFile(chunk, tempWav, err)) {
                    played = playWaveTruthfully(tempWav, serial);
                }
            }
        } else if (backendInfo_.type == VoiceBackendType::PIPER) {
            if (piperBackend_->synthesizeToFile(chunk, tempWav, err)) {
                played = playWaveTruthfully(tempWav, serial);
            }
        }

        if (!played) {
            if (sapiBackend_->initialize()) {
                if (!sapiBackend_->speakDirect(chunk, serial,
                    [this](uint64_t s) { return this->requestStillCurrent(s); },
                    [this](SpeakPhase p, const std::string& t) { this->setPhase(p, t); })) {
                    return false;
                }
            } else {
                return false;
            }
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
