param(
    [int]$ProcessId = 0,
    [long]$AffinityMask = 0x4000,
    [switch]$PerCoreCpu,
    [switch]$VerifyProjectEndless,
    [int]$Seconds = 0,
    [ValidateSet('11x','25x')][string]$CpuMultiplier = '11x',
    [string]$OutputDirectory = '',
    [switch]$NoPrompt
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'out/x64-win32-vulkan'
$executable = (Resolve-Path (Join-Path $build 'Release/pvz_perf.exe')).Path
if (!$ProcessId) {
    $candidates = @(Get-Process pvz_perf -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable })
    if ($candidates.Count -ne 1) { throw 'Launch one Vulkan Release instance, or provide -ProcessId.' }
    $ProcessId = $candidates[0].Id
}
$process = Get-Process -Id $ProcessId
if ($process.Path -ne $executable) { throw 'The PID is not the current Vulkan Release executable.' }
if ($process.ProcessorAffinity.ToInt64() -ne $AffinityMask) {
    throw ("Expected affinity mask 0x{0:X}." -f $AffinityMask)
}
if ($Seconds -lt 0) { throw 'Measurement duration cannot be negative; zero means until game exit.' }
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $env:TEMP ('pvz-vulkan-' + $CpuMultiplier + '-' + [Guid]::NewGuid().ToString('N'))
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$layoutTool = Join-Path $build 'Tests/performance/build/pvz_game_perf_layout.exe'
$layout = (& $layoutTool) | ConvertFrom-Json -AsHashtable
if ($LASTEXITCODE -ne 0) { throw 'Cannot read the current native layout.' }
$layout['executable_sha256'] = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
$layoutFile = Join-Path $OutputDirectory 'game-layout.json'
$layout | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $layoutFile -Encoding utf8
$measurementDirectory = Join-Path $OutputDirectory 'measurement'
if (Test-Path -LiteralPath $measurementDirectory) {
    $measurementDirectory = Join-Path $OutputDirectory ('measurement-' + [Guid]::NewGuid().ToString('N'))
}
# External read-only sampling; closing the observer does not stop the game.
$probeArguments = @((Join-Path $root 'Tests/performance/windows_game_probe.py'), $ProcessId,
    '--kind', 'project', '--layout', $layoutFile, '--scene', 'endless',
    '--multiplier', $CpuMultiplier, '--output', $measurementDirectory, '--preroll', 3)
$probeArguments += @('--affinity', ('0x{0:X}' -f $AffinityMask))
if ($PerCoreCpu) { $probeArguments += '--per-core-cpu' }
if ($VerifyProjectEndless) { $probeArguments += '--verify-project-endless' }
if ($Seconds) { $probeArguments += @('--seconds', $Seconds) }
else { $probeArguments += '--until-exit' }
if (!$NoPrompt) {
    Write-Host 'Prepare the endless Continue/Paused screen. Press Enter here, then resume battle at t=3 seconds.'
    Write-Host 'Sampling continues until you Quit the game. Enter does not stop observation.'
    $null = Read-Host 'Press Enter to arm the measurement'
}
& python @probeArguments
if ($LASTEXITCODE -ne 0) { throw 'External performance observation failed.' }
Write-Output ('Measurement saved to ' + $measurementDirectory)
