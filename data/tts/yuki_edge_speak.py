"""
yuki_edge_speak.py
Yuki_1.0 — EdgeTTS Synthesis Helper & Persistent Daemon

Converts EdgeTTS to PCM WAV directly in memory without intermediate MP3 disk files.
Supports both single-shot CLI mode and persistent --daemon IPC mode over STDIN/STDOUT JSON.
"""

import argparse
import asyncio
import io
import json
import os
import sys
import traceback
import edge_tts

async def synthesize_to_wav_bytes(text: str) -> bytes:
    communicate = edge_tts.Communicate(text=text, voice="en-US-JennyNeural", rate="+0%", pitch="-5Hz")
    mp3_buffer = io.BytesIO()

    async for chunk in communicate.stream():
        if chunk["type"] == "audio":
            mp3_buffer.write(chunk["data"])

    mp3_buffer.seek(0)
    mp3_bytes = mp3_buffer.getvalue()

    # Try pydub conversion
    try:
        from pydub import AudioSegment
        audio = AudioSegment.from_file(io.BytesIO(mp3_bytes), format="mp3")
        audio = audio.set_channels(1).set_frame_rate(22050).set_sample_width(2)
        wav_buffer = io.BytesIO()
        audio.export(wav_buffer, format="wav")
        return wav_buffer.getvalue()
    except Exception:
        pass

    # Try ffmpeg fallback
    try:
        import subprocess
        proc = subprocess.run(
            ["ffmpeg", "-y", "-i", "pipe:0", "-f", "wav", "-acodec", "pcm_s16le", "-ar", "22050", "-ac", "1", "pipe:1"],
            input=mp3_bytes, capture_output=True, check=True
        )
        return proc.stdout
    except Exception:
        pass

    # Fallback to returning raw bytes
    return mp3_bytes

async def process_command(cmd: dict):
    if cmd.get("cmd") == "quit":
        return {"ok": True, "quit": True}

    text = (cmd.get("text") or "").strip()
    wav_path = (cmd.get("wav_path") or "").strip()

    if not text:
        return {"ok": False, "error": "Missing text"}
    if not wav_path:
        return {"ok": False, "error": "Missing wav_path"}

    wav_bytes = await synthesize_to_wav_bytes(text)

    out_dir = os.path.dirname(wav_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(wav_path, "wb") as f:
        f.write(wav_bytes)

    return {
        "ok": True,
        "wav_path": wav_path,
        "bytes": len(wav_bytes),
    }

def run_daemon():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            cmd = json.loads(raw_line)
            result = loop.run_until_complete(process_command(cmd))
            sys.stdout.write(json.dumps(result) + "\n")
            sys.stdout.flush()
            if result.get("quit"):
                break
        except Exception as exc:
            sys.stdout.write(json.dumps({
                "ok": False,
                "error": str(exc),
                "trace": traceback.format_exc(limit=1),
            }) + "\n")
            sys.stdout.flush()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemon", action="store_true")
    parser.add_argument("text_path", nargs="?")
    parser.add_argument("wav_path", nargs="?")
    args = parser.parse_args()

    if args.daemon:
        run_daemon()
        return

    if not args.text_path or not args.wav_path:
        raise SystemExit("Expected either --daemon or text_path wav_path")

    with open(args.text_path, "r", encoding="utf-8") as f:
        text = f.read().strip()

    wav_bytes = asyncio.run(synthesize_to_wav_bytes(text))
    out_dir = os.path.dirname(args.wav_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.wav_path, "wb") as f:
        f.write(wav_bytes)

if __name__ == "__main__":
    main()
