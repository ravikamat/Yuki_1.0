// SpeechSystem.cpp — WhisperEngine + SpeechToTextRuntime (merged)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "input/SpeechSystem.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include "whisper.h"

// ══════════════════════════════════════════════════════════════════════════════
// WhisperEngine
// ══════════════════════════════════════════════════════════════════════════════

const char* whisperModelStatusStr(WhisperModelStatus s) {
    switch (s) {
        case WhisperModelStatus::MODEL_NOT_FOUND:  return "MODEL_NOT_FOUND";
        case WhisperModelStatus::MODEL_EMPTY:       return "MODEL_EMPTY";
        case WhisperModelStatus::MODEL_INVALID:     return "MODEL_INVALID";
        case WhisperModelStatus::MODEL_LOAD_FAILED: return "MODEL_LOAD_FAILED";
        case WhisperModelStatus::READY:             return "READY";
        case WhisperModelStatus::DISABLED:          return "DISABLED";
        default:                                    return "UNKNOWN";
    }
}

WhisperEngine::WhisperEngine() : ctx_(nullptr), modelStatus_(WhisperModelStatus::DISABLED) {}
WhisperEngine::~WhisperEngine() { unloadModel(); }

WhisperModelStatus WhisperEngine::loadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    unloadModel();
    lastError_.clear();

    std::ifstream f(modelPath, std::ios::binary | std::ios::ate);
    if (!f.is_open() || !f.good()) {
        lastError_ = "Model file not found: " + modelPath;
        modelStatus_ = WhisperModelStatus::MODEL_NOT_FOUND;
        std::cerr << "[STT Engine] [CHECKPOINT 1] [ERROR] Model file NOT FOUND at path: " << modelPath << "\n";
        std::cerr << "[STT Engine] [GUIDE] Place a valid GGML model in data/models/whisper/\n";
        return modelStatus_;
    }

    std::streamsize fileSizeBytes = f.tellg();
    f.seekg(0, std::ios::beg);
    double fileSizeMB = static_cast<double>(fileSizeBytes) / (1024.0 * 1024.0);

    std::cout << "[STT Engine] [CHECKPOINT 1] Model path exists: " << modelPath << "\n";
    std::cout << "[STT Engine] [CHECKPOINT 1A] Model file size: "
              << fileSizeBytes << " bytes ("
              << std::fixed << std::setprecision(2) << fileSizeMB << " MB)\n";

    if (fileSizeBytes < 1024) {
        lastError_ = "Model file is suspiciously small (" + std::to_string(fileSizeBytes) + " bytes).";
        modelStatus_ = WhisperModelStatus::MODEL_EMPTY;
        std::cerr << "[STT Engine] [CHECKPOINT 1A] [ERROR] Model file too small: " << fileSizeBytes << " bytes.\n";
        f.close(); return modelStatus_;
    }

    unsigned char header[4] = {0};
    f.read(reinterpret_cast<char*>(header), 4);
    f.close();

    std::ostringstream hexHeader;
    hexHeader << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < 4; ++i) { hexHeader << std::setw(2) << static_cast<int>(header[i]); if (i < 3) hexHeader << " "; }
    std::cout << "[STT Engine] [CHECKPOINT 1B] File header: [" << hexHeader.str() << "]\n";

    std::cout << "[STT Engine] [CHECKPOINT 2] Attempting whisper context creation from: " << modelPath << "\n";
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    ctx_ = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

    if (!ctx_) {
        bool isGGMLv1 = (header[0]==0x6C&&header[1]==0x6D&&header[2]==0x67&&header[3]==0x67);
        bool isGGMFv2 = (header[0]==0x67&&header[1]==0x67&&header[2]==0x6D&&header[3]==0x66);
        bool isGGSN   = (header[0]==0x67&&header[1]==0x67&&header[2]==0x73&&header[3]==0x6E);
        bool isGGSS   = (header[0]==0x67&&header[1]==0x67&&header[2]==0x73&&header[3]==0x73);
        bool badMagic  = !(isGGMLv1||isGGMFv2||isGGSN||isGGSS);
        if (badMagic) {
            modelStatus_ = WhisperModelStatus::MODEL_INVALID;
            lastError_ = "Invalid model file (bad magic). Header: [" + hexHeader.str() + "]";
            std::cerr << "[STT Engine] [ERROR] Bad GGML magic. Download from: https://huggingface.co/ggerganov/whisper.cpp\n";
        } else {
            modelStatus_ = WhisperModelStatus::MODEL_LOAD_FAILED;
            lastError_ = "Whisper context creation failed despite valid header.";
            std::cerr << "[STT Engine] [ERROR] whisper_init returned null — try re-downloading the model.\n";
        }
        return modelStatus_;
    }

    modelStatus_ = WhisperModelStatus::READY;
    std::cout << "[STT Engine] [CHECKPOINT 2] Model READY: " << modelPath
              << " (" << std::fixed << std::setprecision(2) << fileSizeMB << " MB)\n";
    return modelStatus_;
}

void WhisperEngine::unloadModel() {
    if (ctx_) { whisper_free(ctx_); ctx_ = nullptr; }
    if (modelStatus_ == WhisperModelStatus::READY) modelStatus_ = WhisperModelStatus::DISABLED;
}

bool               WhisperEngine::isLoaded() const     { std::lock_guard<std::mutex> lock(mutex_); return ctx_ != nullptr; }
WhisperModelStatus WhisperEngine::getModelStatus() const { std::lock_guard<std::mutex> lock(mutex_); return modelStatus_; }
std::string        WhisperEngine::getLastError() const  { std::lock_guard<std::mutex> lock(mutex_); return lastError_; }

std::string WhisperEngine::transcribe(const std::vector<float>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ctx_ || samples.empty()) return "";
    std::cout << "[STT Engine] [CHECKPOINT 7] Decode started with " << samples.size() << " samples.\n";
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress=false; wparams.print_special=false;
    wparams.print_realtime=false; wparams.print_timestamps=false;
    wparams.language=nullptr; wparams.no_context=true;  // nullptr = auto-detect language (Phase 1 fix)

    int ret = whisper_full(ctx_, wparams, samples.data(), static_cast<int>(samples.size()));
    std::cout << "[STT Engine] [CHECKPOINT 8] Decode finished (ret=" << ret << ")\n";
    if (ret != 0) { lastError_="whisper_full failed"; return ""; }
    int n_segments = whisper_full_n_segments(ctx_);
    std::string text;
    for (int i = 0; i < n_segments; ++i) { const char* seg = whisper_full_get_segment_text(ctx_, i); if (seg) text += seg; }
    // Trim
    auto trim = [](std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
        return s;
    };
    text = trim(text);
    if (text.empty()) std::cout << "[STT Engine] [CHECKPOINT 10] Empty transcript.\n";
    else              std::cout << "[STT Engine] [CHECKPOINT 9] Transcript: \"" << text << "\"\n";
    return text;
}

std::string WhisperEngine::transcribePartial(const std::vector<float>& samples) { return transcribe(samples); }

// ══════════════════════════════════════════════════════════════════════════════
// SpeechToTextRuntime
// ══════════════════════════════════════════════════════════════════════════════

namespace {
std::string trimCopy(std::string s) {
    auto notSpace = [](unsigned char ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}
std::string normalizePartial(std::string s) {
    s = trimCopy(std::move(s));
    for (char& c : s) if (c=='\n'||c=='\r'||c=='\t') c=' ';
    while (s.find("  ")!=std::string::npos) s.replace(s.find("  "),2," ");
    return s;
}
enum class UtteranceState { IDLE, PRIMED, IN_SPEECH, READY_TO_DECODE };
} // namespace

SpeechToTextRuntime::SpeechToTextRuntime(EarRuntime& ear, SubsystemControl& subsystems)
    : ear_(ear), subsystems_(subsystems), state_(SttState::STOPPED), running_(false) {}

SpeechToTextRuntime::~SpeechToTextRuntime() { stop(); }

bool SpeechToTextRuntime::start() {
    if (running_) return true;
    setState(SttState::STARTING); lastError_.clear();
    std::cout << "[STT] Trying Python faster-whisper daemon...\n";
    if (launchPythonDaemon()) {
        usingPythonDaemon_=true; running_=true; setState(SttState::LISTENING);
        std::cout << "[STT] Python STT daemon active (faster-whisper base.en + WebRTC VAD).\n";
        workerThread_ = std::thread(&SpeechToTextRuntime::pythonReadLoop, this);
        return true;
    }
    std::cout << "[STT] Python daemon unavailable, falling back to whisper.cpp...\n";
    static const std::string MODEL_PATH = "data/models/whisper/ggml-tiny.en.bin";
    std::cout << "[STT Runtime] Attempting to load STT model: " << MODEL_PATH << "\n";
    setState(SttState::LOADING_MODEL);
    WhisperModelStatus status = whisper_.loadModel(MODEL_PATH);
    switch (status) {
        case WhisperModelStatus::READY:
            usingPythonDaemon_=false; running_=true;
            setState(SttState::READY); setState(SttState::LISTENING);
            std::cout << "[STT Runtime] [CHECKPOINT 3] STT runtime thread starting.\n";
            workerThread_ = std::thread(&SpeechToTextRuntime::runLoop, this);
            return true;
        default:
            lastError_=whisper_.getLastError(); setState(SttState::FAILED);
            std::cerr << "[STT Runtime] STT DISABLED: " << whisperModelStatusStr(status) << "\n";
            return false;
    }
}

void SpeechToTextRuntime::stop() {
    if (running_) {
        setState(SttState::STOPPING); running_=false;
        if (usingPythonDaemon_) stopPythonDaemon();
        if (workerThread_.joinable()) workerThread_.join();
    }
    if (!usingPythonDaemon_) whisper_.unloadModel();
    setState(SttState::STOPPED);
}

bool SpeechToTextRuntime::launchPythonDaemon() {
    const std::string script = "data\\stt\\yuki_stt_daemon.py";
    DWORD attr = GetFileAttributesA(script.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) { std::cerr << "[STT] Daemon script not found: " << script << "\n"; return false; }
    SECURITY_ATTRIBUTES sa{}; sa.nLength=sizeof(sa); sa.bInheritHandle=TRUE;
    HANDLE hChildReadStdout=INVALID_HANDLE_VALUE, hChildWriteStdout=INVALID_HANDLE_VALUE;
    HANDLE hChildReadStdin=INVALID_HANDLE_VALUE,  hChildWriteStdin=INVALID_HANDLE_VALUE;
    if (!CreatePipe(&hChildReadStdout,&hChildWriteStdout,&sa,0)) return false;
    if (!CreatePipe(&hChildReadStdin,&hChildWriteStdin,&sa,0)) { CloseHandle(hChildReadStdout); CloseHandle(hChildWriteStdout); return false; }
    SetHandleInformation(hChildReadStdout, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hChildWriteStdin,  HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.wShowWindow=SW_HIDE; si.hStdInput=hChildReadStdin; si.hStdOutput=hChildWriteStdout;
    si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    char cmd[512]; std::snprintf(cmd,sizeof(cmd),"python -u \"%s\"",script.c_str());
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr,cmd,nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
    CloseHandle(hChildWriteStdout); CloseHandle(hChildReadStdin);
    if (!ok) { CloseHandle(hChildReadStdout); CloseHandle(hChildWriteStdin); std::cerr<<"[STT] Failed to launch daemon (error "<<GetLastError()<<")\n"; return false; }
    hReadPipe_=hChildReadStdout; hWriteStdin_=hChildWriteStdin; hPythonProc_=pi.hProcess; hPythonThread_=pi.hThread;
    std::cout << "[STT] Waiting for Python daemon (PID=" << pi.dwProcessId << ")...\n";
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    std::string lineBuf; char ch; DWORD bytesRead, bytesAvail;
    while (std::chrono::steady_clock::now() < deadline) {
        bytesAvail=0;
        BOOL peeked = PeekNamedPipe(hReadPipe_,nullptr,0,nullptr,&bytesAvail,nullptr);
        if (!peeked) { std::cerr<<"[STT] Pipe broken\n"; break; }
        if (bytesAvail==0) { Sleep(50); continue; }
        if (!ReadFile(hReadPipe_,&ch,1,&bytesRead,nullptr)||bytesRead==0) break;
        if (ch=='\n') {
            if (lineBuf.find("\"ready\"")!=std::string::npos&&lineBuf.find("\"type\"")!=std::string::npos) { std::cout<<"[STT] Python daemon CONNECTED.\n"; return true; }
            if (lineBuf.find("\"error\"")!=std::string::npos) std::cerr<<"[STT] Daemon error: "<<lineBuf<<"\n";
            lineBuf.clear();
        } else if (ch!='\r') lineBuf+=ch;
    }
    std::cerr<<"[STT] Daemon ready-wait timed out.\n"; stopPythonDaemon(); return false;
}

void SpeechToTextRuntime::stopPythonDaemon() {
    if (hWriteStdin_!=INVALID_HANDLE_VALUE) { const char q[]="{\"cmd\":\"quit\"}\n"; DWORD w; WriteFile(hWriteStdin_,q,(DWORD)strlen(q),&w,nullptr); CloseHandle(hWriteStdin_); hWriteStdin_=INVALID_HANDLE_VALUE; }
    if (hPythonProc_!=INVALID_HANDLE_VALUE) { WaitForSingleObject(hPythonProc_,3000); TerminateProcess(hPythonProc_,0); CloseHandle(hPythonProc_); CloseHandle(hPythonThread_); hPythonProc_=hPythonThread_=INVALID_HANDLE_VALUE; }
    if (hReadPipe_!=INVALID_HANDLE_VALUE) { CloseHandle(hReadPipe_); hReadPipe_=INVALID_HANDLE_VALUE; }
}

void SpeechToTextRuntime::sendDaemonCmd(const char* jsonLine) { if (hWriteStdin_==INVALID_HANDLE_VALUE) return; DWORD w; WriteFile(hWriteStdin_,jsonLine,(DWORD)strlen(jsonLine),&w,nullptr); }

void SpeechToTextRuntime::setListening(bool listen) {
    if (!running_||!usingPythonDaemon_) return;
    if (listen) { sendDaemonCmd("{\"cmd\":\"start\"}\n"); setState(SttState::LISTENING); std::cout<<"[STT] Mic resumed.\n"; }
    else         { sendDaemonCmd("{\"cmd\":\"stop\"}\n");  setState(SttState::STOPPED);   std::cout<<"[STT] Mic paused.\n"; }
}

void SpeechToTextRuntime::pythonReadLoop() {
    try {
        std::string lineBuf; char ch; DWORD bytesRead;
        while (running_&&hReadPipe_!=INVALID_HANDLE_VALUE) {
            BOOL ok = ReadFile(hReadPipe_,&ch,1,&bytesRead,nullptr);
            if (!ok||bytesRead==0) break;
            if (ch=='\n') {
                if (!lineBuf.empty()) {
                    auto extract = [&](const std::string& key) -> std::string {
                        std::string n1="\""+key+"\": \"", n2="\""+key+"\":\"";
                        size_t pos=lineBuf.find(n1), nlen=n1.size();
                        if (pos==std::string::npos){pos=lineBuf.find(n2);nlen=n2.size();}
                        if (pos==std::string::npos) return "";
                        pos+=nlen; size_t end=lineBuf.find('"',pos);
                        return (end!=std::string::npos)?lineBuf.substr(pos,end-pos):"";
                    };
                    std::string type=extract("type");
                    if (type=="partial")       { std::string t=extract("text"); if(!t.empty()) onPartial(t); }
                    else if (type=="final")    { std::string t=extract("text"); if(!t.empty()){setState(SttState::DECODING);onFinal(t);setState(SttState::LISTENING);} }
                    else if (type=="speaking_start") setState(SttState::CAPTURING_UTTERANCE);
                    else if (type=="listening")      setState(SttState::LISTENING);
                    else if (type=="error")    { std::cerr<<"[STT Daemon] "<<extract("msg")<<"\n"; }
                    lineBuf.clear();
                }
            } else if (ch!='\r') lineBuf+=ch;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[SpeechToTextRuntime] pythonReadLoop exception: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "[SpeechToTextRuntime] pythonReadLoop unknown exception\n";
    }
    std::cout<<"[STT] Python daemon reader loop exited.\n";
}

void SpeechToTextRuntime::onPartial(const std::string& text) {
    std::string norm=normalizePartial(text); TranscriptCallback cb;
    { std::lock_guard<std::mutex> lock(mutex_); latestPartialText_=norm; ++partialVersion_; partialDirty_=true; cb=partialCallback_; }
    if (cb) cb(norm);
}
void SpeechToTextRuntime::onFinal(const std::string& text) {
    std::string norm=trimCopy(text); if (norm.empty()) return;
    TranscriptCallback cb;
    { std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(norm); latestPartialText_.clear(); partialDirty_=false; cb=transcriptCallback_; }
    if (cb) cb(norm);
}
void SpeechToTextRuntime::setState(SttState s) { state_=s; subsystems_.setSttState(s); }

SttState    SpeechToTextRuntime::getState() const     { return state_; }
bool        SpeechToTextRuntime::isDecoding() const   { return state_==SttState::DECODING; }
std::string SpeechToTextRuntime::getLastError() const { std::lock_guard<std::mutex> lock(mutex_); return lastError_; }
WhisperModelStatus SpeechToTextRuntime::getModelStatus() const { return whisper_.getModelStatus(); }

std::vector<std::string> SpeechToTextRuntime::consumeFinishedTexts() {
    std::lock_guard<std::mutex> lock(mutex_); partialDirty_=false;
    std::vector<std::string> texts=std::move(finishedTexts_); finishedTexts_.clear(); return texts;
}
std::string SpeechToTextRuntime::getLatestPartialText() const { std::lock_guard<std::mutex> lock(mutex_); return latestPartialText_; }
bool        SpeechToTextRuntime::hasNewPartialText() const    { std::lock_guard<std::mutex> lock(mutex_); return partialDirty_; }
uint64_t    SpeechToTextRuntime::getPartialVersion() const    { std::lock_guard<std::mutex> lock(mutex_); return partialVersion_; }
void        SpeechToTextRuntime::setTranscriptCallback(TranscriptCallback cb)        { std::lock_guard<std::mutex> lock(mutex_); transcriptCallback_=cb; }
void        SpeechToTextRuntime::setPartialTranscriptCallback(TranscriptCallback cb) { std::lock_guard<std::mutex> lock(mutex_); partialCallback_=cb; }

void SpeechToTextRuntime::runLoop() {
    try {
        const double pollIntervalMs=100.0, partialDecodeEveryMs=500.0;
        const size_t partialWindowSamples=16000*4;
        double partialDecodeTimerMs=0.0;
        UtteranceState utteranceState=UtteranceState::IDLE;
        double silenceTimerMs=0.0, speechTimerMs=0.0, primedTimerMs=0.0;
        const double rmsThreshold=25.0, minSpeechMs=300.0, minSilenceMs=800.0;
        const double noiseBurstMs=150.0, maxUtteranceMs=15000.0;
        const size_t preRollSamples=4800;
        ear_.drainPCM(); size_t lastPolledSize=0;

        while (running_) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(pollIntervalMs)));
                if (!subsystems_.isActive(SubsystemName::EAR)) { ear_.drainPCM(); utteranceState=UtteranceState::IDLE; lastPolledSize=0; continue; }
                double vol=ear_.getLatestVolume(); bool hearsSound=(vol>rmsThreshold);
                switch (utteranceState) {
                    case UtteranceState::IDLE:
                        ear_.drainPCM(preRollSamples); lastPolledSize=ear_.getBufferedPCMCopy().size();
                        if (hearsSound) { utteranceState=UtteranceState::PRIMED; primedTimerMs=0.0; }
                        break;
                    case UtteranceState::PRIMED:
                        primedTimerMs+=pollIntervalMs;
                        if (hearsSound) {
                            if (primedTimerMs>=noiseBurstMs) {
                                utteranceState=UtteranceState::IN_SPEECH; speechTimerMs=primedTimerMs; silenceTimerMs=0.0; partialDecodeTimerMs=0.0;
                                std::cout<<"[STT Runtime] [CHECKPOINT 5] Utterance started (RMS: "<<vol<<").\n";
                                setState(SttState::CAPTURING_UTTERANCE);
                            }
                        } else { ear_.drainPCM(preRollSamples); utteranceState=UtteranceState::IDLE; lastPolledSize=ear_.getBufferedPCMCopy().size(); }
                        break;
                    case UtteranceState::IN_SPEECH:
                        speechTimerMs+=pollIntervalMs;
                        if (hearsSound) silenceTimerMs=0.0; else silenceTimerMs+=pollIntervalMs;
                        { std::vector<short> cur=ear_.getBufferedPCMCopy();
                          if (cur.size()>lastPolledSize) { std::cout<<"[STT Runtime] [CHECKPOINT 4] Drained "<<(cur.size()-lastPolledSize)<<" PCM samples.\n"; lastPolledSize=cur.size(); } }
                        partialDecodeTimerMs+=pollIntervalMs;
                        if (partialDecodeTimerMs>=partialDecodeEveryMs) {
                            partialDecodeTimerMs=0.0;
                            std::vector<short> partialPcm=ear_.readLatestPCMWindow(partialWindowSamples);
                            if (!partialPcm.empty()) {
                                setState(SttState::DECODING);
                                std::vector<float> fSamples; fSamples.reserve(partialPcm.size());
                                for (short s : partialPcm) fSamples.push_back(static_cast<float>(s)/32768.0f);
                                std::string partialText=normalizePartial(whisper_.transcribePartial(fSamples));
                                TranscriptCallback partCb;
                                { std::lock_guard<std::mutex> lock(mutex_);
                                  if (!partialText.empty()&&partialText!=latestPartialText_) { latestPartialText_=partialText; ++partialVersion_; partialDirty_=true; partCb=partialCallback_; } }
                                if (partCb&&!partialText.empty()) partCb(partialText);
                                setState(SttState::CAPTURING_UTTERANCE);
                            }
                        }
                        if (silenceTimerMs>=minSilenceMs||speechTimerMs>=maxUtteranceMs) utteranceState=UtteranceState::READY_TO_DECODE;
                        break;
                    case UtteranceState::READY_TO_DECODE: break;
                }
                if (utteranceState==UtteranceState::READY_TO_DECODE) {
                    std::cout<<"[STT Runtime] [CHECKPOINT 6] Utterance finalized. Length: "<<speechTimerMs<<" ms.\n";
                    std::vector<short> utteranceBuffer=ear_.drainPCM(0);
                    if (speechTimerMs>=minSpeechMs&&!utteranceBuffer.empty()) {
                        setState(SttState::DECODING);
                        std::vector<float> fSamples; fSamples.reserve(utteranceBuffer.size());
                        for (short s : utteranceBuffer) fSamples.push_back(static_cast<float>(s)/32768.0f);
                        std::string text=normalizePartial(whisper_.transcribe(fSamples));
                        TranscriptCallback finalCb;
                        { std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(text); latestPartialText_.clear(); ++partialVersion_; partialDirty_=true; finalCb=transcriptCallback_; }
                        if (finalCb&&!text.empty()) finalCb(text);
                        setState(SttState::LISTENING);
                    } else {
                        std::lock_guard<std::mutex> lock(mutex_); latestPartialText_.clear(); ++partialVersion_; partialDirty_=true;
                        setState(SttState::LISTENING);
                    }
                    utteranceState=UtteranceState::IDLE; lastPolledSize=0;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[SpeechToTextRuntime] runLoop iteration exception: " << e.what() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...) {
                std::cerr << "[SpeechToTextRuntime] runLoop iteration unknown exception\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        // Flush final utterance on shutdown
        if (utteranceState==UtteranceState::IN_SPEECH&&speechTimerMs>=minSpeechMs) {
            std::vector<short> buf=ear_.drainPCM(0);
            if (!buf.empty()) {
                std::vector<float> fSamples; fSamples.reserve(buf.size());
                for (short s : buf) fSamples.push_back(static_cast<float>(s)/32768.0f);
                std::string text=normalizePartial(whisper_.transcribe(fSamples));
                TranscriptCallback finalCb;
                { std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(text); finalCb=transcriptCallback_; }
                if (finalCb&&!text.empty()) finalCb(text);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[SpeechToTextRuntime] runLoop exception: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "[SpeechToTextRuntime] runLoop unknown exception\n";
    }
}
