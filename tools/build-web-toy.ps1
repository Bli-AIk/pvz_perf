param(
    [string]$Emsdk = 'C:/UserProgram/DevTools/emsdk',
    [string]$Python = 'C:/UserProgram/DevTools/Python314/python.exe',
    [string]$JsonInclude = '',
    [string]$SpngSource = 'C:/UserProgram/vcpkg/buildtrees/libspng/src/v0.7.4-e8b3878a48.clean/spng',
    [string]$WebConfig = '',
    [string]$PakFile = '',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $WebConfig) { $WebConfig = Join-Path $PSScriptRoot 'wasm/game/web-config.json' }
if (-not $JsonInclude) {
    $JsonInclude = Join-Path (Split-Path $projectRoot -Parent) '_shared/vcpkg_installed/x64-windows-static-md/include'
}
$webBuild = Join-Path $projectRoot 'out/wasm/game'
if (-not $PakFile) { $PakFile = Join-Path $projectRoot 'out/x64-win32-vulkan/Release/main.pak64' }
if (-not (Test-Path -LiteralPath $PakFile)) {
    throw 'Build the requested main.pak64 resource package first.'
}
& "$Emsdk/upstream/emscripten/emcmake.exe" cmake -S "$PSScriptRoot/wasm/game" -B $webBuild -G Ninja `
    -DCMAKE_BUILD_TYPE=MinSizeRel "-DPVZ_JSON_INCLUDE=$JsonInclude" "-DPVZ_SPNG_SOURCE=$SpngSource" "-DPVZ_WEB_PAK=$PakFile"
if ($LASTEXITCODE) { throw 'Web configure failed.' }
& cmake --build $webBuild --parallel $Jobs
if ($LASTEXITCODE) { throw 'Web compilation/link failed.' }
& $Python "$PSScriptRoot/wasm/package_toy.py" --build "$webBuild/site" --output "$projectRoot/out/wasm/toy" --config $WebConfig
if ($LASTEXITCODE) { throw 'Toy packaging failed.' }
& $Python "$PSScriptRoot/wasm/package_toy.py" --build "$webBuild/site" --output "$projectRoot/out/wasm/toy-bilibili" --config $WebConfig --bilibili
if ($LASTEXITCODE) { throw 'Bilibili Toy packaging failed.' }
