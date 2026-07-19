import os

artifact_path = r'C:\Users\RahulRavi\.gemini\antigravity\brain\1c577143-ca25-4571-beab-d18e7e44dc08\code_review_dump_2.md'

with open(artifact_path, 'w', encoding='utf-8') as out:
    def dump_file(title, path, start_line=1, end_line=None):
        out.write(f'═══════════════════════════════════════════════════════════════════\n')
        out.write(f'FILE: {path}\n')
        out.write(f'═══════════════════════════════════════════════════════════════════\n')
        with open(path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            for i, line in enumerate(lines, 1):
                if i >= start_line and (end_line is None or i <= end_line):
                    out.write(f'{i:04d}: {line}')
        out.write('\n\n')

    dump_file('1', r'D:\Yuki_1.0\src\brain\self\SelfModel.cpp')
    dump_file('2', r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.cpp', 1, 100)
    dump_file('3', r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.cpp', 140, 220)
    dump_file('4', r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.cpp', 640, 720)
    dump_file('5', r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.h')
    dump_file('6', r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.cpp', 240, 320)

    # Check for sqlite3_open and sqlite3_close
    out.write('═══════════════════════════════════════════════════════════════════\n')
    out.write('SQLITE OPEN/CLOSE CHECK IN EpisodicStore.cpp\n')
    out.write('═══════════════════════════════════════════════════════════════════\n')
    with open(r'D:\Yuki_1.0\src\brain\memory\EpisodicStore.cpp', 'r', encoding='utf-8') as f:
        lines = f.readlines()
        for i, line in enumerate(lines, 1):
            if 'sqlite3_open' in line:
                out.write(f'Found sqlite3_open at line {i}: {line.strip()}\n')
            if 'sqlite3_close' in line:
                out.write(f'Found sqlite3_close at line {i}: {line.strip()}\n')
