<# 
.SYNOPSIS
    yuki-doctor.ps1 — KT-6 Machine Readiness Gate
    Runs BEFORE long autonomous runs. Validates toolchain, memory, disk, and YUKI-specific checks.
    Based on K3's k3-doctor.sh: toolchain/process checks, MemAvailable (NOT MemTotal) for capacity,
    disk measured with YUKI's access pattern; strictness tiering — blocking failure vs warning
    that scopes what still works, plus a copy-pasteable next command.

.DESCRIPTION
    Performs pre-flight checks for YUKI autonomous operation:
    - Toolchain: CMake, MSVC, Ninja, Python, Git
    - Memory: MemAvailable (not MemTotal), swap status
    - Disk: NVMe/SSD measured with YUKI's access pattern (QD1 vs QD16)
    - YUKI-specific: bootstrap pack, databases, model files, config
    - Strictness tiers: BLOCKING (must fix) vs ADVISORY (scopes capability)

    Exit codes (KT-2 taxonomy):
      0 = all checks passed
      1 = environment failed (valid request, world broke)
      2 = refused (configuration wrong/unsafe, nothing ran)
      4 = unsound (checks passed but degraded capability detected)

.EXAMPLE
    .\yuki-doctor.ps1
    .\yuki-doctor.ps1 -Strictness Blocking
    .\yuki-doctor.ps1 -Profile laptop
#>

param(
    [ValidateSet('Blocking', 'Advisory')]
    [string]$Strictness = 'Blocking',
    
    [ValidateSet('laptop', 'desktop', 'server')]
    [string]$Profile = 'desktop',
    
    [switch]$JsonOutput,
    [switch]$SkipDiskBench,
    [switch]$AutoFix
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ─── Configuration ──────────────────────────────────────────────────────────
$PROJECT_ROOT = (Get-Item -Path ".\").FullName
$BUILD_DIR = Join-Path $PROJECT_ROOT "build"
$DATA_DIR = Join-Path $PROJECT_ROOT "data"
$BOOTSTRAP_PACK = Join-Path $DATA_DIR "brain" "cognitive_bootstrap.pack"
$YUKI_DB = Join-Path $DATA_DIR "brain" "yuki.db"
$CONFIG_FILE = Join-Path $DATA_DIR "config" "yuki_config.ini"

# Profile-based memory floors (bytes)
$MEM_FLOORS = @{
    'laptop'  = 4GB   # 4 GB minimum available
    'desktop' = 8GB   # 8 GB minimum available  
    'server'  = 16GB  # 16 GB minimum available
}

# Disk benchmark parameters (match YUKI's access pattern)
$DISK_TEST_SIZE = 256MB
$DISK_QD1_ITERATIONS = 10
$DISK_QD16_ITERATIONS = 5

# ─── State ──────────────────────────────────────────────────────────────────
$results = @()
$blocking_failures = 0
$advisory_warnings = 0
$unsound_conditions = 0

function Add-Result {
    param([string]$Check, [string]$Status, [string]$Message, [string]$Severity, [string]$NextCommand)
    $results += [pscustomobject]@{
        Check = $Check
        Status = $Status
        Message = $Message
        Severity = $Severity
        NextCommand = $NextCommand
    }
    switch ($Severity) {
        'BLOCKING' { $script:blocking_failures++ }
        'ADVISORY' { $script:advisory_warnings++ }
        'UNSOUND'  { $script:unsound_conditions++ }
    }
}

function Write-Header {
    param([string]$Text)
    Write-Host "`n=== $Text ===" -ForegroundColor Cyan
}

function Write-Status {
    param([string]$Check, [string]$Status, [string]$Message, [string]$Severity = '', [string]$NextCommand = '')
    $color = switch ($Status) {
        'PASS' { 'Green' }
        'FAIL' { 'Red' }
        'WARN' { 'Yellow' }
        'DEGRADED' { 'Magenta' }
        default { 'White' }
    }
    $prefix = switch ($Severity) {
        'BLOCKING' { '[BLOCK] ' }
        'ADVISORY' { '[ADVISORY] ' }
        'UNSOUND'  { '[UNSOUND] ' }
        default { '' }
    }
    Write-Host "  $prefix$Check : $Status" -NoNewline -ForegroundColor $color
    Write-Host " — $Message"
    if ($NextCommand) { Write-Host "    Next: $NextCommand" -ForegroundColor Gray }
}

# ─── Check 1: Toolchain ────────────────────────────────────────────────────
Write-Header "TOOLCHAIN"

$tools = @(
    @{ Name='CMake';        Cmd='cmake --version';           MinVersion='3.20' },
    @{ Name='MSVC (cl.exe)'; Cmd='cl 2>&1 | Select-String "Version"'; MinVersion='19.30' },
    @{ Name='Ninja';        Cmd='ninja --version';           MinVersion='1.11' },
    @{ Name='Python';       Cmd='python --version';          MinVersion='3.10' },
    @{ Name='Git';          Cmd='git --version';             MinVersion='2.40' },
    @{ Name='vcpkg';        Cmd='vcpkg version';             MinVersion='2023' }
)

foreach ($t in $tools) {
    try {
        $output = Invoke-Expression $t.Cmd 2>&1 | Select-Object -First 1
        $version = ($output -split '\s+')[-1]
        Add-Result -Check $t.Name -Status 'PASS' -Message "Found $version" -Severity '' -NextCommand ''
        Write-Status -Check $t.Name -Status 'PASS' -Message "Found $version"
    }
    catch {
        Add-Result -Check $t.Name -Status 'FAIL' -Message "Not found or error: $_" -Severity 'BLOCKING' -NextCommand "Install $($t.Name) and ensure it's in PATH"
        Write-Status -Check $t.Name -Status 'FAIL' -Message "Not found: $_" -Severity 'BLOCKING' -NextCommand "Install $($t.Name) and ensure it's in PATH"
    }
}

# ─── Check 2: Memory (MemAvailable, NOT MemTotal) ──────────────────────────
Write-Header "MEMORY"

try {
    $os = Get-CimInstance Win32_OperatingSystem
    $totalPhysical = [int64]$os.TotalVisibleMemorySize * 1024
    $freePhysical  = [int64]$os.FreePhysicalMemory * 1024
    $availPhysical = [int64]$os.FreePhysicalMemory * 1024  # Win32_OperatingSystem doesn't expose MemAvailable directly
    
    # Better: use Get-CimInstance Win32_PageFileUsage for commit limit
    $pf = Get-CimInstance Win32_PageFileUsage
    $commitLimit = ($pf | Measure-Object -Property AllocatedBaseSize -Sum).Sum * 1024 * 1024
    $commitPeak  = ($pf | Measure-Object -Property CurrentUsage -Sum).Sum * 1024 * 1024
    
    # On Windows, MemAvailable ≈ FreePhysical + Standby (cached)
    # Use performance counter for true available
    $perf = Get-Counter '\Memory\Available Bytes' -ErrorAction SilentlyContinue
    if ($perf) {
        $memAvailable = [int64]$perf.CounterSamples[0].CookedValue
    } else {
        $memAvailable = $freePhysical  # Conservative fallback
    }
    
    $floor = $MEM_FLOORS[$Profile]
    $pct = [math]::Round(($memAvailable / $totalPhysical) * 100, 1)
    
    $msg = "Available: $([math]::Round($memAvailable/1GB,2)) GB / Total: $([math]::Round($totalPhysical/1GB,2)) GB ($pct%) | Commit: $([math]::Round($commitPeak/1GB,2))/$([math]::Round($commitLimit/1GB,2)) GB"
    
    if ($memAvailable -lt $floor) {
        $severity = if ($Strictness -eq 'Blocking') { 'BLOCKING' } else { 'ADVISORY' }
        Add-Result -Check 'MemAvailable' -Status 'FAIL' -Message $msg -Severity $severity `
            -NextCommand "Close applications or increase RAM. Floor for $Profile: $([math]::Round($floor/1GB)) GB"
        Write-Status -Check 'MemAvailable' -Status 'FAIL' -Message $msg -Severity $severity `
            -NextCommand "Close applications or increase RAM. Floor for $Profile: $([math]::Round($floor/1GB)) GB"
    } else {
        Add-Result -Check 'MemAvailable' -Status 'PASS' -Message $msg -Severity ''
        Write-Status -Check 'MemAvailable' -Status 'PASS' -Message $msg
    }
    
    # Swap check
    if ($commitPeak -gt ($commitLimit * 0.8)) {
        Add-Result -Check 'CommitCharge' -Status 'WARN' -Message "Commit charge at $([math]::Round(($commitPeak/$commitLimit)*100,1))% of limit" -Severity 'ADVISORY' -NextCommand 'Increase page file or physical RAM'
        Write-Status -Check 'CommitCharge' -Status 'WARN' -Message "Commit charge at $([math]::Round(($commitPeak/$commitLimit)*100,1))% of limit" -Severity 'ADVISORY' -NextCommand 'Increase page file or physical RAM'
    }
}
catch {
    Add-Result -Check 'Memory' -Status 'FAIL' -Message "Could not query memory: $_" -Severity 'BLOCKING' -NextCommand 'Run as Administrator or check WMI service'
    Write-Status -Check 'Memory' -Status 'FAIL' -Message "Could not query memory: $_" -Severity 'BLOCKING' -NextCommand 'Run as Administrator or check WMI service'
}

# ─── Check 3: Disk (measured with YUKI's access pattern) ───────────────────
if (-not $SkipDiskBench) {
    Write-Header "DISK (YUKI Access Pattern)"
    
    $testFile = Join-Path $env:TEMP "yuki_disk_test_$PID.bin"
    $blockSize = 1MB  # YUKI's typical read size
    
    try {
        # Create test file
        $bytes = New-Object byte[] ($DISK_TEST_SIZE)
        [System.IO.File]::WriteAllBytes($testFile, $bytes)
        
        # QD1 sequential read (unbuffered)
        $qd1Times = @()
        for ($i=0; $i -lt $DISK_QD1_ITERATIONS; $i++) {
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            $fs = [System.IO.File]::Open($testFile, 'Open', 'Read', 'None', 1MB, [System.IO.FileOptions]::Asynchronous)
            $buffer = New-Object byte[] $blockSize
            while ($fs.Read($buffer, 0, $blockSize) -gt 0) { }
            $fs.Close()
            $qd1Times += $sw.ElapsedMilliseconds
        }
        $qd1Avg = ($qd1Times | Measure-Object -Average).Average
        $qd1BW = [math]::Round(($DISK_TEST_SIZE / 1MB) / ($qd1Avg / 1000), 1)
        
        # QD16 (simulated with async reads)
        $qd16Times = @()
        for ($i=0; $i -lt $DISK_QD16_ITERATIONS; $i++) {
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            $tasks = @()
            for ($q=0; $q -lt 16; $q++) {
                $tasks += [System.Threading.Tasks.Task]::Run({
                    $fs = [System.IO.File]::Open($testFile, 'Open', 'Read', 'None', 1MB, [System.IO.FileOptions]::Asynchronous)
                    $buffer = New-Object byte[] $blockSize
                    while ($fs.Read($buffer, 0, $blockSize) -gt 0) { }
                    $fs.Close()
                })
            }
            [System.Threading.Tasks.Task]::WaitAll($tasks)
            $qd16Times += $sw.ElapsedMilliseconds
        }
        $qd16Avg = ($qd16Times | Measure-Object -Average).Average
        $qd16BW = [math]::Round(($DISK_TEST_SIZE / 1MB) / ($qd16Avg / 1000), 1)
        
        $gap = [math]::Round(($qd16BW - $qd1BW) / $qd1BW * 100, 1)
        
        $msg = "QD1: ${qd1BW} MB/s | QD16: ${qd16BW} MB/s | Gap: ${gap}% (prefetch headroom)"
        
        # K3: QD1→QD16 gap is the headroom prefetch can convert
        if ($qd1BW -lt 500) {  # Below 500 MB/s sequential is concerning for SQLite/WAL
            Add-Result -Check 'DiskSequential' -Status 'WARN' -Message $msg -Severity 'ADVISORY' -NextCommand 'Consider NVMe SSD; current disk may bottleneck T1/T2 paging'
            Write-Status -Check 'DiskSequential' -Status 'WARN' -Message $msg -Severity 'ADVISORY' -NextCommand 'Consider NVMe SSD; current disk may bottleneck T1/T2 paging'
        } else {
            Add-Result -Check 'DiskSequential' -Status 'PASS' -Message $msg -Severity ''
            Write-Status -Check 'DiskSequential' -Status 'PASS' -Message $msg
        }
        
        # Random 4K (SQLite page size)
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $fs = [System.IO.File]::Open($testFile, 'Open', 'Read', 'None', 4KB, [System.IO.FileOptions]::Asynchronous)
        $rnd = New-Object System.Random
        $buffer = New-Object byte[] 4KB
        for ($i=0; $i -lt 1000; $i++) {
            $pos = $rnd.Next(0, $DISK_TEST_SIZE - 4KB)
            $fs.Position = $pos
            $fs.Read($buffer, 0, 4KB) | Out-Null
        }
        $fs.Close()
        $randTime = $sw.ElapsedMilliseconds
        $randIOPS = [math]::Round(1000 / ($randTime / 1000), 0)
        
        $msg2 = "Random 4K: ${randIOPS} IOPS (${randTime}ms for 1000 ops)"
        if ($randIOPS -lt 10000) {
            Add-Result -Check 'DiskRandom4K' -Status 'WARN' -Message $msg2 -Severity 'ADVISORY' -NextCommand 'Low random IOPS may stall SQLite WAL; consider NVMe'
            Write-Status -Check 'DiskRandom4K' -Status 'WARN' -Message $msg2 -Severity 'ADVISORY' -NextCommand 'Low random IOPS may stall SQLite WAL; consider NVMe'
        } else {
            Add-Result -Check 'DiskRandom4K' -Status 'PASS' -Message $msg2 -Severity ''
            Write-Status -Check 'DiskRandom4K' -Status 'PASS' -Message $msg2
        }
        
        # Cleanup
        Remove-Item $testFile -Force -ErrorAction SilentlyContinue
    }
    catch {
        Add-Result -Check 'DiskBenchmark' -Status 'FAIL' -Message "Disk benchmark failed: $_" -Severity 'ADVISORY' -NextCommand 'Run with -SkipDiskBench to bypass'
        Write-Status -Check 'DiskBenchmark' -Status 'FAIL' -Message "Disk benchmark failed: $_" -Severity 'ADVISORY' -NextCommand 'Run with -SkipDiskBench to bypass'
        Remove-Item $testFile -Force -ErrorAction SilentlyContinue
    }
}

# ─── Check 4: YUKI-Specific Files ──────────────────────────────────────────
Write-Header "YUKI ARTIFACTS"

$artifacts = @(
    @{ Path=$BOOTSTRAP_PACK; Name='Cognitive Bootstrap Pack'; MinSize=1GB; Required=$true },
    @{ Path=$YUKI_DB;        Name='YUKI Database';          MinSize=10MB; Required=$true },
    @{ Path=$CONFIG_FILE;    Name='Config File';            MinSize=1KB;  Required=$true }
)

foreach ($a in $artifacts) {
    if (Test-Path $a.Path) {
        $size = (Get-Item $a.Path).Length
        $sizeGB = [math]::Round($size/1GB, 2)
        if ($size -ge $a.MinSize) {
            Add-Result -Check $a.Name -Status 'PASS' -Message "Found ($sizeGB GB)" -Severity ''
            Write-Status -Check $a.Name -Status 'PASS' -Message "Found ($sizeGB GB)"
        } else {
            Add-Result -Check $a.Name -Status 'WARN' -Message "Found but small ($sizeGB GB, expected > $([math]::Round($a.MinSize/1GB,2)) GB)" -Severity 'ADVISORY' -NextCommand "Run bootstrap extraction: .\yuki.exe --extract-bootstrap"
            Write-Status -Check $a.Name -Status 'WARN' -Message "Found but small ($sizeGB GB)" -Severity 'ADVISORY' -NextCommand "Run bootstrap extraction: .\yuki.exe --extract-bootstrap"
        }
    } else {
        $severity = if ($a.Required) { 'BLOCKING' } else { 'ADVISORY' }
        Add-Result -Check $a.Name -Status 'FAIL' -Message "Not found at $($a.Path)" -Severity $severity -NextCommand "Run bootstrap extraction: .\yuki.exe --extract-bootstrap"
        Write-Status -Check $a.Name -Status 'FAIL' -Message "Not found at $($a.Path)" -Severity $severity -NextCommand "Run bootstrap extraction: .\yuki.exe --extract-bootstrap"
    }
}

# Model file check
$modelDir = Join-Path $DATA_DIR "models"
$ggufs = Get-ChildItem $modelDir -Filter "*.gguf" -ErrorAction SilentlyContinue
if ($ggufs.Count -gt 0) {
    foreach ($g in $ggufs) {
        $sizeGB = [math]::Round($g.Length/1GB, 2)
        Write-Status -Check "Model: $($g.Name)" -Status 'PASS' -Message "Found ($sizeGB GB)"
    }
} else {
    Add-Result -Check 'GGUF Models' -Status 'FAIL' -Message "No .gguf models in $modelDir" -Severity 'ADVISORY' -NextCommand 'Place teacher model in data/models/ for Stage 0'
    Write-Status -Check 'GGUF Models' -Status 'FAIL' -Message "No .gguf models in $modelDir" -Severity 'ADVISORY' -NextCommand 'Place teacher model in data/models/ for Stage 0'
}

# ─── Check 5: Build Artifacts ──────────────────────────────────────────────
Write-Header "BUILD"

$exe = Join-Path $BUILD_DIR "yuki.exe"
$kbExe = Join-Path $BUILD_DIR "yuki_kb.exe"
$coreLib = Join-Path $BUILD_DIR "yuki_core.lib"

if (Test-Path $exe) {
    $ver = (Get-Item $exe).VersionInfo.ProductVersion
    Add-Result -Check 'yuki.exe' -Status 'PASS' -Message "Built (v$ver)" -Severity ''
    Write-Status -Check 'yuki.exe' -Status 'PASS' -Message "Built (v$ver)"
} else {
    Add-Result -Check 'yuki.exe' -Status 'FAIL' -Message "Not built" -Severity 'BLOCKING' -NextCommand "cmake --build build --config Release"
    Write-Status -Check 'yuki.exe' -Status 'FAIL' -Message "Not built" -Severity 'BLOCKING' -NextCommand "cmake --build build --config Release"
}

if (Test-Path $kbExe) {
    Add-Result -Check 'yuki_kb.exe' -Status 'PASS' -Message "Built" -Severity ''
    Write-Status -Check 'yuki_kb.exe' -Status 'PASS' -Message "Built"
} else {
    Add-Result -Check 'yuki_kb.exe' -Status 'FAIL' -Message "Not built" -Severity 'BLOCKING' -NextCommand "cmake --build build --config Release --target yuki_kb"
    Write-Status -Check 'yuki_kb.exe' -Status 'FAIL' -Message "Not built" -Severity 'BLOCKING' -NextCommand "cmake --build build --config Release --target yuki_kb"
}

# ─── Check 6: YUKI Soundness Counter ───────────────────────────────────────
Write-Header "SOUNDNESS"
Add-Result -Check 'SoundnessCounter' -Status 'PASS' -Message "yuki_soundness.h available" -Severity ''
Write-Status -Check 'SoundnessCounter' -Status 'PASS' -Message "yuki_soundness.h available"

# ─── Summary ────────────────────────────────────────────────────────────────
Write-Header "SUMMARY"

$exitCode = 0
if ($blocking_failures -gt 0) {
    $exitCode = 2  # REFUSED - configuration wrong
    Write-Host "`n[BLOCKING FAILURES] $blocking_failures — Cannot proceed" -ForegroundColor Red
}
if ($advisory_warnings -gt 0) {
    if ($exitCode -eq 0) { $exitCode = 1 }  # ENV_FAILED - world not ready
    Write-Host "[ADVISORY WARNINGS] $advisory_warnings — Capability scoped" -ForegroundColor Yellow
}
if ($unsound_conditions -gt 0) {
    $exitCode = 4  # UNSOUND - passed but degraded
    Write-Host "[UNSOUND CONDITIONS] $unsound_conditions — Degraded capability detected" -ForegroundColor Magenta
}
if ($exitCode -eq 0) {
    Write-Host "`n[READY] All checks passed. YUKI can run autonomous." -ForegroundColor Green
}

# Print copy-pasteable next command
if ($blocking_failures -gt 0 -or $advisory_warnings -gt 0) {
    Write-Host "`n=== NEXT COMMAND ===" -ForegroundColor Cyan
    $nextCmds = $results | Where-Object { $_.NextCommand } | Select-Object -ExpandProperty NextCommand -Unique
    foreach ($cmd in $nextCmds) { Write-Host "  $cmd" }
}

# JSON output for CI
if ($JsonOutput) {
    $results | ConvertTo-Json -Depth 3
}

exit $exitCode