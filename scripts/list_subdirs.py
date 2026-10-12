import os

subdirs = sorted([os.path.relpath(root, 'F:/Yuki_1.0').replace('\\', '/') for root, dirs, files in os.walk('F:/Yuki_1.0/src')])
print(f"Total subdirs: {len(subdirs)}")
for d in subdirs:
    print(f"include_directories(${{CMAKE_SOURCE_DIR}}/{d})")
