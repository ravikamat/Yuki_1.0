# build_and_log.ps1
# Usage: powershell -ExecutionPolicy Bypass -File .\build_and_log.ps1 -Task "Name" -Detail "What happened"
param(
    [Parameter(Mandatory=$true)][string]$Task,
    [Parameter(Mandatory=$true)][string]$Detail,
    [string]$FilesNew = "0",
    [string]$FilesModified = "0",
    [string]$Notes = ""
)

Write-Host "=== Building Yuki_1.0 ===" -ForegroundColor Cyan
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    powershell -ExecutionPolicy Bypass -File .\log_status.ps1 -Task $Task -Status "FAIL" -Detail "Build failed: $Detail" -FilesNew $FilesNew -FilesModified $FilesModified -Tests "0/13" -Build "FAIL" -Notes $Notes
    exit 1
}

Write-Host "=== Running Tests ===" -ForegroundColor Cyan
.\build\Release\test_predictive_turn_engine.exe
if ($LASTEXITCODE -ne 0) {
    Write-Host "TESTS FAILED" -ForegroundColor Red
    powershell -ExecutionPolicy Bypass -File .\log_status.ps1 -Task $Task -Status "FAIL" -Detail "Tests failed: $Detail" -FilesNew $FilesNew -FilesModified $FilesModified -Tests "0/13" -Build "Clean" -Notes $Notes
    exit 1
}

Write-Host "=== Logging Success ===" -ForegroundColor Green
powershell -ExecutionPolicy Bypass -File .\log_status.ps1 -Task $Task -Status "PASS" -Detail $Detail -FilesNew $FilesNew -FilesModified $FilesModified -Tests "13/13" -Build "Clean" -Notes $Notes
Write-Host "Done. Check status.md for updated log." -ForegroundColor Green
