@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -G Ninja -B F:\Yuki_1.0\build-ninja -S F:\Yuki_1.0 -DCMAKE_BUILD_TYPE=Release