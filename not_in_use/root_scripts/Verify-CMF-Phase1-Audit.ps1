#Requires -Version 5.1
<#
.SYNOPSIS
    Yuki CMF Phase 1 Audit Script - Verify database state before Phase 1.5
.DESCRIPTION
    Comprehensive audit of Cognitive Memory Fabric Phase 1 implementation.
    Checks files, database schema, row counts, sample data, and build status.
    Generates detailed report for Phase 1.5 planning.
.NOTES
    Run from D:\Yuki_1.0 root directory as Administrator
    Requires: sqlite3.exe in PATH or in D:\Yuki_1.0
#>

$ErrorActionPreference = "Stop"
$RootDir = "D:\Yuki_1.0"
$DbPath = "$RootDir\datarain\cmf_episodes.db"
$ReportPath = "$RootDir\datarain\cmf_phase1_audit_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
$SqliteExe = "sqlite3"

# Try to find sqlite3
function Find-Sqlite3 {
    $localSqlite = "$RootDir\sqlite3.exe"
    if (Test-Path $localSqlite) { return $localSqlite }

    try {
        $inPath = Get-Command sqlite3 -ErrorAction SilentlyContinue
        if ($inPath) { return "sqlite3" }
    } catch {}

    # Check common locations
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\sqlite\sqlite3.exe",
        "C:\Program Files\sqlite3\sqlite3.exe",
        "C:\sqlite\sqlite3.exe",
        "$RootDir\srcendor\sqlite\sqlite3.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

$Sqlite3 = Find-Sqlite3

function Write-Report {
    param([string]$Line, [string]$Color = "White")
    Add-Content -Path $ReportPath -Value $Line -ErrorAction SilentlyContinue
    Write-Host $Line -ForegroundColor $Color
}

function Invoke-SqliteQuery {
    param([string]$Query, [string]$Fallback = "N/A")
    if (-not $Sqlite3) { return $Fallback }
    try {
        $result = & $Sqlite3 $DbPath $Query 2>$null
        if ($LASTEXITCODE -eq 0 -and $result) { return $result }
        return $Fallback
    } catch { return "ERROR: $_" }
}

# Initialize report
New-Item -ItemType File -Path $ReportPath -Force | Out-Null
Write-Report "===================================================================" "Cyan"
Write-Report "  YUKI COGNITIVE MEMORY FABRIC - PHASE 1 AUDIT REPORT" "Cyan"
Write-Report "  Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "Cyan"
Write-Report "  Database: $DbPath" "Cyan"
Write-Report "===================================================================" "Cyan"
Write-Report ""

# ============================================================================
# SECTION 1: FILE SYSTEM AUDIT
# ============================================================================
Write-Report "[1] PHASE 1 FILE SYSTEM AUDIT" "Yellow"
Write-Report "-------------------------------------------------------------------"

$Phase1Files = @{
    "MemoryEncoder.h" = "srcrain\memory\MemoryEncoder.h"
    "MemoryEncoder.cpp" = "srcrain\memory\MemoryEncoder.cpp"
    "EpisodicStore.h" = "srcrain\memory\EpisodicStore.h"
    "EpisodicStore.cpp" = "srcrain\memory\EpisodicStore.cpp"
    "SemanticGraph.h" = "srcrain\memory\SemanticGraph.h"
    "SemanticGraph.cpp" = "srcrain\memory\SemanticGraph.cpp"
    "CognitiveMemoryFabric.h" = "srcrain\memory\CognitiveMemoryFabric.h"
    "CognitiveMemoryFabric.cpp" = "srcrain\memory\CognitiveMemoryFabric.cpp"
}

$AllPresent = $true
foreach ($name in $Phase1Files.Keys) {
    $path = "$RootDir\$($Phase1Files[$name])"
    $exists = Test-Path $path
    $size = if ($exists) { "{0:N0} bytes" -f (Get-Item $path).Length } else { "MISSING" }
    $status = if ($exists) { "[PASS]" } else { "[FAIL]" }
    $color = if ($exists) { "Green" } else { "Red" }
    Write-Report ("  {0} {1,-30} {2}" -f $status, $name, $size) $color
    if (-not $exists) { $AllPresent = $false }
}

Write-Report ""
Write-Report "  Phase 1 Core Files: $(if ($AllPresent) { 'ALL PRESENT' } else { 'MISSING FILES DETECTED' })" $(if ($AllPresent) { "Green" } else { "Red" })
Write-Report ""

# ============================================================================
# SECTION 2: MODIFIED FILES AUDIT
# ============================================================================
Write-Report "[2] MODIFIED EXISTING FILES AUDIT" "Yellow"
Write-Report "-------------------------------------------------------------------"

$ModifiedFiles = @(
    @{ Path = "src\input\encoding\ObservationEncoder.h"; Check = "setMemoryFabric|HeuristicScores|cmf_" },
    @{ Path = "src\input\encoding\ObservationEncoder.cpp"; Check = "cmf_->ingest|MemoryPacket" },
    @{ Path = "srcrain\learning\KnowledgeDaemon.h"; Check = "setMemoryFabric|cmf_" },
    @{ Path = "srcrain\learning\KnowledgeDaemon.cpp"; Check = "cmf_->ingest|KNOWLEDGE_FACT" },
    @{ Path = "src\BabyMode.h"; Check = "cmFabric|cmf_" },
    @{ Path = "src\BabyMode.cpp"; Check = "CognitiveMemoryFabric|cmf_->" },
    @{ Path = "CMakeLists.txt"; Check = "MemoryEncoder|EpisodicStore|SemanticGraph|CognitiveMemoryFabric" }
)

foreach ($mod in $ModifiedFiles) {
    $fullPath = "$RootDir\$($mod.Path)"
    $exists = Test-Path $fullPath
    if (-not $exists) {
        Write-Report ("  [FAIL] {0} - FILE NOT FOUND" -f $mod.Path) "Red"
        continue
    }

    $content = Get-Content $fullPath -Raw -ErrorAction SilentlyContinue
    $hasChanges = $content -match $mod.Check
    $status = if ($hasChanges) { "[PASS]" } else { "[WARN]" }
    $color = if ($hasChanges) { "Green" } else { "Yellow" }
    Write-Report ("  {0} {1}" -f $status, $mod.Path) $color

    if ($hasChanges) {
        $matches = Select-String -Path $fullPath -Pattern $mod.Check | Select-Object -First 3
        foreach ($m in $matches) {
            Write-Report ("       + {0}" -f ($m.Line -replace '\s+', ' ').Trim()) "Gray"
        }
    }
}
Write-Report ""

# ============================================================================
# SECTION 3: DATABASE AUDIT
# ============================================================================
Write-Report "[3] DATABASE AUDIT" "Yellow"
Write-Report "-------------------------------------------------------------------"

if (-not $Sqlite3) {
    Write-Report "  [FAIL] sqlite3.exe NOT FOUND in PATH" "Red"
    Write-Report "  Please install SQLite or add to PATH" "Red"
    Write-Report "  Download: https://sqlite.org/download.html" "Yellow"
} elseif (-not (Test-Path $DbPath)) {
    Write-Report "  [FAIL] Database file NOT FOUND: $DbPath" "Red"
    Write-Report "  CMF has not been initialized. Run yuki.exe first." "Yellow"
} else {
    $dbSize = (Get-Item $DbPath).Length
    Write-Report ("  [PASS] Database found: {0:N0} bytes ({1:N2} MB)" -f $dbSize, ($dbSize/1MB)) "Green"

    # Table list
    Write-Report ""
    Write-Report "  Tables:"
    $tables = Invoke-SqliteQuery ".tables" "N/A"
    foreach ($t in ($tables -split '\s+' | Where-Object { $_ })) {
        $count = Invoke-SqliteQuery "SELECT COUNT(*) FROM [$t];" "0"
        Write-Report ("    - {0,-25} {1,8} rows" -f $t, $count) "White"
    }

    # Schema details
    Write-Report ""
    Write-Report "  Schema Details:"
    $schema = Invoke-SqliteQuery ".schema" "N/A"
    if ($schema -ne "N/A") {
        $schemaLines = $schema -split '
' | Where-Object { $_ -match 'CREATE' }
        foreach ($line in $schemaLines) {
            Write-Report ("    {0}" -f $line.Trim()) "Gray"
        }
    }

    # Sample episodes
    Write-Report ""
    Write-Report "  Latest 5 Episodes:"
    $latest = Invoke-SqliteQuery "SELECT timestamp_ms, source, substr(text,1,60), intent_label, confidence FROM episodes ORDER BY timestamp_ms DESC LIMIT 5;" "N/A"
    if ($latest -ne "N/A") {
        foreach ($row in ($latest -split '
' | Where-Object { $_ })) {
            Write-Report ("    {0}" -f $row) "White"
        }
    }

    # Episode statistics
    Write-Report ""
    Write-Report "  Episode Statistics:"
    $sources = Invoke-SqliteQuery "SELECT source, COUNT(*) FROM episodes GROUP BY source;" "N/A"
    if ($sources -ne "N/A") {
        foreach ($s in ($sources -split '
' | Where-Object { $_ })) {
            Write-Report ("    {0}" -f $s) "White"
        }
    }

    # Concept graph stats
    Write-Report ""
    Write-Report "  Concept Graph Statistics:"
    $conceptCount = Invoke-SqliteQuery "SELECT COUNT(*) FROM concepts;" "0"
    $edgeCount = Invoke-SqliteQuery "SELECT COUNT(*) FROM concept_edges;" "0"
    Write-Report ("    Concepts: {0}" -f $conceptCount) "White"
    Write-Report ("    Edges:    {0}" -f $edgeCount) "White"

    # Top concepts by strength
    if ([int]$conceptCount -gt 0) {
        Write-Report ""
        Write-Report "  Top 10 Concepts by Strength:"
        $topConcepts = Invoke-SqliteQuery "SELECT name, type, strength, access_count FROM concepts ORDER BY strength DESC LIMIT 10;" "N/A"
        if ($topConcepts -ne "N/A") {
            foreach ($c in ($topConcepts -split '
' | Where-Object { $_ })) {
                Write-Report ("    {0}" -f $c) "White"
            }
        }
    }

    # Sample edges
    if ([int]$edgeCount -gt 0) {
        Write-Report ""
        Write-Report "  Sample Concept Edges:"
        $edges = Invoke-SqliteQuery "SELECT c1.name, e.relation_type, c2.name, e.weight FROM concept_edges e JOIN concepts c1 ON e.from_id=c1.id JOIN concepts c2 ON e.to_id=c2.id LIMIT 5;" "N/A"
        if ($edges -ne "N/A") {
            foreach ($e in ($edges -split '
' | Where-Object { $_ })) {
                Write-Report ("    {0}" -f $e) "White"
            }
        }
    }
}
Write-Report ""

# ============================================================================
# SECTION 4: BUILD AUDIT
# ============================================================================
Write-Report "[4] BUILD AUDIT" "Yellow"
Write-Report "-------------------------------------------------------------------"

$BuildPaths = @(
    "$RootDiruild\Release\yuki.exe",
    "$RootDiruild\yuki.exe",
    "$RootDiruild\Debug\yuki.exe"
)

$FoundExe = $null
foreach ($bp in $BuildPaths) {
    if (Test-Path $bp) {
        $FoundExe = Get-Item $bp
        break
    }
}

if ($FoundExe) {
    $age = (Get-Date) - $FoundExe.LastWriteTime
    Write-Report ("  [PASS] yuki.exe found: {0}" -f $FoundExe.FullName) "Green"
    Write-Report ("  Size: {0:N0} bytes" -f $FoundExe.Length) "White"
    Write-Report ("  Age:  {0:N0} minutes old" -f $age.TotalMinutes) $(if ($age.TotalMinutes -lt 60) { "Green" } else { "Yellow" })
} else {
    Write-Report "  [FAIL] yuki.exe NOT FOUND in build directories" "Red"
    Write-Report "  Run: cmake --build build --config Release" "Yellow"
}
Write-Report ""

# ============================================================================
# SECTION 5: VECTORSTORE INDEX AUDIT
# ============================================================================
Write-Report "[5] VECTORSTORE INDEX AUDIT" "Yellow"
Write-Report "-------------------------------------------------------------------"

$IndexPath = "$RootDir\datarain\cmf_vectors.index"
$MetaPath = "$RootDir\datarain\cmf_vectors.meta"

if (Test-Path $IndexPath) {
    $idxSize = (Get-Item $IndexPath).Length
    Write-Report ("  [PASS] HNSW index found: {0:N0} bytes ({1:N2} MB)" -f $idxSize, ($idxSize/1MB)) "Green"
} else {
    Write-Report "  [WARN] HNSW index NOT FOUND: $IndexPath" "Yellow"
    Write-Report "  Vector search not yet active (time-based fallback in use)" "Yellow"
}

if (Test-Path $MetaPath) {
    $metaSize = (Get-Item $MetaPath).Length
    $metaLines = (Get-Content $MetaPath | Measure-Object).Count
    Write-Report ("  [PASS] Metadata file found: {0:N0} bytes, {1} entries" -f $metaSize, $metaLines) "Green"
} else {
    Write-Report "  [WARN] Metadata file NOT FOUND: $MetaPath" "Yellow"
}
Write-Report ""

# ============================================================================
# SECTION 6: PHASE 1.5 READINESS CHECK
# ============================================================================
Write-Report "[6] PHASE 1.5 READINESS CHECK" "Yellow"
Write-Report "-------------------------------------------------------------------"

$Readiness = @{
    "CMF Core Files" = $AllPresent
    "Database Initialized" = (Test-Path $DbPath)
    "Episodes Table Has Data" = ((Invoke-SqliteQuery "SELECT COUNT(*) FROM episodes;" "0") -gt 0)
    "Concepts Table Has Data" = ((Invoke-SqliteQuery "SELECT COUNT(*) FROM concepts;" "0") -gt 0)
    "Build Exists" = ($null -ne $FoundExe)
    "VectorStore.h Exists" = (Test-Path "$RootDir\srcrainetrieval\VectorStore.h")
    "VectorStore.cpp Exists" = (Test-Path "$RootDir\srcrainetrieval\VectorStore.cpp")
}

$ReadyCount = 0
foreach ($check in $Readiness.Keys) {
    $pass = $Readiness[$check]
    $status = if ($pass) { "[PASS]" } else { "[FAIL]" }
    $color = if ($pass) { "Green" } else { "Red" }
    Write-Report ("  {0} {1}" -f $status, $check) $color
    if ($pass) { $ReadyCount++ }
}

Write-Report ""
$percent = [int]($ReadyCount / $Readiness.Count * 100)
$readyColor = if ($percent -ge 80) { "Green" } elseif ($percent -ge 50) { "Yellow" } else { "Red" }
Write-Report ("  READINESS: {0}/{1} checks passed ({2}%)" -f $ReadyCount, $Readiness.Count, $percent) $readyColor

if ($percent -ge 80) {
    Write-Report ""
    Write-Report "  >>> READY FOR PHASE 1.5: Vector Search Integration <<<" "Green"
    Write-Report "  Next steps:" "White"
    Write-Report "    1. Wire VectorStore::search() into EpisodicStore::retrieveSimilar()" "White"
    Write-Report "    2. Add retrieveContextForQuery() to CognitiveMemoryFabric" "White"
    Write-Report "    3. Inject CMF context into TurnCoordinator response generation" "White"
} else {
    Write-Report ""
    Write-Report "  >>> PHASE 1 INCOMPLETE - Fix failures before Phase 1.5 <<<" "Red"
}

Write-Report ""
Write-Report "===================================================================" "Cyan"
Write-Report "  END OF AUDIT" "Cyan"
Write-Report "  Report saved to: $ReportPath" "Cyan"
Write-Report "===================================================================" "Cyan"

Write-Host ""
Write-Host "Audit complete! Report saved to:" -ForegroundColor Green
Write-Host $ReportPath -ForegroundColor Cyan
Write-Host ""
if ($percent -ge 80) {
    Write-Host "PHASE 1.5 READY: Run the Phase 1.5 Gemini prompt now." -ForegroundColor Green -BackgroundColor Black
} else {
    Write-Host "Fix failures above before proceeding to Phase 1.5." -ForegroundColor Red -BackgroundColor Black
}
