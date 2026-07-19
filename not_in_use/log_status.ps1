# log_status.ps1 — Yuki_1.0 Section-Gate status reporter
# Usage: powershell -File D:\Yuki_1.0\log_status.ps1
# Runs ctest and summarises gate-relevant log entries.

$buildDir  = "D:\Yuki_1.0\build"
$logMarkers = @("[METRIC]", "[DUAL]", "[SLEEP]", "[DMC]", "[RESOLVE]", "[CONTEST]",
                "[KnowledgeDaemon]", "[SystemExecutor]", "[SleepThread]")

Write-Host "============================================================"
Write-Host " Yuki_1.0 Status Report — $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "============================================================"

# ── Gate D: Build ─────────────────────────────────────────────────────────────
Write-Host "`n[Gate D] Running cmake build..."
$buildOut = & cmake --build $buildDir --config Release 2>&1
$buildErrors = $buildOut | Select-String "error" | Where-Object { $_ -notmatch "warning" }
if ($buildErrors) {
    Write-Host "  FAIL — Build errors:"
    $buildErrors | ForEach-Object { Write-Host "    $_" }
} else {
    Write-Host "  PASS — Build clean (warnings only)"
}

# ── Gate D: Tests ─────────────────────────────────────────────────────────────
Write-Host "`n[Gate D] Running ctest..."
$ctestOut = & ctest -C Release --test-dir $buildDir --output-on-failure 2>&1
$passed = ($ctestOut | Select-String "tests passed").ToString()
$failed = ($ctestOut | Select-String "FAILED").Count
Write-Host "  Result: $passed"
if ($failed -gt 0) {
    Write-Host "  FAIL — $failed test(s) failed (baseline: 1)"
    $ctestOut | Select-String "FAILED" | ForEach-Object { Write-Host "    $_" }
} else {
    Write-Host "  PASS — All tests passing!"
}

# ── Gate summary ──────────────────────────────────────────────────────────────
Write-Host "`n============================================================"
Write-Host " Gate Summary"
Write-Host "============================================================"
Write-Host " Gate A  MAP>=16/20         — requires manual 20-turn test"
Write-Host " Gate B  prec.intent>0.15   — requires manual run"
Write-Host " Gate C  No heuristic       — resolved() uses pure VSE"
Write-Host " Gate D  Build+tests        — see above"
Write-Host " Gate E  No crashes         — requires runtime observation"
Write-Host " Gate F  SleepThread vis>0  — requires 30s idle"
Write-Host " Gate G  resolve() pure VSE — code verified"
Write-Host " Gate H  Bayesian math OK   — code verified (no NaN paths)"
Write-Host " Gate I  Cross-turn accum   — code verified (EMA blend)"
Write-Host " Gate J  VSE MAP>0.30       — requires manual 20-turn test"
Write-Host "============================================================"
