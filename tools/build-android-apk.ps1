param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\out\android-arm64'),
    [string]$PakFile = "",
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\out\arm64-android14-vulkan1.3'),
    [string]$JavaHome = $env:JAVA_HOME,
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
# Resolve while still in the caller's directory; Gradle runs after Push-Location.
$OutputDirectory = [IO.Path]::GetFullPath([IO.Path]::Combine($PWD.Path, $OutputDirectory))
$build = (Resolve-Path $BuildDirectory -ErrorAction SilentlyContinue)
if (!$build) { throw "Android build directory does not exist: $BuildDirectory" }
$build = $build.Path
if (!$PakFile) { $PakFile = Join-Path $build "Release\main.pak64" }
if (!(Test-Path -LiteralPath $PakFile)) { throw "main.pak64 not found: $PakFile" }
$project = Join-Path $root 'android'
$assetDir = Join-Path $project 'app\src\main\assets'
$jniDir = Join-Path $project 'app\src\main\jniLibs\arm64-v8a'
$shaderSource = Join-Path $build 'Release\shaders'
$shaderFiles = @(Get-ChildItem -LiteralPath $shaderSource -Filter '*.spv' -File -ErrorAction Stop | Where-Object { $_.Name -notin @('game_hdr_ps.spv','game_hdr_linear_ps.spv') })
foreach ($required in @('game_vs.spv', 'game_ps.spv', 'game_sprite_ps.spv', 'game_sprite_mixed_ps.spv')) {
    if (!(Test-Path -LiteralPath (Join-Path $shaderSource $required))) {
        throw "Required Vulkan shader is missing: $required"
    }
}
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'pvzPerf/SexyAppFramework/platform/android/default-scale.json') -Destination (Join-Path $assetDir 'scale.json') -Force
New-Item -ItemType Directory -Force -Path $jniDir | Out-Null
$shaderAssets = Join-Path $assetDir 'shaders'
New-Item -ItemType Directory -Force -Path $shaderAssets | Out-Null
# Replace this build-owned SPIR-V set, including removal of retired shaders.
Get-ChildItem -LiteralPath $shaderAssets -Filter '*.spv' -File | Remove-Item -Force
foreach ($shader in $shaderFiles) {
    Copy-Item -LiteralPath $shader.FullName -Destination (Join-Path $shaderAssets $shader.Name) -Force
}
Copy-Item -LiteralPath $PakFile -Destination (Join-Path $assetDir 'main.pak64') -Force
$nativeLibrary = Join-Path $build 'Release\libpvz_perf.so'
if (!(Test-Path -LiteralPath $nativeLibrary)) { throw "Android native library not found: $nativeLibrary" }
Copy-Item -LiteralPath $nativeLibrary -Destination (Join-Path $jniDir 'libpvz_perf.so') -Force
$gradle = Get-Command gradlew.bat -ErrorAction SilentlyContinue
if (!$gradle) { $gradle = Get-Command gradle -ErrorAction SilentlyContinue }
if (!$gradle) {
    $cachedGradle = Get-ChildItem "$env:USERPROFILE\.gradle\wrapper\dists" -Recurse -Filter gradle.bat -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($cachedGradle) { $gradle = $cachedGradle }
}
if (!$gradle) { throw 'Gradle is required. The cached Gradle distribution was not found.' }
# 缓存分发命中的是 FileInfo（无 .Source）；Get-Command 命中的是 ApplicationInfo。
$gradle = if ($gradle.Source) { $gradle.Source } else { $gradle.FullName }
if (!$JavaHome) {
    $jdk = Get-ChildItem 'C:\Program Files\Java' -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'bin\java.exe') } |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($jdk) { $JavaHome = $jdk.FullName }
}
if (!$JavaHome -or !(Test-Path -LiteralPath (Join-Path $JavaHome 'bin\javac.exe'))) {
    throw 'A complete JDK is required; specify -JavaHome or JAVA_HOME.'
}
$previousJavaHome = $env:JAVA_HOME
$previousJavaOptions = $env:JAVA_TOOL_OPTIONS
# JDK 25 uses AF_UNIX for selector wakeups on Windows. A short, normal path
# avoids Invalid argument: connect with this machine's redirected TEMP path.
$socketDir = Join-Path $env:USERPROFILE '.gradle\uds'
New-Item -ItemType Directory -Force -Path $socketDir | Out-Null
$env:JAVA_HOME = $JavaHome
$env:JAVA_TOOL_OPTIONS = ($previousJavaOptions + ' "-Djdk.net.unixdomain.tmpdir=' + $socketDir + '"').Trim()
Push-Location $project
try {
    & $gradle (':app:assemble' + $Configuration)
    if ($LASTEXITCODE -ne 0) { throw "Gradle failed with exit code $LASTEXITCODE" }
    $apkDirectory = Join-Path $project "app\build\outputs\apk\$($Configuration.ToLower())"
    $metadata = Get-Content -LiteralPath (Join-Path $apkDirectory 'output-metadata.json') -Raw | ConvertFrom-Json
    $outputs = @($metadata.elements)
    if ($outputs.Count -ne 1 -or !$outputs[0].outputFile) {
        throw 'Expected exactly one APK in the current Gradle output metadata.'
    }
    $apk = Join-Path $apkDirectory $outputs[0].outputFile
    if (!(Test-Path -LiteralPath $apk -PathType Leaf)) { throw 'The APK declared by Gradle is missing.' }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $suffix = if ($apk.EndsWith('-unsigned.apk')) { '-unsigned' } else { '' }
    $outputApk = Join-Path $OutputDirectory ("pvz_perf-arm64-android14-vulkan1.3-" + $Configuration.ToLower() + $suffix + '.apk')
    Copy-Item -LiteralPath $apk -Destination $outputApk -Force
    Write-Output ([IO.Path]::GetFullPath($outputApk))
    if ($Install) {
        if ($suffix) { throw 'Sign the unsigned Release APK before installing it.' }
        adb install -r $outputApk
        if ($LASTEXITCODE -ne 0) { throw "adb install failed with exit code $LASTEXITCODE" }
    }
} finally {
    Pop-Location
    $env:JAVA_HOME = $previousJavaHome
    $env:JAVA_TOOL_OPTIONS = $previousJavaOptions
}
