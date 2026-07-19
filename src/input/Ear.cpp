// Ear.cpp
#include "input/Ear.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winmm.lib")

// -------------------------------------------------
// EarRuntime Implementation
// -------------------------------------------------

EarRuntime::EarRuntime(SubsystemControl& control)
    : control_(control), state_(SubsystemRuntimeState::STOPPED), running_(false) {
}

EarRuntime::~EarRuntime() {
    stop();
}

void EarRuntime::start() {
    SubsystemStatus status = control_.getStatus(SubsystemName::EAR);
    if (!status.active) {
        state_ = SubsystemRuntimeState::UNAVAILABLE;
        return;
    }
    
    if (running_) return;
    running_ = true;
    state_ = SubsystemRuntimeState::STARTING;
    workerThread_ = std::thread(&EarRuntime::captureLoop, this);
}

void EarRuntime::stop() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    state_ = SubsystemRuntimeState::STOPPED;
}

bool EarRuntime::isRunning() const {
    return state_ == SubsystemRuntimeState::RUNNING;
}

SubsystemRuntimeState EarRuntime::reportState() const {
    return state_;
}

double EarRuntime::getLatestVolume() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestVolume_;
}

double EarRuntime::getLatestRms() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestVolume_;
}

bool EarRuntime::hasRecentSignal() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestVolume_ > 25.0; // Standard speech detection threshold used in STT
}

std::string EarRuntime::getDeviceName() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return deviceName_;
}

std::string EarRuntime::getLastError() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastError_;
}

void EarRuntime::captureLoop() {
    UINT numDevs = waveInGetNumDevs();
    if (numDevs == 0) {
        // Fallback to simulation
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            deviceName_ = "Simulated Audio Input (No Mic Hardware)";
            lastError_ = "No audio input hardware.";
        }
        state_ = SubsystemRuntimeState::RUNNING;
        
        while (running_) {
            // Generate dynamic low-amplitude ambient white noise simulation
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                latestVolume_ = 5.0 + (rand() % 1500) / 100.0; 
                // Add simulated samples
                int numSimSamples = 1600; // 100ms at 16kHz
                for (int i = 0; i < numSimSamples; ++i) {
                    capturedSamples_.push_back(static_cast<short>((rand() % 100) - 50));
                }
                if (capturedSamples_.size() > maxSamples_) {
                    capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    }

    // Attempt real waveInOpen
    HWAVEIN hWaveIn = nullptr;
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 32000;
    wfx.cbSize = 0;

    MMRESULT mmr = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (mmr != MMSYSERR_NOERROR) {
        // Fallback to simulation
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            deviceName_ = "Simulated Audio Input (waveInOpen Failed)";
            lastError_ = "waveInOpen failed.";
        }
        state_ = SubsystemRuntimeState::RUNNING;
        while (running_) {
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                latestVolume_ = 4.0 + (rand() % 500) / 100.0;
                int numSimSamples = 1600;
                for (int i = 0; i < numSimSamples; ++i) {
                    capturedSamples_.push_back(static_cast<short>((rand() % 80) - 40));
                }
                if (capturedSamples_.size() > maxSamples_) {
                    capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        lastError_.clear();
        WAVEINCAPSA caps = {};
        if (waveInGetDevCapsA(0, &caps, sizeof(WAVEINCAPSA)) == MMSYSERR_NOERROR) {
            deviceName_ = caps.szPname;
        } else {
            deviceName_ = "Real Default Windows Mic";
        }
    }

    // Allocate double buffers
    const int bufferSize = 3200; // 100ms of audio at 16kHz 16-bit
    short* buf1 = new short[bufferSize / 2];
    short* buf2 = new short[bufferSize / 2];

    WAVEHDR hdr1 = {};
    hdr1.lpData = (LPSTR)buf1;
    hdr1.dwBufferLength = bufferSize;
    waveInPrepareHeader(hWaveIn, &hdr1, sizeof(WAVEHDR));

    WAVEHDR hdr2 = {};
    hdr2.lpData = (LPSTR)buf2;
    hdr2.dwBufferLength = bufferSize;
    waveInPrepareHeader(hWaveIn, &hdr2, sizeof(WAVEHDR));

    waveInAddBuffer(hWaveIn, &hdr1, sizeof(WAVEHDR));
    waveInAddBuffer(hWaveIn, &hdr2, sizeof(WAVEHDR));

    waveInStart(hWaveIn);
    state_ = SubsystemRuntimeState::RUNNING;

    while (running_) {
        double sum = 0;
        int count = 0;
        
        if (hdr1.dwFlags & WHDR_DONE) {
            short* samples = (short*)hdr1.lpData;
            int nSamples = hdr1.dwBytesRecorded / 2;
            for (int i = 0; i < nSamples; ++i) {
                sum += samples[i] * samples[i];
            }
            count += nSamples;

            // Accumulate real samples
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                capturedSamples_.insert(capturedSamples_.end(), samples, samples + nSamples);
                if (capturedSamples_.size() > maxSamples_) {
                    capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                }
            }

            waveInAddBuffer(hWaveIn, &hdr1, sizeof(WAVEHDR));
        }
        
        if (hdr2.dwFlags & WHDR_DONE) {
            short* samples = (short*)hdr2.lpData;
            int nSamples = hdr2.dwBytesRecorded / 2;
            for (int i = 0; i < nSamples; ++i) {
                sum += samples[i] * samples[i];
            }
            count += nSamples;

            // Accumulate real samples
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                capturedSamples_.insert(capturedSamples_.end(), samples, samples + nSamples);
                if (capturedSamples_.size() > maxSamples_) {
                    capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                }
            }

            waveInAddBuffer(hWaveIn, &hdr2, sizeof(WAVEHDR));
        }

        double rms = 0.0;
        if (count > 0) {
            rms = sqrt(sum / count);
        } else {
            // Ambient default noise value if no buffer complete yet
            rms = 10.0 + (rand() % 50) / 10.0;
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            latestVolume_ = rms;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    waveInUnprepareHeader(hWaveIn, &hdr1, sizeof(WAVEHDR));
    waveInUnprepareHeader(hWaveIn, &hdr2, sizeof(WAVEHDR));
    waveInClose(hWaveIn);

    delete[] buf1;
    delete[] buf2;
}

std::vector<short> EarRuntime::drainPCM(size_t keepSamples) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    std::vector<short> samples;
    if (keepSamples == 0 || capturedSamples_.size() <= keepSamples) {
        samples = std::move(capturedSamples_);
        capturedSamples_.clear();
    } else {
        size_t splitIdx = capturedSamples_.size() - keepSamples;
        samples.assign(capturedSamples_.begin(), capturedSamples_.begin() + splitIdx);
        capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + splitIdx);
    }
    return samples;
}

std::vector<short> EarRuntime::getBufferedPCMCopy() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return capturedSamples_;
}

std::vector<short> EarRuntime::readLatestPCMWindow(size_t maxSamples) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (capturedSamples_.empty()) return {};

    const size_t n = capturedSamples_.size();
    const size_t take = (std::min)(maxSamples, n);
    return std::vector<short>(capturedSamples_.end() - take, capturedSamples_.end());
}

// -------------------------------------------------
// EarReader Implementation
// -------------------------------------------------

EarSnapshot EarReader::capture(const SubsystemControl& control, const EarRuntime& runtime) const {
  EarSnapshot snap;

  // 1. Check Subsystem Permission
  SubsystemStatus status = control.getStatus(SubsystemName::EAR);
  snap.allowed = status.active;
  snap.subsystem_available = status.available;
  snap.subsystem_active = status.active;

  // 2. Map EarRuntime state and error
  snap.runtime_running = runtime.isRunning();
  snap.latest_rms = runtime.getLatestRms();
  snap.receiving_signal = runtime.hasRecentSignal();
  snap.device_name = runtime.getDeviceName();
  snap.last_error = runtime.getLastError();

  if (!snap.subsystem_active) {
    snap.summary = "Ear blocked or unavailable.";
    return snap;
  }

  // 3. Minimal Microphone Presence Check (WinMM)
  UINT numDevs = waveInGetNumDevs();
  if (numDevs > 0) {
    snap.microphone_present = true;
    snap.input_stream_ready = true;
    snap.capture_pipeline_active = (status.runtimeState == SubsystemRuntimeState::RUNNING);
  }

  // 4. Summary Logic
  std::ostringstream ss;
  if (snap.microphone_present) {
    ss << "Ear ready: " << (snap.device_name.empty() ? "microphone detected." : snap.device_name + " present.");
    if (snap.receiving_signal) {
      ss << " [Active speech detected (RMS: " << std::fixed << std::setprecision(1) << snap.latest_rms << ")]";
    }
  } else {
    ss << "Ear active but no microphone detected.";
  }

  snap.summary = ss.str();
  return snap;
}
