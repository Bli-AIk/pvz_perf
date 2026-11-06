param(
    [string]$Emsdk = 'C:/UserProgram/DevTools/emsdk',
    [string]$OutputDirectory = 'out/wasm-webgpu/toy',
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $sources = @('Tests/native/WebGpuRuntimeProbe.cpp',
        'pvzPerf/SexyAppFramework/graphics/webgpu/WebGPUContext.cpp',
        'pvzPerf/SexyAppFramework/graphics/webgpu/WebGPURenderer.cpp',
        'pvzPerf/SexyAppFramework/graphics/webgpu/WebGPUShaders.cpp',
        'pvzPerf/SexyAppFramework/graphics/PoolMesh.cpp')
    & "$Emsdk/upstream/emscripten/em++.exe" --use-port=emdawnwebgpu -std=c++23 -Oz -fexceptions `
        -DPVZ_GPU_WEBGPU=1 -DSEXY_WINDOW_BROWSER=1 -I . -I pvzPerf -I pvzPerf/SexyAppFramework `
        -I pvzPerf/SexyAppFramework/graphics @sources -sASYNCIFY=1 -sALLOW_MEMORY_GROWTH=1 `
        -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 "-sEXPORTED_RUNTIME_METHODS=['callMain']" -o "$OutputDirectory/gpu-probe.js"
    if ($LASTEXITCODE) { throw 'WebGPU runtime probe compilation failed.' }
    Set-Content -LiteralPath "$OutputDirectory/gpu-probe.html" -Encoding utf8 -Value '<!doctype html><link rel="icon" href="data:,"><canvas id="canvas" width="64" height="64"></canvas><script>var Module={onRuntimeInitialized(){Module.callMain([])}};</script><script src="gpu-probe.js"></script>'
    if ($Run) {
        & node tools/wasm/verify_webgpu_runtime.mjs
        if ($LASTEXITCODE) { throw 'WebGPU runtime probe failed. Serve the output directory on localhost:18089.' }
    }
} finally { Pop-Location }
