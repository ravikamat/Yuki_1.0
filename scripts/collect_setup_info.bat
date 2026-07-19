@echo off
REM ================================================================
REM collect_setup_info.bat — Yuki_1.0
REM Collects the current build environment details and appends them
REM to docs\ENVIRONMENT_REQUIREMENTS.md between the anchor comments.
REM Safe: read-only queries only. No installs. No deletions.
REM ================================================================

setlocal enabledelayedexpansion

set "DOCS_FILE=%~dp0..\docs\ENVIRONMENT_REQUIREMENTS.md"
set "TEMP_OUT=%~dp0..\docs\_env_temp.txt"

echo.
echo [Yuki Setup Collector] Gathering environment information...
echo.

REM ── Collect raw data into a temp file ───────────────────────────

echo ================================================================ > "%TEMP_OUT%"
echo ENVIRONMENT SNAPSHOT >> "%TEMP_OUT%"
echo Collected: %DATE% %TIME% >> "%TEMP_OUT%"
echo ================================================================ >> "%TEMP_OUT%"
echo. >> "%TEMP_OUT%"

REM -- cl (MSVC compiler)
echo --- cl (MSVC Compiler) ----------------------------------------- >> "%TEMP_OUT%"
where.exe cl >> "%TEMP_OUT%" 2>&1
cl 2>> "%TEMP_OUT%" 1>nul
echo. >> "%TEMP_OUT%"

REM -- cmake
echo --- cmake ------------------------------------------------------ >> "%TEMP_OUT%"
where.exe cmake >> "%TEMP_OUT%" 2>&1
cmake --version >> "%TEMP_OUT%" 2>&1
echo. >> "%TEMP_OUT%"

REM -- git
echo --- git -------------------------------------------------------- >> "%TEMP_OUT%"
where.exe git >> "%TEMP_OUT%" 2>&1
git --version >> "%TEMP_OUT%" 2>&1
echo. >> "%TEMP_OUT%"

REM -- System info
echo --- System ----------------------------------------------------- >> "%TEMP_OUT%"
echo OS: %OS% >> "%TEMP_OUT%"
echo PROCESSOR: %PROCESSOR_ARCHITECTURE% >> "%TEMP_OUT%"
echo COMPUTERNAME: %COMPUTERNAME% >> "%TEMP_OUT%"
echo. >> "%TEMP_OUT%"

echo ================================================================ >> "%TEMP_OUT%"

REM ── Display collected data to terminal ──────────────────────────
echo.
echo [Collected Output]
echo ----------------------------------------------------------------
type "%TEMP_OUT%"
echo ----------------------------------------------------------------
echo.

REM ── Inject into ENVIRONMENT_REQUIREMENTS.md between anchors ─────
REM Strategy: rewrite the file, replacing the content between
REM the START and END anchor lines with the new snapshot.

if not exist "%DOCS_FILE%" (
    echo [ERROR] Cannot find %DOCS_FILE%
    echo         Run this script from the Yuki_1.0 project root or scripts\ folder.
    del /f /q "%TEMP_OUT%" 2>nul
    exit /b 1
)

set "TEMP_REBUILT=%~dp0..\docs\_env_rebuilt.txt"
set "IN_BLOCK=0"
set "BLOCK_WRITTEN=0"

for /f "usebackq delims=" %%L in ("%DOCS_FILE%") do (
    set "LINE=%%L"

    REM Detect start anchor
    echo !LINE! | findstr /c:"COLLECT_SETUP_INFO_OUTPUT_START" >nul 2>&1
    if !errorlevel! == 0 (
        echo !LINE! >> "%TEMP_REBUILT%"
        set "IN_BLOCK=1"
        REM Immediately write the snapshot after the start anchor
        type "%TEMP_OUT%" >> "%TEMP_REBUILT%"
        set "BLOCK_WRITTEN=1"
        goto :continue_loop
    )

    REM Detect end anchor
    echo !LINE! | findstr /c:"COLLECT_SETUP_INFO_OUTPUT_END" >nul 2>&1
    if !errorlevel! == 0 (
        set "IN_BLOCK=0"
        echo !LINE! >> "%TEMP_REBUILT%"
        goto :continue_loop
    )

    REM Skip old block content (between anchors)
    if "!IN_BLOCK!" == "1" (
        goto :continue_loop
    )

    REM Normal line — copy through
    echo !LINE! >> "%TEMP_REBUILT%"

    :continue_loop
)

REM Replace the original file with the rebuilt one
copy /y "%TEMP_REBUILT%" "%DOCS_FILE%" >nul
del /f /q "%TEMP_REBUILT%" 2>nul
del /f /q "%TEMP_OUT%" 2>nul

echo [Yuki Setup Collector] Done.
echo     Environment snapshot written to:
echo     %DOCS_FILE%
echo.

endlocal
