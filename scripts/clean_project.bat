@echo off
REM ================================================================
REM clean_project.bat — Yuki_1.0
REM Removes all CMake and MSVC build artifacts.
REM Does NOT touch source files, docs, or scripts.
REM ================================================================

echo.
echo [Yuki Clean] Starting artifact cleanup...
echo.

REM ── build directory ─────────────────────────────────────────────
if exist "%~dp0..\build" (
    echo [REMOVE] build\
    rd /s /q "%~dp0..\build"
) else (
    echo [SKIP]   build\  ^(not found^)
)

REM ── Visual Studio hidden folder ──────────────────────────────────
if exist "%~dp0..\.vs" (
    echo [REMOVE] .vs\
    rd /s /q "%~dp0..\.vs"
) else (
    echo [SKIP]   .vs\  ^(not found^)
)

REM ── CMakeFiles directory (in-source, if it exists) ───────────────
if exist "%~dp0..\CMakeFiles" (
    echo [REMOVE] CMakeFiles\
    rd /s /q "%~dp0..\CMakeFiles"
) else (
    echo [SKIP]   CMakeFiles\  ^(not found^)
)

REM ── CMakeCache.txt ───────────────────────────────────────────────
if exist "%~dp0..\CMakeCache.txt" (
    echo [REMOVE] CMakeCache.txt
    del /f /q "%~dp0..\CMakeCache.txt"
) else (
    echo [SKIP]   CMakeCache.txt  ^(not found^)
)

REM ── cmake_install.cmake ──────────────────────────────────────────
if exist "%~dp0..\cmake_install.cmake" (
    echo [REMOVE] cmake_install.cmake
    del /f /q "%~dp0..\cmake_install.cmake"
) else (
    echo [SKIP]   cmake_install.cmake  ^(not found^)
)

REM ── compile_commands.json ────────────────────────────────────────
if exist "%~dp0..\compile_commands.json" (
    echo [REMOVE] compile_commands.json
    del /f /q "%~dp0..\compile_commands.json"
) else (
    echo [SKIP]   compile_commands.json  ^(not found^)
)

echo.
echo [Yuki Clean] Done. Source files, docs, and scripts are untouched.
echo.
