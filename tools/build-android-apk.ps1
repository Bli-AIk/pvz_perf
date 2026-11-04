param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\out\android-arm64'),
    [string]$PakFile = "",
    [string]$BassLibrary = '',
    [string]$OutputDirectory = '',
    [string]$JavaHome = $env:JAVA_HOME,
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [string]$AndroidSdk = $env:ANDROID_HOME,
    [switch]$Sign,
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path $BuildDirectory -ErrorAction SilentlyContinue)
if (!$build) { throw "Android build directory does not exist: $BuildDirectory" }
$build = $build.Path
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
$legacy = $cache -match '(?m)^PVZ_VULKAN_LEGACY:BOOL=ON\r?$'
$hdrTest = if ($cache -match '(?m)^PVZ_ANDROID_HDR_TEST:BOOL=ON\r?$') { 'true' } else { 'false' }
$platform = if ($legacy) { 'arm64-android7-vulkan1.0' } else { 'arm64-android14-vulkan1.3' }
$minSdk = if ($legacy) { 24 } else { 34 }
if ($legacy -and $cache -notmatch '(?m)^ANDROID_PLATFORM:[^=]+=android-24\r?$') {
    throw 'The Vulkan 1.0 package must use an Android API 24 native build.'
}
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root "out\$platform" }
# Resolve while still in the caller's directory; Gradle runs after Push-Location.
$OutputDirectory = [IO.Path]::GetFullPath([IO.Path]::Combine($PWD.Path, $OutputDirectory))
if (!$PakFile) { $PakFile = Join-Path $build "Release\main.pak64" }
if (!(Test-Path -LiteralPath $PakFile)) { throw "main.pak64 not found: $PakFile" }
$project = Join-Path $root 'android'
$staging = Join-Path $build 'apk-staging'
$assetDir = Join-Path $staging 'assets'
$jniDir = Join-Path $staging 'jniLibs\arm64-v8a'
$bundledSource = Join-Path $project 'app\src\main\assets\bundled-saves'
$bundledNames = @('users.dat','user1.dat64','game1_58.dat64')
foreach ($name in $bundledNames) {
    if (!(Test-Path -LiteralPath (Join-Path $bundledSource $name))) {
        throw "Bundled startup save is missing: $name"
    }
}
if (!$BassLibrary) { $BassLibrary = Join-Path $build 'Release\libbass.so' }
if (!(Test-Path -LiteralPath $BassLibrary)) {
    $BassLibrary = Join-Path $project 'app\src\main\jniLibs\arm64-v8a\libbass.so'
}
if (!(Test-Path -LiteralPath $BassLibrary)) {
    throw 'Android BASS is required; stage Release/libbass.so or pass -BassLibrary.'
}
$shaderSource = Join-Path $build 'Release\shaders'
$shaderFiles = @(Get-ChildItem -LiteralPath $shaderSource -Filter '*.spv' -File -ErrorAction Stop | Where-Object { $_.Name -ne 'game_hdr_ps.spv' })
foreach ($required in @('game_vs.spv', 'game_ps.spv', 'game_sprite_ps.spv', 'game_sprite_mixed_ps.spv')) {
    if (!(Test-Path -LiteralPath (Join-Path $shaderSource $required))) {
        throw "Required Vulkan shader is missing: $required"
    }
}
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'pvzPerf/SexyAppFramework/platform/android/default-scale.json') -Destination (Join-Path $assetDir 'scale.json') -Force
New-Item -ItemType Directory -Force -Path $jniDir | Out-Null
Copy-Item -LiteralPath $BassLibrary -Destination (Join-Path $jniDir 'libbass.so') -Force
$bundledAssets = Join-Path $assetDir 'bundled-saves'
New-Item -ItemType Directory -Force -Path $bundledAssets | Out-Null
foreach ($name in $bundledNames) {
    Copy-Item -LiteralPath (Join-Path $bundledSource $name) -Destination (Join-Path $bundledAssets $name) -Force
}
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
    & $gradle (':app:assemble' + $Configuration) "-PpvzMinSdk=$minSdk" "-PpvzStagingDir=$staging" "-PpvzHdrTest=$hdrTest"
    if ($LASTEXITCODE -ne 0) { throw "Gradle failed with exit code $LASTEXITCODE" }
    $apkDirectory = Join-Path $project "app\build\outputs\apk\$($Configuration.ToLower())"
    $metadata = Get-Content -LiteralPath (Join-Path $apkDirectory 'output-metadata.json') -Raw | ConvertFrom-Json
    $outputs = @($metadata.elements)
    if ($outputs.Count -ne 1 -or !$outputs[0].outputFile) {
        throw 'Expected exactly one APK in the current Gradle output metadata.'
    }
    $apk = Join-Path $apkDirectory $outputs[0].outputFile
    if (!(Test-Path -LiteralPath $apk -PathType Leaf)) { throw 'The APK declared by Gradle is missing.' }
    $archive = [IO.Compression.ZipFile]::OpenRead($apk)
    try {
        foreach ($entry in @('lib/arm64-v8a/libpvz_perf.so','lib/arm64-v8a/libbass.so',
                'assets/main.pak64','assets/scale.json') + @($bundledNames | ForEach-Object { "assets/bundled-saves/$_" })) {
            $file = $archive.GetEntry($entry)
            if (!$file -or $file.Length -eq 0) { throw "APK runtime dependency is missing or empty: $entry" }
        }
    } finally { $archive.Dispose() }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $suffix = if ($apk.EndsWith('-unsigned.apk')) { '-unsigned' } else { '' }
    if ($suffix -and ($Sign -or $Install)) {
        if (!$AndroidSdk) { $AndroidSdk = 'C:\UserProgram\DevTools\AndroidSdk' }
        $buildTools = Get-ChildItem -LiteralPath (Join-Path $AndroidSdk 'build-tools') -Directory |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'apksigner.bat') } |
            Sort-Object Name -Descending | Select-Object -First 1
        if (!$buildTools) { throw 'Android build-tools are required for APK signing.' }
        $key = Join-Path $env:USERPROFILE '.android\debug.keystore'
        if (!(Test-Path -LiteralPath $key)) { throw 'Development signing requires ~/.android/debug.keystore.' }
        $aligned = Join-Path $OutputDirectory ("pvz_perf-$platform-aligned.apk")
        $signed = Join-Path $OutputDirectory ("pvz_perf-$platform-" + $Configuration.ToLower() + '.apk')
        & (Join-Path $buildTools.FullName 'zipalign.exe') -P 16 -f 4 $apk $aligned
        if ($LASTEXITCODE -ne 0) { throw 'APK alignment failed.' }
        & (Join-Path $buildTools.FullName 'apksigner.bat') sign --ks $key --ks-key-alias androiddebugkey `
            --ks-pass pass:android --key-pass pass:android --out $signed $aligned
        if ($LASTEXITCODE -ne 0) { throw 'APK signing failed.' }
        & (Join-Path $buildTools.FullName 'apksigner.bat') verify $signed
        if ($LASTEXITCODE -ne 0) { throw 'APK signature verification failed.' }
        Remove-Item -LiteralPath $aligned
        $apk=$signed
        $suffix=''
    }
    $outputApk = Join-Path $OutputDirectory ("pvz_perf-" + $platform + '-' + $Configuration.ToLower() + $suffix + '.apk')
    if ([IO.Path]::GetFullPath($apk) -ne [IO.Path]::GetFullPath($outputApk)) {
        Copy-Item -LiteralPath $apk -Destination $outputApk -Force
    }
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
