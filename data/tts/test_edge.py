import asyncio
import edge_tts
import os, sys

async def test():
    print("edge_tts version:", edge_tts.__version__)
    print("Python:", sys.version)
    os.makedirs("data/tts", exist_ok=True)
    try:
        tts = edge_tts.Communicate("Hello, I am Yuki.", voice="en-US-JennyNeural")
        await tts.save("data/tts/test_edge.mp3")
        size = os.path.getsize("data/tts/test_edge.mp3")
        print("OK — saved", size, "bytes to data/tts/test_edge.mp3")
    except Exception as e:
        print("FAILED:", type(e).__name__, e)
        import traceback; traceback.print_exc()

asyncio.run(test())
