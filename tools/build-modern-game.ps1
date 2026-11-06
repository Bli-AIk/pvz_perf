param([switch]$StageRuntime,[string]$BuildDirectory,
    [ValidateSet('WIN7','WIN10')][string]$WindowsPlatform='WIN10',
    [string]$MsvcToolsetDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$vs='C:\UserProgram\Visual Studuio\2026\Community\VC\Auxiliary\Build\vcvars64.bat'
$cmake=(Get-Command cmake -ErrorAction Stop).Source
$ninja='C:\Users\Administrator\AppData\Local\Programs\CLion\bin\ninja\win\x64\ninja.exe'
$build=if($BuildDirectory){[IO.Path]::GetFullPath($BuildDirectory)}elseif($WindowsPlatform -eq 'WIN7'){Join-Path $root 'out\x64-win7-vulkan'}else{Join-Path $root 'out\x64-win32-vulkan'}
[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity=[IntPtr]0x3FFF
$installed=Join-Path (Split-Path -Parent $root) '_shared\vcpkg_installed'
$env:VULKAN_SDK='C:\UserProgram\VulkanSDK\1.4.350.0'
if($WindowsPlatform -eq 'WIN7' -and !$MsvcToolsetDirectory){
    $MsvcToolsetDirectory=Join-Path $root 'out\toolchains\msvc-14.44\Contents\VC\Tools\MSVC\14.44.35207'
}
$toolsetEnvironment=''
$toolsetOptions=''
if($MsvcToolsetDirectory){
    $toolset=(Resolve-Path -LiteralPath $MsvcToolsetDirectory).Path
    $compiler=Join-Path $toolset 'bin\Hostx64\x64\cl.exe'
    if(!(Test-Path -LiteralPath $compiler)){throw "MSVC toolset compiler missing: $compiler"}
    $toolsetEnvironment='set "PATH={0}\bin\Hostx64\x64;!PATH!" && set "INCLUDE={0}\include;!INCLUDE!" && set "LIB={0}\lib\x64;!LIB!" && ' -f $toolset
    $toolsetOptions=' -DCMAKE_C_COMPILER="{0}" -DCMAKE_CXX_COMPILER="{0}" -DCMAKE_LINKER="{1}\bin\Hostx64\x64\link.exe" -DCMAKE_AR="{1}\bin\Hostx64\x64\lib.exe"' -f $compiler,$toolset
}
$configure='call "{0}" >nul && {7}"{1}" -S "{2}" -B "{3}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="{4}" -DVCPKG_INSTALLED_DIR="{5}" -DVCPKG_MANIFEST_INSTALL=OFF -DPVZ_BUILD_FRAMEWORK_VULKAN=ON -DPVZ_BUILD_FRAMEWORK_D3D12=OFF -DPVZ_WINDOW_BACKEND=WIN32 -DPVZ_WINDOWS_PLATFORM={6} -DPVZ_RENDER_BACKEND=VULKAN{8}' -f $vs,$cmake,$root,$build,$ninja,$installed,$WindowsPlatform,$toolsetEnvironment,$toolsetOptions
& $env:ComSpec /d /v:on /s /c $configure
if($LASTEXITCODE -ne 0){throw "Modern configure failed: $LASTEXITCODE"}
$compile='call "{0}" >nul && {1}"{2}" --build "{3}" --target LawnProject --parallel 12' -f $vs,$toolsetEnvironment,$cmake,$build
& $env:ComSpec /d /v:on /s /c $compile
if($LASTEXITCODE -ne 0){throw "Modern game build failed: $LASTEXITCODE"}
# The game POST_BUILD copy is not rerun when only the converter relinks.
# Keep the playable directory current for converter-only source fixes too.
& $cmake -E copy_if_different (Join-Path $root 'tools/bin/windows-x64/pvz_dat64_converter.exe') (Join-Path $build 'Release\data\pvz_dat64_converter.exe')
if($LASTEXITCODE -ne 0){throw "Modern converter staging failed: $LASTEXITCODE"}
if($StageRuntime){
    $source=Join-Path $root 'out\x64-win32-dx12\Release'
    $dest=Join-Path $build 'Release'
    foreach($file in @('main.pak64','bass.dll')){
        if(!(Test-Path -LiteralPath (Join-Path $dest $file))){
            Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $dest $file)
        }
    }
    if(!(Test-Path -LiteralPath (Join-Path $dest 'scale.json'))){
        Copy-Item -LiteralPath (Join-Path $root 'scale.json') -Destination (Join-Path $dest 'scale.json')
    }
    Copy-Item -LiteralPath (Join-Path $root 'tools\start-lp14.cmd') -Destination (Join-Path $dest 'start-lp14.cmd') -Force
}
