# setup_ollama.ps1 — Bootstrap Ollama + Qwen3-1.7B for Yuki
# Run as: powershell -ExecutionPolicy Bypass -File scripts\setup_ollama.ps1

$ErrorActionPreference = "Stop"

Write-Host "=== Yuki Ollama Setup ===" -ForegroundColor Cyan

# Check if Ollama is installed
$ollama = Get-Command ollama -ErrorAction SilentlyContinue
if (-not $ollama) {
    Write-Host "Ollama not found. Downloading installer..." -ForegroundColor Yellow
    $url = "https://ollama.com/download/OllamaSetup.exe"
    $out = "$env:TEMP\OllamaSetup.exe"
    Invoke-WebRequest -Uri $url -OutFile $out
    Write-Host "Installing Ollama (run the installer, then re-run this script)..." -ForegroundColor Yellow
    Start-Process -FilePath $out -Wait
    Write-Host "Please restart your terminal and re-run this script." -ForegroundColor Green
    exit 0
}

Write-Host "Ollama found at: $($ollama.Source)" -ForegroundColor Green

# Check if Ollama service is running
try {
    $resp = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
    Write-Host "Ollama service is running." -ForegroundColor Green
} catch {
    Write-Host "Ollama service not responding. Starting Ollama..." -ForegroundColor Yellow
    Start-Process ollama -ArgumentList "serve" -WindowStyle Hidden
    Start-Sleep -Seconds 5
}

# Pull Qwen3-1.7B
Write-Host "Pulling qwen3:1.7b (this may take a few minutes)..." -ForegroundColor Cyan
& ollama pull qwen3:1.7b

# Verify
$models = & ollama list
if ($models -match "qwen3:1.7b") {
    Write-Host "SUCCESS: qwen3:1.7b is ready!" -ForegroundColor Green
    Write-Host "VRAM usage: ~1.5GB. Inference speed: ~2-3 tok/s on i5 CPU." -ForegroundColor Gray
} else {
    Write-Host "ERROR: Model pull failed. Check your internet connection." -ForegroundColor Red
    exit 1
}

Write-Host "`nOllama is running at http://localhost:11434" -ForegroundColor Green
Write-Host "Yuki will auto-detect and use it on next startup." -ForegroundColor Green
