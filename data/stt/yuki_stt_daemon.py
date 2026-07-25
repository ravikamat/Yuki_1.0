"""
yuki_stt_daemon.py
Yuki_1.0 — Real-Time Speech-to-Text Daemon

Architecture:
  - Owns the microphone entirely (no conflict with C++ Ear)
  - Captures audio via sounddevice (Windows WASAPI)
  - WebRTC VAD for precise speech/silence detection (10ms frames)
  - faster-whisper (CTranslate2 backend) for transcription
    Model: base.en (~145 MB) — 4-6x faster than whisper.cpp tiny
    with better accuracy
  - Streams PARTIAL transcripts every ~300ms while speaking
  - Sends FINAL transcript on confirmed silence (600ms gap)
  - C++ reads JSON lines from STDOUT and fires UI callbacks

Commands on STDIN (JSON lines):
  {"cmd": "start"}   — start listening
  {"cmd": "stop"}    — pause listening
  {"cmd": "ping"}    — health check
  {"cmd": "quit"}    — shutdown

Output on STDOUT (JSON lines):
  {"type": "ready",   "backend": "faster-whisper", "model": "base.en"}
  {"type": "listening"}
  {"type": "partial", "text": "hello yuki how",  "ts": 1234.5}
  {"type": "final",   "text": "Hello Yuki, how are you?", "ts": 1234.5}
  {"type": "error",   "msg": "..."}
"""

import sys
import json
import time
import threading
import queue
import os
import struct
import numpy as np

# ── Emit helpers ────────────────────────────────────────────────────────────

def _emit(obj: dict):
    line = json.dumps(obj, ensure_ascii=False)
    sys.stdout.write(line + "\n")
    sys.stdout.flush()

def _err(msg: str):
    _emit({"type": "error", "msg": str(msg)})
    sys.stderr.write("[STT] ERROR: " + str(msg) + "\n")
    sys.stderr.flush()

# ── Constants ────────────────────────────────────────────────────────────────

SAMPLE_RATE      = 16000       # Whisper expects 16kHz
CHANNELS         = 1
DTYPE            = np.int16
FRAME_DURATION   = 30          # ms — VAD frame size (10, 20, or 30ms only)
FRAME_SAMPLES    = SAMPLE_RATE * FRAME_DURATION // 1000  # 480 samples @ 30ms

SILENCE_TIMEOUT  = 0.65        # seconds of silence → commit final transcript
PARTIAL_INTERVAL = 0.30        # seconds between partial transcript emissions
MIN_SPEECH_MS    = 200         # ignore utterances shorter than this
MAX_SPEECH_SEC   = 30          # max utterance length before forced decode

VAD_AGGRESSIVENESS = 2         # 0-3: higher = more aggressive filtering

MODEL_DIR  = "data/models/stt"
MODEL_NAME = "base.en"         # Much better than tiny.en, still very fast

# ── Audio queue ─────────────────────────────────────────────────────────────

_audio_q: "queue.Queue[np.ndarray]" = queue.Queue(maxsize=2000)
_running = threading.Event()
_listening = threading.Event()

# ── Load faster-whisper ──────────────────────────────────────────────────────

_model = None
_model_lock = threading.Lock()

def _load_model():
    global _model
    try:
        from faster_whisper import WhisperModel
        os.makedirs(MODEL_DIR, exist_ok=True)
        _emit({"type": "status", "msg": f"Loading faster-whisper {MODEL_NAME}..."})
        mdl = WhisperModel(
            MODEL_NAME,
            device="cpu",
            compute_type="int8",
            download_root=MODEL_DIR,
            num_workers=2
        )
        with _model_lock:
            _model = mdl
        _emit({"type": "model_ready", "model": MODEL_NAME})
        return True
    except Exception as e:
        _err(f"faster-whisper load failed: {e}")
        return False

# ── WebRTC VAD ───────────────────────────────────────────────────────────────

def _make_vad():
    try:
        import webrtcvad
        v = webrtcvad.Vad(VAD_AGGRESSIVENESS)
        return v
    except Exception as e:
        _err(f"WebRTC VAD unavailable: {e}")
        return None

# ── Audio capture ─────────────────────────────────────────────────────────────

_stream = None

def _audio_callback(indata, frames, time_info, status):
    """sounddevice callback — called from a real-time audio thread."""
    if _listening.is_set():
        _audio_q.put_nowait(indata.copy())

def _start_stream(max_retries=5, base_delay=1.0):
    """Open the default microphone input stream with retry logic."""
    for attempt in range(max_retries):
        try:
            import sounddevice as sd
            stream = sd.InputStream(
                samplerate=SAMPLE_RATE,
                channels=CHANNELS,
                dtype="int16",
                blocksize=FRAME_SAMPLES,
                callback=_audio_callback,
                device=None,  # default mic
            )
            stream.start()
            _emit({"type": "status", "msg": f"Audio stream opened successfully (attempt {attempt+1})."})
            return stream
        except Exception as e:
            delay = min(base_delay * (2 ** attempt), 8.0)
            _err(f"Audio stream open failed (attempt {attempt+1}/{max_retries}): {e}")
            if attempt < max_retries - 1:
                _emit({"type": "status", "msg": f"Retrying audio stream connection in {delay:.1f}s..."})
                time.sleep(delay)
    return None

# ── Transcription ─────────────────────────────────────────────────────────────

def _transcribe(pcm_int16: np.ndarray) -> str:
    """Run faster-whisper on a numpy int16 PCM array. Returns cleaned text."""
    with _model_lock:
        mdl = _model
    if mdl is None:
        return ""
    try:
        # Convert int16 → float32 [-1, 1]
        audio_f32 = pcm_int16.astype(np.float32) / 32768.0
        segments, info = mdl.transcribe(
            audio_f32,
            language="en",
            beam_size=3,           # lower = faster, still accurate
            best_of=3,
            temperature=0.0,       # greedy for speed
            vad_filter=False,      # we do our own VAD
            without_timestamps=True,
            condition_on_previous_text=False,
        )
        parts = []
        for seg in segments:
            t = seg.text.strip()
            # Filter hallucinations (whisper sometimes emits these when silent)
            if t and t not in ("[BLANK_AUDIO]", "(Music)", "(Applause)",
                               "[music]", "[Music]", "(music)"):
                parts.append(t)
        return " ".join(parts).strip()
    except Exception as e:
        _err(f"Transcription error: {e}")
        return ""

# ── VAD + Streaming loop ──────────────────────────────────────────────────────

def _stt_loop():
    """
    Main STT loop.
    - Reads audio frames from _audio_q
    - Uses WebRTC VAD to detect speech vs silence
    - Accumulates speech frames
    - Emits partial transcripts every PARTIAL_INTERVAL seconds
    - Emits final transcript after SILENCE_TIMEOUT silence
    """
    global _stream
    vad = _make_vad()
    speech_frames = []          # accumulated PCM frames during utterance
    in_speech = False
    silence_start = 0.0
    last_partial_time = 0.0
    speech_start_time = 0.0
    last_partial_text = ""
    last_audio_time = time.monotonic()

    _emit({"type": "listening"})

    while _running.is_set():
        if not _listening.is_set():
            time.sleep(0.05)
            last_audio_time = time.monotonic()
            continue

        # Drain the audio queue in batches
        try:
            frame = _audio_q.get(timeout=0.05)
            last_audio_time = time.monotonic()
        except queue.Empty:
            if _listening.is_set() and (time.monotonic() - last_audio_time > 5.0):
                _emit({"type": "status", "msg": "STT Daemon audio stall detected. Reconnecting..."})
                try:
                    if _stream is not None:
                        _stream.stop()
                        _stream.close()
                except Exception:
                    pass
                try:
                    _stream = _start_stream(max_retries=3, base_delay=1.0)
                    last_audio_time = time.monotonic()
                    # Discard stale queue items
                    while not _audio_q.empty():
                        try:
                            _audio_q.get_nowait()
                        except queue.Empty:
                            break
                except Exception as e:
                    _err(f"STT Daemon reconnection failed: {e}")
                    time.sleep(2.0)

            # Check for silence timeout if we have accumulated speech
            if in_speech and speech_frames:
                elapsed_silence = time.monotonic() - silence_start
                if elapsed_silence >= SILENCE_TIMEOUT:
                    _commit_utterance(speech_frames, last_partial_text)
                    speech_frames = []
                    in_speech = False
                    last_partial_text = ""
            continue

        # frame is shape (FRAME_SAMPLES, 1) — flatten to 1D
        pcm = frame.flatten()

        # VAD decision
        is_speech = False
        if vad is not None:
            try:
                # webrtcvad needs bytes of int16 samples
                pcm_bytes = pcm.tobytes()
                is_speech = vad.is_speech(pcm_bytes, SAMPLE_RATE)
            except Exception:
                # Energy fallback
                energy = float(np.sqrt(np.mean(pcm.astype(np.float32) ** 2)))
                is_speech = energy > 300.0
        else:
            energy = float(np.sqrt(np.mean(pcm.astype(np.float32) ** 2)))
            is_speech = energy > 300.0

        now = time.monotonic()

        if is_speech:
            if not in_speech:
                in_speech = True
                speech_start_time = now
                last_partial_time = now
                _emit({"type": "speaking_start"})

            speech_frames.append(pcm)
            silence_start = now

            # Emit partial transcript every PARTIAL_INTERVAL
            speech_duration = now - speech_start_time
            since_partial = now - last_partial_time

            if since_partial >= PARTIAL_INTERVAL and len(speech_frames) > 5:
                # Transcribe what we have so far (non-blocking quick pass)
                partial_pcm = np.concatenate(speech_frames)
                partial_text = _transcribe(partial_pcm)
                if partial_text and partial_text != last_partial_text:
                    last_partial_text = partial_text
                    _emit({"type": "partial", "text": partial_text,
                           "ts": round(time.time(), 3)})
                last_partial_time = now

            # Force-commit if utterance too long
            if speech_duration >= MAX_SPEECH_SEC:
                _commit_utterance(speech_frames, last_partial_text)
                speech_frames = []
                in_speech = False
                last_partial_text = ""

        else:
            if in_speech:
                speech_frames.append(pcm)   # include trailing silence
                elapsed_silence = now - silence_start

                if elapsed_silence >= SILENCE_TIMEOUT:
                    # Silence confirmed → commit final transcript
                    speech_ms = (now - speech_start_time) * 1000
                    if speech_ms >= MIN_SPEECH_MS:
                        _commit_utterance(speech_frames, last_partial_text)
                    speech_frames = []
                    in_speech = False
                    last_partial_text = ""

def _commit_utterance(speech_frames, last_partial):
    """Transcribe accumulated frames and emit a final transcript."""
    if not speech_frames:
        return
    pcm = np.concatenate(speech_frames)
    final_text = _transcribe(pcm)
    if final_text:
        _emit({"type": "final", "text": final_text, "ts": round(time.time(), 3)})
    elif last_partial:
        # Use the last partial if transcription came up empty on final pass
        _emit({"type": "final", "text": last_partial, "ts": round(time.time(), 3)})

# ── Command listener ─────────────────────────────────────────────────────────

def _cmd_loop():
    """Read JSON commands from STDIN. Runs in main thread."""
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            cmd = json.loads(raw)
        except Exception:
            continue

        action = cmd.get("cmd", "")

        if action == "ping":
            _emit({"type": "pong", "listening": _listening.is_set(),
                   "ts": round(time.time(), 3)})

        elif action == "start":
            _listening.set()
            _emit({"type": "listening"})

        elif action == "stop":
            _listening.clear()
            _emit({"type": "stopped"})

        elif action == "quit":
            _running.clear()
            _listening.clear()
            _emit({"type": "bye"})
            break

# ── Entry point ─────────────────────────────────────────────────────────────

def main():
    global _stream
    _emit({"type": "starting", "msg": "Yuki STT daemon initialising..."})

    # Open mic FIRST — this is fast (~50ms)
    _stream = _start_stream()
    if _stream is None:
        _err("No microphone found — STT daemon cannot start.")
        sys.exit(1)

    # Signal C++ that we are alive and the mic is open.
    # The model loads in the background — STT loop handles model=None
    # gracefully (returns empty string until model is ready).
    _emit({"type": "ready", "backend": "faster-whisper",
           "model": MODEL_NAME, "compute": "int8/CPU"})

    # Start background STT loop immediately (captures audio, waits for model)
    _running.set()
    _listening.set()
    stt_thread = threading.Thread(target=_stt_loop, daemon=True)
    stt_thread.start()

    # Load model in background thread — emits 'model_ready' when done
    model_thread = threading.Thread(target=_load_model, daemon=True)
    model_thread.start()

    # Block main thread on command loop (reads STDIN from C++)
    try:
        _cmd_loop()
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        _running.clear()
        _listening.clear()
        if _stream is not None:
            try:
                _stream.stop()
                _stream.close()
            except Exception:
                pass
        stt_thread.join(timeout=2.0)
        model_thread.join(timeout=2.0)


if __name__ == "__main__":
    main()
