"""
yuki_edge_speak.py
EdgeTTS synthesis helper — called by C++ MouthRuntime.
Usage: python yuki_edge_speak.py <input_text_file> <output_wav_file>

Converts TTS to WAV using:
  1. pydub (primary, no ffmpeg needed)
  2. ffmpeg (secondary)
  3. Raw MP3 copy (last resort)
"""
import sys
import asyncio
import os
import edge_tts

def main():
    if len(sys.argv) < 3:
        print("Usage: yuki_edge_speak.py <text_file> <wav_output>", file=sys.stderr)
        sys.exit(1)

    text_file = sys.argv[1]
    wav_path  = sys.argv[2]
    mp3_path  = wav_path + ".mp3"

    # Read text
    with open(text_file, "r", encoding="utf-8") as f:
        text = f.read().strip()

    if not text:
        print("Empty text", file=sys.stderr)
        sys.exit(1)

    # Step 1: Synthesize to MP3 via EdgeTTS (Microsoft Neural Voice)
    async def _synth():
        tts = edge_tts.Communicate(
            text,
            voice="en-US-JennyNeural",
            rate="+0%",
            pitch="-5Hz"
        )
        await tts.save(mp3_path)

    asyncio.run(_synth())

    if not os.path.exists(mp3_path) or os.path.getsize(mp3_path) < 100:
        print("EdgeTTS MP3 synthesis failed", file=sys.stderr)
        sys.exit(2)

    # Step 2: Convert MP3 -> PCM WAV
    converted = False

    # Strategy A: pydub (works on Windows without ffmpeg for MP3 decode)
    try:
        from pydub import AudioSegment
        audio = AudioSegment.from_mp3(mp3_path)
        audio = audio.set_channels(1).set_frame_rate(22050).set_sample_width(2)
        audio.export(wav_path, format="wav")
        if os.path.exists(wav_path) and os.path.getsize(wav_path) > 44:
            converted = True
            print(f"pydub OK: {os.path.getsize(wav_path)} bytes")
    except Exception as e:
        print(f"pydub failed: {e}", file=sys.stderr)

    # Strategy B: ffmpeg
    if not converted:
        try:
            import subprocess
            r = subprocess.run(
                ["ffmpeg", "-y", "-i", mp3_path,
                 "-acodec", "pcm_s16le", "-ar", "22050", "-ac", "1", wav_path],
                capture_output=True, timeout=15
            )
            if r.returncode == 0 and os.path.exists(wav_path) and os.path.getsize(wav_path) > 44:
                converted = True
                print(f"ffmpeg OK: {os.path.getsize(wav_path)} bytes")
        except Exception as e:
            print(f"ffmpeg failed: {e}", file=sys.stderr)

    # Strategy C: raw MP3 copy (C++ PlaySound won't play it but avoids a crash)
    if not converted:
        with open(mp3_path, "rb") as src, open(wav_path, "wb") as dst:
            dst.write(src.read())
        print("fallback: raw MP3 copy as WAV placeholder", file=sys.stderr)

    sys.exit(0)


if __name__ == "__main__":
    main()
