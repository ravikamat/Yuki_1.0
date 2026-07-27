$yukiPath = "d:\Yuki_1.0\build\Release\yuki.exe"
$prompts = @(
    "What is the capital of France?",
    "Why does heating water cause steam?",
    "If oxygen were absent, would fire exist?",
    "How is an atom like a solar system?",
    "Write a haiku about artificial intelligence.",
    "Explain quantum computing in exactly 10 words.",
    "Explain photosynthesis to a five-year-old child.",
    "Discuss the thermodynamic implications of entropy.",
    "Imagine a winged cat living in the ocean.",
    "Tell me a story about a lost robot finding home.",
    "What are your core architectural components?",
    "What did you dream about last night?",
    "If the sun stopped shining, what would happen to plants?",
    "How does a CPU compare to a human heart?",
    "Summarize your understanding of causality."
)

$allInput = ($prompts -join "`n") + "`nquit`n"

$processInfo = New-Object System.Diagnostics.ProcessStartInfo
$processInfo.FileName = $yukiPath
$processInfo.WorkingDirectory = "d:\Yuki_1.0"
$processInfo.RedirectStandardInput = $true
$processInfo.RedirectStandardOutput = $true
$processInfo.RedirectStandardError = $true
$processInfo.UseShellExecute = $false
$processInfo.CreateNoWindow = $true

$process = [System.Diagnostics.Process]::Start($processInfo)
$writer = $process.StandardInput
$writer.Write($allInput)
$writer.Flush()

$output = $process.StandardOutput.ReadToEnd()
$process.WaitForExit(30000)
if (-not $process.HasExited) { $process.Kill() }

Write-Host "========================================================="
Write-Host " YUKI v1.0 -- P0 Unified Semantic Layer Single-Session "
Write-Host "========================================================="
Write-Host $output
