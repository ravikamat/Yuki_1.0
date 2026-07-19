// ContextMemory.cpp — Conversation memory + world snapshot (merged)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "brain/memory/ContextMemory.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

// ══════════════════════════════════════════════════════════════════════════════
// ConversationMemory
// ══════════════════════════════════════════════════════════════════════════════

ConversationMemory::ConversationMemory(size_t maxTurns) : maxTurns_(maxTurns) {}

void ConversationMemory::pushTurn(MemoryTurn&& turn) {
    std::lock_guard<std::mutex> lock(mutex_);
    turns_.push_back(std::move(turn));
    while (turns_.size() > maxTurns_) turns_.erase(turns_.begin());
}

void ConversationMemory::recordUser(const std::string& text, bool wasVoice) {
    MemoryTurn t; t.timestampMs=GetTickCount64(); t.speaker="User"; t.text=text; t.wasVoice=wasVoice; t.wasCommand=false;
    pushTurn(std::move(t));
}
void ConversationMemory::recordYuki(const std::string& text, bool wasCommand) {
    MemoryTurn t; t.timestampMs=GetTickCount64(); t.speaker="Yuki"; t.text=text; t.wasVoice=false; t.wasCommand=wasCommand;
    pushTurn(std::move(t));
}

std::vector<MemoryTurn> ConversationMemory::getRecentTurns(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (turns_.empty()) return {};
    size_t start = (turns_.size() > n) ? (turns_.size()-n) : 0;
    return std::vector<MemoryTurn>(turns_.begin()+start, turns_.end());
}

std::vector<std::string> ConversationMemory::getRecentUserTexts(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    for (auto it=turns_.rbegin(); it!=turns_.rend()&&out.size()<n; ++it)
        if (it->speaker=="User") out.push_back(it->text);
    std::reverse(out.begin(),out.end()); return out;
}

std::string ConversationMemory::buildContextBlock(size_t maxTurns) const {
    auto recent=getRecentTurns(maxTurns); if (recent.empty()) return "";
    std::string out;
    out.reserve((std::min)(recent.size(), maxTurns) * 80);
    out += "[Recent conversation]\n";
    for (const auto& t : recent) { out += t.speaker; out += ": "; out += t.text; out += "\n"; }
    return out;
}

size_t ConversationMemory::size() const { std::lock_guard<std::mutex> lock(mutex_); return turns_.size(); }
void   ConversationMemory::clear()      { std::lock_guard<std::mutex> lock(mutex_); turns_.clear(); }

bool ConversationMemory::userRecentlySaid(const std::string& keyword, size_t withinLastN) const {
    std::lock_guard<std::mutex> lock(mutex_); size_t checked=0;
    for (auto it=turns_.rbegin(); it!=turns_.rend()&&checked<withinLastN; ++it) {
        if (it->speaker=="User") {
            std::string lower=it->text; std::transform(lower.begin(),lower.end(),lower.begin(),::tolower);
            if (lower.find(keyword)!=std::string::npos) return true; ++checked;
        }
    }
    return false;
}

bool ConversationMemory::yukiRecentlySaid(const std::string& text, size_t withinLastN) const {
    std::lock_guard<std::mutex> lock(mutex_); size_t checked=0;
    for (auto it=turns_.rbegin(); it!=turns_.rend()&&checked<withinLastN; ++it) {
        if (it->speaker=="Yuki") { if (it->text.find(text)!=std::string::npos) return true; ++checked; }
    }
    return false;
}

// ══════════════════════════════════════════════════════════════════════════════
// WorldSnapshot
// ══════════════════════════════════════════════════════════════════════════════

uint64_t WorldSnapshot::ageMs() const { return GetTickCount64()-capturedAtMs; }

std::string WorldSnapshot::toCompactString() const {
    std::ostringstream ss;
    ss<<"CPU="<<static_cast<int>(cpuPercent)<<"%"
      <<" RAM="<<ramLoadPct<<"%"
      <<" STORAGE="<<freeStorageGb<<"GB"
      <<" NET="<<(internetAlive?"UP":"DOWN")
      <<" | MIC="<<(micActive?(micCapturing?"LIVE":"ON"):"OFF")
      <<" RMS="<<std::fixed<<std::setprecision(1)<<micRmsLevel
      <<" | SPK="<<(speakerActive?(activelySpeaking?"SPEAKING":"READY"):"OFF")
      <<" BACKEND="<<ttsBackend
      <<" | SCR="<<(screenActive?focusedProcess:"OFF");
    if (screenActive&&screenVisionServerActive)
        ss<<" BRI="<<static_cast<int>(screenBrightness)<<" EDGE="<<std::setprecision(2)<<screenEdgeDensity<<" ACT="<<screenActivity;
    ss<<" | CAM="<<(cameraActive?(cameraHardware?"HW":"NOHW"):"OFF");
    if (cameraActive&&cameraVisionServerActive)
        ss<<" FACES="<<faceCount<<" MOTION="<<(motionDetected?"Y":"N")<<" LIGHT="<<cameraLighting;
    return ss.str();
}

SystemHealthGrade WorldSnapshotBuilder::rateHealth(double cpu, uint32_t ram) const {
    if (cpu<30&&ram<50) return SystemHealthGrade::EXCELLENT;
    if (cpu<55&&ram<70) return SystemHealthGrade::GOOD;
    if (cpu<75&&ram<85) return SystemHealthGrade::MODERATE;
    if (cpu<90&&ram<95) return SystemHealthGrade::STRAINED;
    return SystemHealthGrade::CRITICAL;
}
AudioAmbience WorldSnapshotBuilder::rateAmbience(double rms) const {
    if (rms<100) return AudioAmbience::SILENT;
    if (rms<500) return AudioAmbience::QUIET;
    if (rms<2000) return AudioAmbience::MODERATE;
    return AudioAmbience::LOUD;
}

WorldSnapshot WorldSnapshotBuilder::build(const SubsystemControl& control,
                                           const EarRuntime&       mic,
                                           const MouthRuntime&     mouth,
                                           const ScreenRuntime*    screenRt,
                                           const CameraRuntime*    cameraRt) const {
    WorldSnapshot snap; snap.capturedAtMs=GetTickCount64();

    // Body State
    BodyStateReader bodyReader; BodyStateSnapshot body=bodyReader.capture(control);
    if (body.subsystem_active) {
        snap.cpuPercent=body.cpu_usage_percent; snap.ramLoadPct=static_cast<uint32_t>(body.memory_load_percent);
        snap.freeStorageGb=body.free_storage_gb; snap.internetAlive=body.internet_available;
    }
    snap.healthGrade=rateHealth(snap.cpuPercent,snap.ramLoadPct);

    // Screen Context: Win32
    ScreenEyeReader screenReader; ScreenSnapshot screenEye=screenReader.capture(control);
    snap.screenActive=screenEye.subsystem_active;
    if (screenEye.subsystem_active) {
        snap.focusedAppTitle=screenEye.foreground_window_title; snap.focusedAppClass=screenEye.foreground_window_class;
        snap.focusedProcess=screenEye.foreground_process_name; snap.screenW=screenEye.screen_width; snap.screenH=screenEye.screen_height;
    }

    // Screen Context: Python CV
    if (screenRt&&snap.screenActive) {
        ScreenFrameSnapshot frame=screenRt->getLatestFrame();
        if (!frame.foregroundTitle.empty())   snap.focusedAppTitle=frame.foregroundTitle;
        if (!frame.foregroundProcess.empty()) snap.focusedProcess=frame.foregroundProcess;
        if (!frame.foregroundClass.empty())   snap.focusedAppClass=frame.foregroundClass;
        snap.screenW=frame.width; snap.screenH=frame.height; snap.screenChanged=frame.screenChanged; snap.screenPixelHash=frame.pixelHash;
        snap.screenVisionServerActive=frame.analysis.visionServerActive; snap.screenBrightness=frame.analysis.brightness;
        snap.screenEdgeDensity=frame.analysis.edgeDensity; snap.screenActivity=frame.analysis.activityLevel;
        snap.screenDominantColour=frame.analysis.dominantColour; snap.screenOcrText=frame.analysis.ocrText;
    }

    // Microphone
    EarReader earReader; EarSnapshot ear=earReader.capture(control,mic);
    snap.micActive=ear.subsystem_active; snap.micCapturing=ear.capture_pipeline_active;
    snap.micRmsLevel=ear.latest_rms; snap.micDeviceName=ear.device_name; snap.audioAmbience=rateAmbience(ear.latest_rms);

    // Speaker
    MouthReader mouthReader; MouthSnapshot mouthSnap=mouthReader.capture(control,mouth);
    snap.speakerActive=mouthSnap.subsystem_active; snap.activelySpeaking=mouthSnap.actively_speaking;
    snap.ttsBackend=mouthSnap.backend_name; snap.ttsVoiceName=mouthSnap.voice_name;

    // Camera
    snap.cameraActive=control.isActive(SubsystemName::WORLD_EYE);
    if (snap.cameraActive&&cameraRt) {
        CameraFrameSnapshot camFrame=cameraRt->getLatestFrame();
        snap.cameraHardware=camFrame.analysis.hardwarePresent; snap.cameraFrameW=camFrame.width; snap.cameraFrameH=camFrame.height;
        snap.cameraDetails=camFrame.details; snap.cameraDeviceName=cameraRt->getDeviceName();
        snap.cameraVisionServerActive=camFrame.analysis.visionServerActive; snap.cameraBrightness=camFrame.analysis.brightness;
        snap.cameraLighting=camFrame.analysis.lighting; snap.faceCount=camFrame.analysis.faceCount; snap.motionDetected=camFrame.analysis.motionDetected;
    } else if (snap.cameraActive) {
        VisionResult vres=vision().getLatestResult();
        snap.cameraFrameW=vres.frameWidth; snap.cameraFrameH=vres.frameHeight; snap.cameraDetails=vres.details;
    }
    return snap;
}
