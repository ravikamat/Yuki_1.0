#include "input/VoiceEngine.h"
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#endif

namespace yuki::input {

#ifdef _WIN32
static ISpVoice* g_pVoice = nullptr;
#endif

VoiceEngine::VoiceEngine() = default;

VoiceEngine::~VoiceEngine() {
    shutdown();
}

bool VoiceEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

#ifdef _WIN32
    if (FAILED(::CoInitialize(NULL))) return false;
    if (FAILED(::CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&g_pVoice))) {
        return false;
    }
#endif
    initialized_ = true;
    return true;
}

bool VoiceEngine::speak(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return false;
    if (text.empty()) return true;

#ifdef _WIN32
    if (g_pVoice) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
        if (wlen > 0) {
            std::wstring wtext(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);
            g_pVoice->Speak(wtext.c_str(), SPF_ASYNC, NULL);
        }
    }
#endif
    return true;
}

bool VoiceEngine::setRate(int rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = std::clamp(rate, -10, 10);
#ifdef _WIN32
    if (g_pVoice) {
        g_pVoice->SetRate(rate_);
    }
#endif
    return true;
}

bool VoiceEngine::setVolume(int volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    volume_ = std::clamp(volume, 0, 100);
#ifdef _WIN32
    if (g_pVoice) {
        g_pVoice->SetVolume(static_cast<USHORT>(volume_));
    }
#endif
    return true;
}

void VoiceEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return;
#ifdef _WIN32
    if (g_pVoice) {
        g_pVoice->Release();
        g_pVoice = nullptr;
    }
    ::CoUninitialize();
#endif
    initialized_ = false;
}

} // namespace yuki::input
