import os
import datetime

files = [
    ('src/brain/self/SelfModel.h', 1, -1),
    ('src/brain/self/SelfModel.cpp', 1, -1),
    ('src/brain/self/YukiSelfModel.h', 1, -1),
    ('src/brain/self/YukiSelfModel.cpp', 1, -1),
    ('src/brain/predictive/predictive_turn_engine.cpp', 1338, 1453),
    ('src/brain/memory/EpisodicStore.cpp', 1, -1),
    ('src/brain/memory/EpisodicStore.h', 1, -1),
    ('src/brain/memory/CognitiveMemoryFabric.cpp', 68, 238),
    ('src/brain/memory/CognitiveMemoryFabric.h', 1, -1),
    ('src/brain/sleep/SleepThread.cpp', 107, 205),
    ('tests/test_yuki_full.cpp', 333, 398),
    ('CMakeLists.txt', 66, 120),
    ('src/brain/inference/VariationalStateEstimator.cpp', 20, 60),
    ('src/brain/inference/BeliefState.cpp', 50, 80),
    ('src/brain/inference/GenerativeModel.cpp', 100, 140),
    ('src/brain/inference/PolicySelector.cpp', 1, 30),
    ('src/infrastructure/ControlPlane.cpp', 60, 80),
    ('src/brain/FreeEnergyCalculator.cpp', 1, 40)
]

cutoff = datetime.datetime(2026, 6, 16).timestamp()

with open('dump1.txt', 'w', encoding='utf-8') as out1, open('dump2.txt', 'w', encoding='utf-8') as out2:
    for idx, (f, start, end) in enumerate(files):
        out = out1 if idx < 9 else out2
        if not os.path.exists(f):
            continue
            
        with open(f, 'r', encoding='utf-8') as fin:
            lines = fin.readlines()
            
        total = len(lines)
        mod_time = os.path.getmtime(f)
        modified = 'Yes' if mod_time > cutoff else 'No'
        
        todos = []
        for i, line in enumerate(lines):
            if any(x in line for x in ['TODO', 'FIXME', 'STUB']):
                todos.append(f'Line {i+1}: {line.strip()}')
                
        out.write(f'\nTotal line count: {total}\n')
        out.write(f'Modified in this session: {modified}\n')
        if todos:
            out.write('TODO/FIXME/STUB comments:\n')
            for t in todos:
                out.write(f'  {t}\n')
        else:
            out.write('TODO/FIXME/STUB comments: None\n')
            
        out.write('-------------------------------------------------------------------\n')
        out.write(f'FILE: {f}\n')
        out.write('-------------------------------------------------------------------\n')
        
        e = total if end == -1 else min(end, total)
        for i in range(start-1, e):
            out.write(f'{i+1:04d}: {lines[i]}')
