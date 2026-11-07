"""Stage a relative-path Toy site; keep the uncompressed Wasm in the build tree."""
import argparse
import gzip
import base64
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import runpy
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/wasm/game/site')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/wasm/toy')
    parser.add_argument('--config', type=Path, default=ROOT / 'tools/wasm/game/web-config.json')
    parser.add_argument('--version', help='Explicit preview version; defaults to the native product version')
    parser.add_argument('--bilibili', action='store_true', help='Disable API bypass; game files remain identical')
    parser.add_argument('--webgpu', action='store_true', help='Package WebGPU; optionally add selectable WebGL through --webgl-build')
    parser.add_argument('--webgl-build', type=Path, help='Include selectable WebGL runtime, sharing the same data package')
    args = parser.parse_args()
    version_source = (ROOT / 'pvzPerf/LawnApp.cpp').read_text(encoding='utf-8')
    version = args.version or re.search(r'mProductVersion\s*=\s*"([^"]+)"', version_source).group(1)
    config = json.loads(args.config.read_text(encoding='utf-8'))
    if not isinstance(config.get('bypassBilibiliChecks'), bool):
        raise ValueError('bypassBilibiliChecks must be a boolean')
    if not isinstance(config.get('rewardVideoBvid'), str) or not re.fullmatch(r'BV[0-9A-Za-z]{10}',config['rewardVideoBvid']):
        raise ValueError('rewardVideoBvid must be one BV string')
    if args.bilibili:
        config['bypassBilibiliChecks'] = False
    required = ['pvz-perf.js', 'pvz-perf.wasm', 'pvz-perf.data']
    for name in required:
        if not (args.build / name).is_file():
            raise FileNotFoundError(args.build / name)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "save-tool.html").unlink(missing_ok=True)
    support_files = ['index.html', 'launcher.js', 'launcher.css', 'rewards.js', 'lineups.js', 'frame-rate.js', 'graphics.js', 'save-tools.js', 'save-tool-runtime.js', 'save-editor-host.js', 'progress-storage.js', 'text-input.js', 'serve.py',
                     'start-local.cmd', 'start-local.sh', 'README.txt']
    for name in support_files:
        shutil.copyfile(ROOT / 'tools/wasm/game' / name, args.output / name)
    # The HTML tool is built and maintained in PvzDevTools; ship its artifact.
    shutil.copyfile(ROOT / 'tools/bin/web/save-editor.html', args.output / 'save-editor.html')
    shutil.copyfile(ROOT / 'tools/bin/web/manifest.json', args.output / 'save-editor-manifest.json')
    shutil.copyfile(ROOT / 'tools/bin/web/save-codec.js', args.output / 'save-codec.js')
    support_files += ['save-editor.html', 'save-editor-manifest.json', 'save-codec.js']
    readme = args.output / 'README.txt'
    text = readme.read_text(encoding='utf-8')
    readme.write_text('pvz_perf Web v' + version + '\n' + text.split('\n', 1)[1], encoding='utf-8')
    (args.output / 'web-config.js').write_text('window.PVZWebConfig = ' + json.dumps(config, ensure_ascii=False, indent=2) + ';\n', encoding='utf-8')
    for name in ['pvz-perf.js', 'pvz-perf.data']:
        shutil.copyfile(args.build / name, args.output / name)
    (args.output / 'pvz-perf.wasm.gz').write_bytes(gzip.compress((args.build / 'pvz-perf.wasm').read_bytes(), compresslevel=9, mtime=0))
    files = support_files + ['web-config.js', 'pvz-perf.js', 'pvz-perf.data', 'pvz-perf.wasm.gz']
    if args.webgl_build:
        if not args.webgpu:
            raise ValueError('--webgl-build requires --webgpu')
        if hashlib.sha256((args.webgl_build / 'pvz-perf.data').read_bytes()).digest() != hashlib.sha256((args.build / 'pvz-perf.data').read_bytes()).digest():
            raise ValueError('WebGL and WebGPU must use the same resource package')
        shutil.copyfile(args.webgl_build / 'pvz-perf.js', args.output / 'pvz-perf-webgl.js')
        (args.output / 'pvz-perf-webgl.wasm.gz').write_bytes(gzip.compress((args.webgl_build / 'pvz-perf.wasm').read_bytes(), compresslevel=9, mtime=0))
        files += ['pvz-perf-webgl.js', 'pvz-perf-webgl.wasm.gz']
    graphics = {'version': version, 'webgpu': args.webgpu, 'fallback': bool(args.webgl_build)}
    (args.output / 'graphics-config.js').write_text('window.PVZGraphicsConfig = ' + json.dumps(graphics) + ';\n', encoding='utf-8')
    files.append('graphics-config.js')
    license_path = args.output / 'licenses/stb-vorbis.txt'
    license_path.parent.mkdir(exist_ok=True)
    shutil.copyfile(ROOT / 'tools/wasm/licenses/stb-vorbis.txt', license_path)
    files.append('licenses/stb-vorbis.txt')
    if args.webgpu:
        license_path = args.output / 'licenses/emdawnwebgpu.txt'
        license_path.parent.mkdir(exist_ok=True)
        shutil.copyfile(ROOT / 'tools/wasm/licenses/emdawnwebgpu.txt', license_path)
        files.append('licenses/emdawnwebgpu.txt')
    decoder = ROOT / 'out/dependencies/libopenmpt-web-0.8.10'
    runtime = decoder / 'bin/wasm/libopenmpt.js'
    if not runtime.is_file():
        raise FileNotFoundError('Install official libopenmpt 0.8.10 Web bundle at ' + str(decoder))
    audio = args.output / 'audio'
    audio.mkdir(exist_ok=True)
    for name in ['openmpt-runtime.mjs', 'music-engine.mjs', 'music-mixer.mjs', 'music-worklet.mjs']:
        (audio / name).unlink(missing_ok=True)
    (audio / 'openmpt-runtime.js').write_text(
        'export function createOpenMpt(libopenmpt){\n' + runtime.read_text() +
        '\nif(libopenmpt.randomFill)randomFill=libopenmpt.randomFill;Module.heap=()=>HEAPU8;Module.functionAt=index=>wasmTable.get(index);return Module;\n}\n', encoding='utf-8')
    shutil.copyfile(decoder / 'bin/wasm/libopenmpt.wasm', audio / 'libopenmpt.wasm')
    files += ['audio/openmpt-runtime.js', 'audio/libopenmpt.wasm']
    for source in (ROOT / 'tools/wasm/game/audio').glob('*.js'):
        shutil.copyfile(source, audio / source.name)
        files.append('audio/' + source.name)
    for source in [decoder / 'license.txt', *(decoder / 'licenses').glob('*')]:
        destination = audio / 'licenses' / source.name
        destination.parent.mkdir(exist_ok=True)
        shutil.copyfile(source, destination)
        files.append('audio/licenses/' + source.name)
    baseline = ROOT / 'out/distribution/v3.3.1/endless-benchmark-v3.3.1.zip'
    with zipfile.ZipFile(baseline) as source:
        for name in ['users.dat', 'user1.dat64', 'game1_58.dat64']:
            relative = 'development-save/' + name
            destination = args.output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            # Regenerate the completed profile: older release ZIPs omitted First Aid.
            data = (runpy.run_path(str(ROOT / 'tools/package-test-saves.py'))['completed_profile']()
                    if name == 'user1.dat64' else source.read('data/' + name))
            destination.write_bytes(data)
            files.append(relative)
    bundle = {name: base64.b64encode((args.output / 'development-save' / name).read_bytes()).decode('ascii') for name in ['users.dat','user1.dat64','game1_58.dat64']}
    (args.output / 'save-bundle.json').write_text(json.dumps(bundle,separators=(',',':')),encoding='utf-8')
    files = [name for name in files if not name.startswith('development-save/')] + ['save-bundle.json']
    source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    manifest = {
        'version': version,
        'backend': 'WebGPU' if args.webgpu else 'WebGL2',
        'webgl_fallback': bool(args.webgl_build),
        'source_commit': source_commit,
        'source_url': 'https://github.com/YuHao2006/pvz_perf/tree/' + source_commit,
        'bypass_bilibili_checks': config['bypassBilibiliChecks'],
        'files': {name: {'bytes': (args.output / name).stat().st_size,
                         'sha256': hashlib.sha256((args.output / name).read_bytes()).hexdigest()}
                  for name in files},
    }
    (args.output / 'web-build-manifest.json').write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    files.append('web-build-manifest.json')
    sizes = {name: (args.output / name).stat().st_size for name in files}
    archive_path = args.output.parent / ('pvz-perf-toy-bilibili.zip' if args.bilibili else 'pvz-perf-toy.zip')
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in files:
            archive.write(args.output / name, name)
    print(json.dumps({'files': sizes, 'total_bytes': sum(sizes.values()),
                      'zip_bytes': archive_path.stat().st_size,
                      'under_50MB': sum(sizes.values()) < 50_000_000,
                      'under_50MiB': sum(sizes.values()) < 50 * 1024 * 1024,
                      'zip_under_50MB': archive_path.stat().st_size < 50_000_000}, indent=2))


if __name__ == '__main__':
    main()
