// Ear.cpp
#include "input/Ear.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <iostream>
#include <random>
#include <cstring>

#pragma comment(lib, "winmm.lib")

namespace {
// Seeded RNG for the simulated-audio fallback. The audit flagged the old
// unseeded rand() calls: they produced the same fake sequence on every run.
std::mt19937& earSimRng() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    return rng;
}
int earSimUniform(int upperInclusive) {
    std::uniform_int_distribution<int> dist(0, upperInclusive);
    return dist(earSimRng());
}
} // namespace

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
    
    // Don't start if Python daemon owns the microphone
    if (pythonDaemonActive_.load()) {
        state_ = SubsystemRuntimeState::UNAVAILABLE;
        return;
    }
    
    if (running_.load()) return;
    running_.store(true);
    state_ = SubsystemRuntimeState::STARTING;
    workerThread_ = std::thread(&EarRuntime::captureLoop, this);
}

void EarRuntime::stop() {
    running_.store(false);
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    // WaveHandle RAII cleans up automatically
    waveHandle_.reset();
    state_ = SubsystemRuntimeState::STOPPED;
}

bool EarRuntime::isRunning() const {
    return state_ == SubsystemRuntimeState::RUNNING;
}

SubsystemRuntimeState EarRuntime::reportState() const {
    return state_.load();
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
    
    auto handle = std::make_unique<WaveHandle>();
    
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        mmr = waveInOpen(&handle->hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
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

    // Allocate double buffers with RAII
    const int bufferSize = 3200; // 100ms of audio at 16kHz 16-bit
    handle->buf1 = std::make_unique<short[]>(bufferSize / 2);
    handle->buf2 = std::make_unique<short[]>(bufferSize / 2);

    handle->hdr1.lpData = (LPSTR)handle->buf1.get();
    handle->hdr1.dwBufferLength = bufferSize;
    waveInPrepareHeader(handle->hWaveIn, &handle->hdr1, sizeof(WAVEHDR));

    handle->hdr2.lpData = (LPSTR)handle->buf2.get();
    handle->hdr2.dwBufferLength = bufferSize;
    waveInPrepareHeader(handle->hWaveIn, &handle->hdr2, sizeof(WAVEHDR));

    waveInAddBuffer(handle->hWaveIn, &handle->hdr1, sizeof(WAVEHDR));
    waveInAddBuffer(handle->hWaveIn, &handle->hdr2, sizeof(WAVEHDR));

    waveInStart(handle->hWaveIn);
    
    waveHandle_ = std::move(handle);
    return true;
}

void EarRuntime::closeDevice() {
    // WaveHandle destructor handles cleanup
    waveHandle_.reset();
}

// ── Ring-buffer helpers ────────────────────────────────────────────────
// Appends are O(1) and never memmove the whole window while holding the lock.
void EarRuntime::pushSamplesLocked(const short* data, size_t count) {
    if (!data || count == 0 || maxSamples_ == 0) return;
    if (ringSamples_.size() != maxSamples_) {
        ringSamples_.assign(maxSamples_, 0);
        ringHead_ = 0;
        ringCount_ = 0;
    }
    if (count >= maxSamples_) {
        data += (count - maxSamples_);   // keep only the newest window
        count = maxSamples_;
        std::memcpy(ringSamples_.data(), data, count * sizeof(short));
        ringHead_ = 0;
        ringCount_ = maxSamples_;
        return;
    }
    const size_t tail = (ringHead_ + ringCount_) % maxSamples_;
    const size_t firstChunk = (std::min)(count, maxSamples_ - tail);
    std::memcpy(ringSamples_.data() + tail, data, firstChunk * sizeof(short));
    if (count > firstChunk) {
        std::memcpy(ringSamples_.data(), data + firstChunk, (count - firstChunk) * sizeof(short));
    }
    const size_t overflow = (ringCount_ + count > maxSamples_) ? (ringCount_ + count - maxSamples_) : 0;
    ringHead_ = (ringHead_ + overflow) % maxSamples_;
    ringCount_ = (std::min)(ringCount_ + count, maxSamples_);
}

std::vector<short> EarRuntime::snapshotLocked() const {
    std::vector<short> out;
    if (ringCount_ == 0) return out;
    out.reserve(ringCount_);
    const size_t firstChunk = (std::min)(ringCount_, maxSamples_ - ringHead_);
    out.insert(out.end(), ringSamples_.begin() + static_cast<std::ptrdiff_t>(ringHead_),
               ringSamples_.begin() + static_cast<std::ptrdiff_t>(ringHead_ + firstChunk));
    if (ringCount_ > firstChunk) {
        out.insert(out.end(), ringSamples_.begin(),
                   ringSamples_.begin() + static_cast<std::ptrdiff_t>(ringCount_ - firstChunk));
    }
    return out;
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
        
        while (running_.load()) {
            try {
                // Generate dynamic low-amplitude ambient white noise simulation
                {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    latestVolume_ = 5.0 + earSimUniform(1500) / 100.0;
                    // Fabricated audio: marked so consumers can tell it apart
                    // from a real microphone signal.
                    int numSimSamples = 1600; // 100ms at 16kHz
                    short simBuffer[1600];
                    for (int i = 0; i < numSimSamples; ++i) {
                        simBuffer[i] = static_cast<short>(earSimUniform(100) - 50);
                    }
                    pushSamplesLocked(simBuffer, static_cast<size_t>(numSimSamples));
                    dataSimulated_.store(true);
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

    while (running_.load()) {
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
                            dataSimulated_.store(true);
                        }
                        // Drop into simulation mode. Clear the stall latch so the
                        // watchdog does not keep firing while we deliberately
                        // serve simulated audio (audit: divergent stall state).
                        audioStalled_.store(false);
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
                    latestVolume_ = 4.0 + earSimUniform(500) / 100.0;
                    int numSimSamples = 1600;
                    short simBuffer[1600];
                    for (int i = 0; i < numSimSamples; ++i) {
                        simBuffer[i] = static_cast<short>(earSimUniform(80) - 40);
                    }
                    pushSamplesLocked(simBuffer, static_cast<size_t>(numSimSamples));
                    dataSimulated_.store(true);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            double sum = 0;
            int count = 0;
            
            if (!waveHandle_ || !waveHandle_->hWaveIn) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            
            if (waveHandle_->hdr1.dwFlags & WHDR_DONE) {
                short* samples = (short*)waveHandle_->hdr1.lpData;
                int nSamples = waveHandle_->hdr1.dwBytesRecorded / 2;
                for (int i = 0; i < nSamples; ++i) {
                    sum += samples[i] * samples[i];
                }
                count += nSamples;

                // Accumulate real samples
                if (nSamples > 0) {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    pushSamplesLocked(samples, static_cast<size_t>(nSamples));
                    dataSimulated_.store(false);
                    lastAudioTime_ = std::chrono::steady_clock::now();
                    audioStalled_.store(false);
                }

                waveInAddBuffer(waveHandle_->hWaveIn, &waveHandle_->hdr1, sizeof(WAVEHDR));
            }
            
            if (waveHandle_->hdr2.dwFlags & WHDR_DONE) {
                short* samples = (short*)waveHandle_->hdr2.lpData;
                int nSamples = waveHandle_->hdr2.dwBytesRecorded / 2;
                for (int i = 0; i < nSamples; ++i) {
                    sum += samples[i] * samples[i];
                }
                count += nSamples;

                // Accumulate real samples
                if (nSamples > 0) {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    pushSamplesLocked(samples, static_cast<size_t>(nSamples));
                    dataSimulated_.store(false);
                    lastAudioTime_ = std::chrono::steady_clock::now();
                    audioStalled_.store(false);
                }

                waveInAddBuffer(waveHandle_->hWaveIn, &waveHandle_->hdr2, sizeof(WAVEHDR));
            }

            double rms = 0.0;
            if (count > 0) {
                rms = sqrt(sum / count);
            } else {
                // Ambient default noise value if no buffer complete yet
                rms = 10.0 + earSimUniform(50) / 10.0;
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
    if (ringCount_ == 0) return {};
    if (keepSamples == 0 || ringCount_ <= keepSamples) {
        // Historical contract: hand over everything and reset the window.
        std::vector<short> samples = snapshotLocked();
        ringHead_ = 0;
        ringCount_ = 0;
        return samples;
    }
    const size_t drop = ringCount_ - keepSamples;
    std::vector<short> samples;
    samples.reserve(drop);
    const size_t firstChunk = (std::min)(drop, maxSamples_ - ringHead_);
    samples.insert(samples.end(), ringSamples_.begin() + static_cast<std::ptrdiff_t>(ringHead_),
                   ringSamples_.begin() + static_cast<std::ptrdiff_t>(ringHead_ + firstChunk));
    if (drop > firstChunk) {
        samples.insert(samples.end(), ringSamples_.begin(),
                       ringSamples_.begin() + static_cast<std::ptrdiff_t>(drop - firstChunk));
    }
    ringHead_ = (ringHead_ + drop) % maxSamples_;
    ringCount_ -= drop;
    return samples;
}

std::vector<short> EarRuntime::getBufferedPCMCopy() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return snapshotLocked();
}

std::vector<short> EarRuntime::readLatestPCMWindow(size_t maxSamples) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (ringCount_ == 0) return {};
    const size_t take = (std::min)(maxSamples, ringCount_);
    const size_t start = (ringHead_ + ringCount_ - take) % maxSamples_;
    std::vector<short> out;
    out.reserve(take);
    const size_t firstChunk = (std::min)(take, maxSamples_ - start);
    out.insert(out.end(), ringSamples_.begin() + static_cast<std::ptrdiff_t>(start),
               ringSamples_.begin() + static_cast<std::ptrdiff_t>(start + firstChunk));
    if (take > firstChunk) {
        out.insert(out.end(), ringSamples_.begin(),
                   ringSamples_.begin() + static_cast<std::ptrdiff_t>(take - firstChunk));
    }
    return out;
}

bool EarRuntime::preWarm(int warmupMs) {
    if (!running_.load()) {
        start();
    }
    if (warmupMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(warmupMs));
    }
    return running_.load();
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
  snap.simulated = runtime.isSimulated();

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