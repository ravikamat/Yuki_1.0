"""
yuki_tts_server.py
Yuki_1.0 — Neural TTS Daemon

Uses Microsoft Edge TTS (edge-tts) for near-human quality speech synthesis.
Voice: en-US-JennyNeural (warm, natural female voice)

Protocol: stdin/stdout JSON lines (same pattern as vision server)
Commands:
  {"cmd": "speak", "text": "...", "out": "path/to/output.wav"}
  {"cmd": "ping"}
  {"cmd": "quit"}
"""

import sys
import json
import time
import asyncio
import os
import subprocess
import tempfile

def _emit(obj: dict):
    line = json.dumps(obj, ensure_ascii=False)
    sys.stdout.write(line + "\n")
    sys.stdout.flush()

def _emit_error(msg: str):
    _emit({"ok": False, "error": msg})

# ── EdgeTTS synthesis ──────────────────────────────────────────────────────
# Voice options ranked by naturalness:
#   en-US-JennyNeural      — warm, conversational female
#   en-US-AriaNeural       — expressive female
#   en-US-SaraNeural       — gentle female
#   en-US-GuyNeural        — natural male
#   en-GB-SoniaNeural      — British female
VOICE = "en-US-JennyNeural"

# SSML rate/pitch tweaks for more natural delivery
RATE  = "+0%"    # normal rate
PITCH = "-5Hz"   # slightly warmer

async def _edge_speak_async(text: str, out_mp3: str) -> bool:
    """Use edge-tts to synthesize text to an MP3 file."""
    try:
        import edge_tts
        tts = edge_tts.Communicate(
            text,
            voice=VOICE,
            rate=RATE,
            pitch=PITCH
        )
        await tts.save(out_mp3)
        return os.path.exists(out_mp3) and os.path.getsize(out_mp3) > 100
    except Exception as e:
        _emit_error(f"edge-tts failed: {e}")
        return False

def _mp3_to_wav(mp3_path: str, wav_path: str) -> bool:
    """Convert MP3 to WAV using ffmpeg or Windows built-in."""
    # Try ffmpeg first (best quality)
    try:
        result = subprocess.run(
            ["ffmpeg", "-y", "-i", mp3_path, "-acodec", "pcm_s16le",
             "-ar", "22050", "-ac", "1", wav_path],
            capture_output=True, timeout=15
        )
        if result.returncode == 0 and os.path.exists(wav_path):
            return True
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Fallback: use Python soundfile / pydub if available
    try:
        from pydub import AudioSegment
        audio = AudioSegment.from_mp3(mp3_path)
        audio = audio.set_channels(1).set_frame_rate(22050)
        audio.export(wav_path, format="wav")
        return os.path.exists(wav_path)
    except Exception:
        pass

    # Last resort: direct WAV from edge-tts (it actually outputs MP3, so we
    # return the MP3 path and let the C++ side play it if it supports MP3)
    # Just copy as-is and signal the format
    return False

def synthesize(text: str, out_path: str) -> dict:
    """Synthesize text → WAV file at out_path. Returns status dict."""
    if not text or not text.strip():
        return {"ok": False, "error": "Empty text"}

    os.makedirs(os.path.dirname(out_path) if os.path.dirname(out_path) else ".", exist_ok=True)

    # Step 1: edge-tts → MP3
    mp3_path = out_path.replace(".wav", ".mp3")
    if not mp3_path.endswith(".mp3"):
        mp3_path = out_path + ".mp3"

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    ok = loop.run_until_complete(_edge_speak_async(text, mp3_path))
    loop.close()

    if not ok:
        return {"ok": False, "error": "edge-tts synthesis failed"}

    # Step 2: MP3 → WAV
    wav_ok = _mp3_to_wav(mp3_path, out_path)

    # If WAV conversion failed, provide the MP3 path as fallback
    if not wav_ok:
        # Rename MP3 to requested out_path (the C++ side uses PlaySound which
        # can play WAV; we must deliver WAV. If ffmpeg not available, signal fallback.)
        return {
            "ok": True,
            "voice": VOICE,
            "wav_path": mp3_path,       # MP3 is the best we can do without ffmpeg
            "format": "mp3",
            "warning": "ffmpeg not found — produced MP3 instead of WAV. Install ffmpeg for best results."
        }

    # Clean up mp3
    try:
        os.remove(mp3_path)
    except Exception:
        pass

    return {
        "ok": True,
        "voice": VOICE,
        "wav_path": out_path,
        "format": "wav"
    }

# ── Main loop ──────────────────────────────────────────────────────────────

def main():
    _emit({
        "ok": True,
        "type": "ready",
        "ts": float(time.time()),
        "voice": VOICE,
        "msg": "Yuki TTS Server online (Microsoft Neural Voice)"
    })

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue

        try:
            cmd = json.loads(raw_line)
        except json.JSONDecodeError as e:
            _emit_error(f"Invalid JSON: {e}")
            continue

        action = cmd.get("cmd", "")

        if action == "ping":
            _emit({"ok": True, "type": "pong", "ts": float(time.time()), "voice": VOICE})

        elif action == "speak":
            text    = cmd.get("text", "")
            out     = cmd.get("out",  "data/tts/yuki_speech.wav")
            result  = synthesize(text, out)
            _emit(result)

        elif action == "quit":
            _emit({"ok": True, "type": "bye"})
            break

        else:
            _emit_error(f"Unknown command: {action}")

if __name__ == "__main__":
    main()
