"""
Test all EdgeTTS -> WAV conversion approaches
"""
import asyncio
import edge_tts
import os
import sys

print("Python:", sys.version)
print("edge_tts:", edge_tts.__version__)

TEXT = "Hello, I am Yuki, your AI assistant."

async def test_mp3():
    tts = edge_tts.Communicate(TEXT, voice="en-US-JennyNeural")
    await tts.save("data/tts/yuki_direct.mp3")
    sz = os.path.getsize("data/tts/yuki_direct.mp3")
    print(f"MP3 saved: {sz} bytes")
    return sz > 100

async def test_pcm_stream():
    """Try to get raw PCM from edge-tts communicate stream."""
    tts = edge_tts.Communicate(TEXT, voice="en-US-JennyNeural")
    chunks = []
    async for chunk in tts.stream():
        if chunk["type"] == "audio":
            chunks.append(chunk["data"])
    if chunks:
        raw = b"".join(chunks)
        print(f"PCM stream bytes: {len(raw)}")
        # Write as WAV (raw MP3 bytes in stream)
        with open("data/tts/yuki_stream.mp3", "wb") as f:
            f.write(raw)
        print("Stream MP3 written")
        return True
    return False

async def main():
    os.makedirs("data/tts", exist_ok=True)
    
    print("\n--- Test 1: Direct MP3 save ---")
    ok1 = await test_mp3()
    print("OK" if ok1 else "FAILED")
    
    print("\n--- Test 2: PCM stream ---")
    ok2 = await test_pcm_stream()
    print("OK" if ok2 else "FAILED")
    
    # Try pydub to convert mp3 to wav
    print("\n--- Test 3: pydub MP3->WAV ---")
    try:
        from pydub import AudioSegment
        audio = AudioSegment.from_mp3("data/tts/yuki_direct.mp3")
        audio = audio.set_channels(1).set_frame_rate(22050).set_sample_width(2)
        audio.export("data/tts/yuki_pydub.wav", format="wav")
        sz = os.path.getsize("data/tts/yuki_pydub.wav")
        print(f"pydub WAV: {sz} bytes")
    except Exception as e:
        print(f"pydub failed: {e}")

asyncio.run(main())
