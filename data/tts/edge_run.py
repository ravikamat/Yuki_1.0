import asyncio, edge_tts, subprocess, os; async def _go():
    with open(r'data\tts\edge_input.txt', encoding='utf-8') as f: txt=f.read()
    tts=edge_tts.Communicate(txt, voice='en-US-JennyNeural', rate='+0%', pitch='-5Hz')
    await tts.save(r'data\tts\temp_chunk_7.wav.mp3')
asyncio.run(_go())
mp3=r'data\tts\temp_chunk_7.wav.mp3'
wav=r'data\tts\temp_chunk_7.wav'
try:
    r=subprocess.run(['ffmpeg','-y','-i',mp3,'-acodec','pcm_s16le','-ar','22050','-ac','1',wav],capture_output=True,timeout=10)
    if r.returncode!=0: open(wav,'wb').write(open(mp3,'rb').read())
except: open(wav,'wb').write(open(mp3,'rb').read())
