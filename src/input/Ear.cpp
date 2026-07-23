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

#include <iostream>

bool EarRuntime::openDevice() {
    closeDevice(); // Clean up any previous state first
    
    UINT numDevs = waveInGetNumDevs();
    if (numDevs == 0) {
        return false;
    }

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 32000;
    wfx.cbSize = 0;

    const int MAX_RETRIES = 5;
    int retryDelayMs = 500; // start at 500ms
    MMRESULT mmr = MMSYSERR_ERROR;
    
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        mmr = waveInOpen(&hWaveIn_, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
        if (mmr == MMSYSERR_NOERROR) {
            std::cout << "[EarRuntime] waveInOpen success on attempt " << (attempt + 1) << "\n";
            break;
        }
        
        std::cerr << "[EarRuntime] waveInOpen failed (attempt " << (attempt + 1) << "/" << MAX_RETRIES 
                  << "), retrying in " << retryDelayMs << "ms...\n";
        
        // Check running_ in case we are stopping mid-retry
        if (!running_.load()) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        retryDelayMs = (std::min)(retryDelayMs * 2, 8000); // cap at 8s
    }

    if (mmr != MMSYSERR_NOERROR) {
        std::cerr << "[EarRuntime] waveInOpen failed after " << MAX_RETRIES << " attempts.\n";
        return false;
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
    buf1_ = new short[bufferSize / 2];
    buf2_ = new short[bufferSize / 2];

    hdr1_ = {};
    hdr1_.lpData = (LPSTR)buf1_;
    hdr1_.dwBufferLength = bufferSize;
    waveInPrepareHeader(hWaveIn_, &hdr1_, sizeof(WAVEHDR));

    hdr2_ = {};
    hdr2_.lpData = (LPSTR)buf2_;
    hdr2_.dwBufferLength = bufferSize;
    waveInPrepareHeader(hWaveIn_, &hdr2_, sizeof(WAVEHDR));

    waveInAddBuffer(hWaveIn_, &hdr1_, sizeof(WAVEHDR));
    waveInAddBuffer(hWaveIn_, &hdr2_, sizeof(WAVEHDR));

    waveInStart(hWaveIn_);
    return true;
}

void EarRuntime::closeDevice() {
    if (hWaveIn_) {
        waveInStop(hWaveIn_);
        waveInReset(hWaveIn_);
        if (buf1_) {
            waveInUnprepareHeader(hWaveIn_, &hdr1_, sizeof(WAVEHDR));
        }
        if (buf2_) {
            waveInUnprepareHeader(hWaveIn_, &hdr2_, sizeof(WAVEHDR));
        }
        waveInClose(hWaveIn_);
        hWaveIn_ = nullptr;
    }
    if (buf1_) {
        delete[] buf1_;
        buf1_ = nullptr;
    }
    if (buf2_) {
        delete[] buf2_;
        buf2_ = nullptr;
    }
    memset(&hdr1_, 0, sizeof(WAVEHDR));
    memset(&hdr2_, 0, sizeof(WAVEHDR));
}

void EarRuntime::captureLoop() {
    usingSimulation_ = !openDevice();
    state_ = SubsystemRuntimeState::RUNNING;
    
    lastAudioTime_ = std::chrono::steady_clock::now();
    audioStalled_.store(false);

    if (usingSimulation_) {
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            deviceName_ = "Simulated Audio Input (No Mic Hardware)";
            lastError_ = "No audio input hardware.";
        }
        
        while (running_) {
            try {
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
            catch (const std::exception& e) {
                std::cerr << "[EarRuntime] captureLoop simulation exception: " << e.what() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...) {
                std::cerr << "[EarRuntime] captureLoop simulation unknown exception\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return;
    }

    while (running_) {
        try {
            // Stall detection
            if (std::chrono::steady_clock::now() - lastAudioTime_ > std::chrono::seconds(5)) {
                if (!audioStalled_.exchange(true)) {
                    std::cerr << "[EarRuntime] Audio stall detected (no data for 5s). Attempting reconnection...\n";
                    closeDevice();
                    usingSimulation_ = !openDevice();
                    if (usingSimulation_) {
                        std::cerr << "[EarRuntime] Reconnection failed. Continuing simulated fallback.\n";
                        {
                            std::lock_guard<std::mutex> lock(dataMutex_);
                            deviceName_ = "Simulated Audio Input (Reconnection Failed)";
                            lastError_ = "Audio device stalled and reconnection failed.";
                        }
                        // Drop into simulation mode
                        continue;
                    } else {
                        // Reconnection succeeded, reset watchdog
                        lastAudioTime_ = std::chrono::steady_clock::now();
                        audioStalled_.store(false);
                    }
                }
            }

            if (usingSimulation_) {
                // We fell back to simulation during stall
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
                continue;
            }

            double sum = 0;
            int count = 0;
            
            if (hdr1_.dwFlags & WHDR_DONE) {
                short* samples = (short*)hdr1_.lpData;
                int nSamples = hdr1_.dwBytesRecorded / 2;
                for (int i = 0; i < nSamples; ++i) {
                    sum += samples[i] * samples[i];
                }
                count += nSamples;

                // Accumulate real samples
                if (nSamples > 0) {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    capturedSamples_.insert(capturedSamples_.end(), samples, samples + nSamples);
                    if (capturedSamples_.size() > maxSamples_) {
                        capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                    }
                    lastAudioTime_ = std::chrono::steady_clock::now();
                    audioStalled_.store(false);
                }

                waveInAddBuffer(hWaveIn_, &hdr1_, sizeof(WAVEHDR));
            }
            
            if (hdr2_.dwFlags & WHDR_DONE) {
                short* samples = (short*)hdr2_.lpData;
                int nSamples = hdr2_.dwBytesRecorded / 2;
                for (int i = 0; i < nSamples; ++i) {
                    sum += samples[i] * samples[i];
                }
                count += nSamples;

                // Accumulate real samples
                if (nSamples > 0) {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    capturedSamples_.insert(capturedSamples_.end(), samples, samples + nSamples);
                    if (capturedSamples_.size() > maxSamples_) {
                        capturedSamples_.erase(capturedSamples_.begin(), capturedSamples_.begin() + (capturedSamples_.size() - maxSamples_));
                    }
                    lastAudioTime_ = std::chrono::steady_clock::now();
                    audioStalled_.store(false);
                }

                waveInAddBuffer(hWaveIn_, &hdr2_, sizeof(WAVEHDR));
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
        catch (const std::exception& e) {
            std::cerr << "[EarRuntime] captureLoop exception: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        catch (...) {
            std::cerr << "[EarRuntime] captureLoop unknown exception\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    closeDevice();
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
