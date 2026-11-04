param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [string]$PakFile,
    [string]$OutputDirectory,
    [ValidateSet('all','plant-attacks','zombie-behavior','ice3','minigame-rules','puzzle-rules','survival-rules','adventure-rules','hidden-rules','garden-rules','intro-rules','ui-rules','award-rules','almanac-clock','actor-birth','loss-rules','opening-rules','lifecycle-rules','grave-rules','hud-seams','touch-input','touch-perf','landscape-ui')][string]$Case='all'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskBuild=(Resolve-Path -LiteralPath $BuildDirectory).Path
$taskRuntime=Join-Path $taskBuild 'Release'
if (!$PakFile) { $PakFile=Join-Path $taskRuntime 'main.pak64' }
$taskPak=(Resolve-Path -LiteralPath $PakFile).Path
$taskOutput=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $taskBuild 'image-runtime-tests'}
# This fixture decodes the real sound catalog. Catch a missing decoder before
# the native loader can open a modal error box in an unattended test process.
$taskBassCandidates=@(
    $env:PVZ_BASS_DLL
    (Join-Path $taskRuntime 'bass.dll')
    (Join-Path $taskOutput 'bass.dll')
    (Join-Path (Split-Path -Parent $taskOutput) 'bass.dll')
)
if (!@($taskBassCandidates | Where-Object {
    $_ -and (Test-Path -LiteralPath $_ -PathType Leaf)
}).Count) {
    throw 'Image runtime tests require bass.dll for sound decoding. Configure PVZ_BASS_DLL to stage it beside the game, or set the PVZ_BASS_DLL environment variable to its absolute path.'
}
$taskNinja='C:\Users\Administrator\AppData\Local\Programs\CLion\bin\ninja\win\x64\ninja.exe'
$taskVcvars='C:\UserProgram\Visual Studuio\2026\Community\VC\Auxiliary\Build\vcvars64.bat'
$taskCommands=@(& $taskNinja -C $taskBuild -t commands LawnProject)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the existing runtime build graph' }
$taskMain=Join-Path $taskRoot 'pvzPerf\main.cpp'
$taskCompile=@($taskCommands | Where-Object {
    $_ -match '/FoSexyAppFramework[\\/]CMakeFiles[\\/]LawnProject.dir[\\/]__[\\/]main.cpp.obj' })
$taskExpanded=@(& $taskNinja -C $taskBuild -t compdb -x | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw 'Cannot expand the runtime link response' }
$taskLink=@($taskExpanded | Where-Object {
    $_.command -match 'vs_link_exe.*pvz_perf.exe' } | ForEach-Object { $_.command })
if ($taskCompile.Count -ne 1 -or $taskLink.Count -ne 1) {
    throw 'Expected one game entry point and one runtime link command'
}
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskTestSource=Join-Path $taskRoot 'Tests\native\ImageRuntimeTests.cpp'
$taskObject=Join-Path $taskOutput 'ImageRuntimeTests.cpp.obj'
$taskExecutable=Join-Path $taskOutput 'image_runtime_tests.exe'
$taskCompileCommand=$taskCompile[0].Replace($taskMain,$taskTestSource)
$taskCompileCommand=[regex]::Replace($taskCompileCommand,'/Fo[^ ]+','/Fo"'+$taskObject+'"')
$taskCompileCommand=[regex]::Replace($taskCompileCommand,'/Fd[^ ]+','/Fd"'+(Join-Path $taskOutput 'compile.pdb')+'"')
$taskOpeningSource=Join-Path $taskRoot 'Tests\native\OpeningFlowRuntimeChecks.cpp'
$taskOpeningObject=Join-Path $taskOutput 'OpeningFlowRuntimeChecks.cpp.obj'
$taskOpeningCompileCommand=$taskCompileCommand.Replace($taskTestSource,$taskOpeningSource).Replace($taskObject,$taskOpeningObject)
$taskCompilerMatch=[regex]::Match($taskCompile[0],'^(?:"([^"]+)"|(\S+))')
$taskCompiler=if ($taskCompilerMatch.Groups[1].Success) {
    $taskCompilerMatch.Groups[1].Value
} else { $taskCompilerMatch.Groups[2].Value }
$taskToolset=Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $taskCompiler)))
$taskEnvironment='call "{0}" >nul && set "PATH={1}\bin\Hostx64\x64;!PATH!" && set "INCLUDE={1}\include;!INCLUDE!" && set "LIB={1}\lib\x64;!LIB!" && ' -f $taskVcvars,$taskToolset
$taskLinkCommand=$taskLink[0].Substring($taskLink[0].IndexOf(' -- ')+4)
$taskLinkCommand=($taskLinkCommand -split ' && ',2)[0]
$taskLinkCommand=$taskLinkCommand.Replace('SexyAppFramework\CMakeFiles\LawnProject.dir\__\main.cpp.obj','"'+$taskObject+'" "'+$taskOpeningObject+'"')
$taskLinkCommand=[regex]::Replace($taskLinkCommand,'/out:[^ ]+','/out:"'+$taskExecutable+'"','IgnoreCase')
$taskLinkCommand=[regex]::Replace($taskLinkCommand,'/pdb:[^ ]+','/pdb:"'+(Join-Path $taskOutput 'image_runtime_tests.pdb')+'"','IgnoreCase')
$taskLinkCommand=[regex]::Replace($taskLinkCommand,'/implib:[^ ]+','/implib:"'+(Join-Path $taskOutput 'image_runtime_tests.lib')+'"','IgnoreCase')
$taskLinkCommand=[regex]::Replace($taskLinkCommand,'/subsystem:windows','/subsystem:console','IgnoreCase')
$taskLinkMatch=[regex]::Match($taskLinkCommand,'^(?:"[^"]+link.exe"|\S*link.exe)')
if (!$taskLinkMatch.Success) { throw 'Cannot identify the runtime linker' }
$taskResponse=Join-Path $taskOutput 'link.rsp'
[IO.File]::WriteAllText($taskResponse,$taskLinkCommand.Substring($taskLinkMatch.Length),
    [Text.UTF8Encoding]::new($false))
$taskLinkCommand=$taskLinkMatch.Value+' @"'+$taskResponse+'"'
Push-Location $taskBuild
try {
    & $env:ComSpec /d /v:on /s /c ($taskEnvironment+$taskCompileCommand) |
        Where-Object { $_ -notmatch '^(Note: including file:|注意: 包含文件:)' }
    if ($LASTEXITCODE -ne 0) { throw 'Image runtime fixture compilation failed' }
    & $env:ComSpec /d /v:on /s /c ($taskEnvironment+$taskOpeningCompileCommand) |
        Where-Object { $_ -notmatch '^(Note: including file:|注意: 包含文件:)' }
    if ($LASTEXITCODE -ne 0) { throw 'Opening runtime fixture compilation failed' }
    & $env:ComSpec /d /v:on /s /c ($taskEnvironment+$taskLinkCommand)
    if ($LASTEXITCODE -ne 0) { throw 'Image runtime fixture link failed' }
}
finally { Pop-Location }
foreach ($taskDll in Get-ChildItem -LiteralPath $taskRuntime -Filter '*.dll' -File) {
    Copy-Item -LiteralPath $taskDll.FullName -Destination $taskOutput -Force
}
$taskShaders=Join-Path $taskOutput 'shaders'
$taskRuntimeShaders=Join-Path $taskRuntime 'shaders'
if ((Test-Path -LiteralPath $taskRuntimeShaders) -and !(Test-Path -LiteralPath $taskShaders)) {
    New-Item -ItemType Junction -Path $taskShaders -Target $taskRuntimeShaders | Out-Null
}
# Reuse the exact production objects and libraries. Only the entry point differs;
# no game window is shown, no player data is loaded, and no log files are written.
Push-Location $taskOutput
try {
    if($Case -eq 'all') { & $taskExecutable $taskPak }
    else { & $taskExecutable $taskPak $Case }
    if ($LASTEXITCODE -ne 0) { throw 'Image runtime regression failed' }
}
finally { Pop-Location }
