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
  - Non-blocking async queue with revision tracking & pre-roll audio recovery
  - Streams PARTIAL transcripts and FINAL transcript JSON to STDOUT
"""

import sys
import json
import time
import threading
import queue
import collections
import itertools
import os
import struct
import numpy as np

# ── Emit helpers ────────────────────────────────────────────────────────────

_emit_lock = threading.Lock()

def _emit(obj: dict):
    with _emit_lock:
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

VAD_AGGRESSIVENESS = 1         # 0-3: lower = less aggressive filtering, catches soft consonants

MODEL_DIR  = "data/models/stt"
MODEL_NAME = "base.en"         # Much better than tiny.en, still very fast

PARTIAL_QUEUE_MAX = 2
FINAL_QUEUE_MAX = 8
PREROLL_FRAMES = 8  # 8 * 30ms = 240ms

# Energy threshold for fallback VAD (normalized to match C++ 25.0 raw RMS scale)
# C++ uses raw 16-bit RMS (0-32768), Python computes float RMS (0-32768)
# So the threshold should be the same: 25.0
FALLBACK_ENERGY_THRESHOLD = 25.0

# ── Audio & Inference queues / Revision State ────────────────────────────────

_audio_q: "queue.Queue[np.ndarray]" = queue.Queue(maxsize=2000)
_infer_queue = queue.Queue(maxsize=FINAL_QUEUE_MAX)
_revision_counter = itertools.count(1)
_latest_partial_revision = 0
_latest_final_revision = 0
_worker_thread = None
_running = threading.Event()
_listening = threading.Event()

# Model loading synchronization
_model = None
_model_lock = threading.Lock()
_model_loaded = threading.Event()

# ── Queue helper ─────────────────────────────────────────────────────────────

def _queue_infer_job(pcm_chunk, is_final, revision):
    global _infer_queue
    try:
        _infer_queue.put_nowait((pcm_chunk, is_final, revision, time.time()))
        return True
    except queue.Full:
        if is_final:
            try:
                dropped = _infer_queue.get_nowait()
            except queue.Empty:
                return False
            try:
                _infer_queue.put_nowait((pcm_chunk, is_final, revision, time.time()))
                return True
            except queue.Full:
                return False
        return False

# ── Load faster-whisper ──────────────────────────────────────────────────────

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
        _model_loaded.set()  # Signal that model is ready
        _emit({"type": "model_ready", "model": MODEL_NAME})
        return True
    except Exception as e:
        _err(f"faster-whisper load failed: {e}")
        _model_loaded.set()  # Still set event to avoid deadlock
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
            # Filter hallucinations
            if t and t not in ("[BLANK_AUDIO]", "(Music)", "(Applause)",
                               "[music]", "[Music]", "(music)"):
                parts.append(t)
        return " ".join(parts).strip()
    except Exception as e:
        _err(f"Transcription error: {e}")
        return ""

def _transcription_worker():
    global _latest_partial_revision, _latest_final_revision
    last_partial_text = ""
    while _running.is_set():
        try:
            pcm_chunk, is_final, revision, queued_at = _infer_queue.get(timeout=0.05)
        except queue.Empty:
            continue

        if pcm_chunk is None or len(pcm_chunk) == 0:
            continue

        if not is_final and revision < _latest_partial_revision:
            continue
        if is_final and revision < _latest_final_revision:
            continue

        # Wait for model to be loaded (with timeout)
        if not _model_loaded.wait(timeout=5.0):
            _err("Model loading timed out, dropping transcription job")
            continue

        text = _transcribe(pcm_chunk)
        if not text:
            continue

        now_ts = round(time.time(), 3)

        if is_final:
            if revision < _latest_final_revision:
                continue
            _latest_final_revision = revision
            _latest_partial_revision = max(_latest_partial_revision, revision)
            _emit({
                "type": "final",
                "text": text,
                "revision": revision,
                "latency_ms": int((time.time() - queued_at) * 1000),
                "ts": now_ts,
            })
            last_partial_text = ""
        else:
            if revision < _latest_partial_revision:
                continue
            if text == last_partial_text:
                continue
            _latest_partial_revision = revision
            last_partial_text = text
            _emit({
                "type": "partial",
                "text": text,
                "revision": revision,
                "latency_ms": int((time.time() - queued_at) * 1000),
                "ts": now_ts,
            })

# ── VAD + Streaming loop ──────────────────────────────────────────────────────

def _stt_loop():
    """
    Main STT loop.
    - Reads audio frames from _audio_q
    - Uses WebRTC VAD to detect speech vs silence
    - Accumulates speech frames with pre-roll recovery
    - Pushes non-blocking tasks to _infer_queue
    """
    global _stream
    vad = _make_vad()
    speech_frames = []          # accumulated PCM frames during utterance
    preroll_buffer = collections.deque(maxlen=PREROLL_FRAMES)  # 240ms rolling buffer
    in_speech = False
    silence_start = 0.0
    last_partial_time = 0.0
    speech_start_time = 0.0
    last_audio_time = time.monotonic()
    silence_frames = 0

    # Wait for model to be loaded before starting to process audio
    _emit({"type": "status", "msg": "Waiting for model to load..."})
    if not _model_loaded.wait(timeout=30.0):
        _err("Model loading timed out — STT loop cannot start")
        return

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
                    final_pcm = np.concatenate(speech_frames).astype(np.int16, copy=False)
                    final_revision = next(_revision_counter)
                    _queue_infer_job(final_pcm, True, final_revision)
                    in_speech = False
                    speech_frames = []
                    silence_frames = 0
                    last_partial_time = 0.0
            continue

        # frame is shape (FRAME_SAMPLES, 1) — flatten to 1D
        pcm = frame.flatten()
        preroll_buffer.append(pcm.copy())

        # VAD decision
        is_speech = False
        if vad is not None:
            try:
                pcm_bytes = pcm.tobytes()
                is_speech = vad.is_speech(pcm_bytes, SAMPLE_RATE)
            except Exception:
                energy = float(np.sqrt(np.mean(pcm.astype(np.float32) ** 2)))
                is_speech = energy > FALLBACK_ENERGY_THRESHOLD
        else:
            energy = float(np.sqrt(np.mean(pcm.astype(np.float32) ** 2)))
            is_speech = energy > FALLBACK_ENERGY_THRESHOLD

        now = time.monotonic()

        if is_speech:
            if not in_speech:
                in_speech = True
                speech_frames = list(preroll_buffer)
                silence_frames = 0
                utterance_revision = next(_revision_counter)
                speech_start_time = now
                last_partial_time = now
                _emit({"type": "speaking_start"})

            speech_frames.append(pcm)
            silence_start = now

            speech_duration = now - speech_start_time
            since_partial = now - last_partial_time

            if in_speech and since_partial >= PARTIAL_INTERVAL and len(speech_frames) > 5:
                partial_pcm = np.concatenate(speech_frames).astype(np.int16, copy=False)
                current_revision = next(_revision_counter)
                _queue_infer_job(partial_pcm, False, current_revision)
                last_partial_time = now

            if speech_duration >= MAX_SPEECH_SEC:
                final_pcm = np.concatenate(speech_frames).astype(np.int16, copy=False)
                final_revision = next(_revision_counter)
                _queue_infer_job(final_pcm, True, final_revision)
                speech_frames = []
                in_speech = False
                silence_frames = 0
                last_partial_time = 0.0

        else:
            if in_speech:
                speech_frames.append(pcm)   # include trailing silence
                silence_frames += 1
                elapsed_silence = now - silence_start

                if elapsed_silence >= SILENCE_TIMEOUT and len(speech_frames) > 0:
                    speech_ms = (now - speech_start_time) * 1000
                    if speech_ms >= MIN_SPEECH_MS:
                        final_pcm = np.concatenate(speech_frames).astype(np.int16, copy=False)
                        final_revision = next(_revision_counter)
                        _queue_infer_job(final_pcm, True, final_revision)
                    speech_frames = []
                    in_speech = False
                    silence_frames = 0
                    last_partial_time = 0.0

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
    global _stream, _worker_thread
    _emit({"type": "starting", "msg": "Yuki STT daemon initialising..."})

    _stream = _start_stream()
    if _stream is None:
        _err("No microphone found — STT daemon cannot start.")
        sys.exit(1)

    _emit({"type": "ready", "backend": "faster-whisper",
           "model": MODEL_NAME, "compute": "int8/CPU"})

    _running.set()
    _listening.set()

    # Start background transcription worker thread
    _worker_thread = threading.Thread(target=_transcription_worker, name="yuki-stt-worker", daemon=True)
    _worker_thread.start()

    # Start model loading FIRST, wait for it to complete
    model_thread = threading.Thread(target=_load_model, name="yuki-stt-model-loader", daemon=True)
    model_thread.start()
    
    # Wait for model to be ready before starting STT loop
    if not _model_loaded.wait(timeout=30.0):
        _err("Model loading timed out — exiting")
        _running.clear()
        sys.exit(1)

    # Now start the STT loop (model is guaranteed loaded)
    stt_thread = threading.Thread(target=_stt_loop, name="yuki-stt-loop", daemon=True)
    stt_thread.start()

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
        if _worker_thread is not None and _worker_thread.is_alive():
            _worker_thread.join(timeout=1.0)
        stt_thread.join(timeout=2.0)
        model_thread.join(timeout=2.0)


if __name__ == "__main__":
    main()