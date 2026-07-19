@echo off
setlocal enabledelayedexpansion

REM ================================================================
REM rebuild_and_run.bat - Yuki_1.0
REM Automates Configure -> Build -> Run workflow.
REM ================================================================

REM 1. Detect project root relative to this script (scripts\rebuild_and_run.bat)
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

REM Resolve to absolute path
pushd "%PROJECT_ROOT%"
set "PROJECT_ROOT=%CD%"
popd

REM 2. Validate Project Root
if not exist "%PROJECT_ROOT%\CMakeLists.txt" (
    echo [ERROR] Could not find CMakeLists.txt at: %PROJECT_ROOT%
    exit /b 1
)

echo.
echo ================================================================
echo Yuki_1.0 - Single Step Rebuild and Run
echo Root: %PROJECT_ROOT%
echo ================================================================

REM ----------------------------------------------------------------
echo [1/3] Configuring CMake...
if not exist "%PROJECT_ROOT%\build" mkdir "%PROJECT_ROOT%\build"

cmake -S "%PROJECT_ROOT%" -B "%PROJECT_ROOT%\build"
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

REM ----------------------------------------------------------------
echo.
echo [2/3] Building Project (Debug)...
cmake --build "%PROJECT_ROOT%\build" --config Debug
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

REM ----------------------------------------------------------------
echo.
echo [3/3] Detecting Executable...

set "EXE_PATH_1=%PROJECT_ROOT%\build\Debug\yuki.exe"
set "EXE_PATH_2=%PROJECT_ROOT%\build\yuki.exe"

set "TARGET_EXE="

if exist "%EXE_PATH_1%" (
    set "TARGET_EXE=%EXE_PATH_1%"
) else if exist "%EXE_PATH_2%" (
    set "TARGET_EXE=%EXE_PATH_2%"
)

if "!TARGET_EXE!"=="" (
    echo [ERROR] Yuki executable not found at:
    echo   1. %EXE_PATH_1%
    echo   2. %EXE_PATH_2%
    exit /b 1
)

echo [SUCCESS] Running: !TARGET_EXE!
echo ----------------------------------------------------------------
echo.

"!TARGET_EXE!"

endlocal
