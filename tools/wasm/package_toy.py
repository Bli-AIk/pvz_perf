"""Stage a relative-path Toy site; keep the uncompressed Wasm in the build tree."""
import argparse
import gzip
import json
from pathlib import Path
import shutil
import runpy
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/wasm/game/site')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/wasm/toy')
    parser.add_argument('--config', type=Path, default=ROOT / 'tools/wasm/game/web-config.json')
    parser.add_argument('--bilibili', action='store_true', help='Disable API bypass; game files remain identical')
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding='utf-8'))
    if not isinstance(config.get('bypassBilibiliChecks'), bool):
        raise ValueError('bypassBilibiliChecks must be a boolean')
    if config.get('perfectSaveRequirement') not in ('following', 'coin'):
        raise ValueError('perfectSaveRequirement must be following or coin')
    videos = config.get('rewardVideoBvids')
    if not isinstance(videos, list) or any(not isinstance(video, str) or not video.startswith('BV') for video in videos):
        raise ValueError('rewardVideoBvids must be an array of BV strings')
    if args.bilibili:
        config['bypassBilibiliChecks'] = False
    required = ['pvz-perf.js', 'pvz-perf.wasm', 'pvz-perf.data']
    for name in required:
        if not (args.build / name).is_file():
            raise FileNotFoundError(args.build / name)
    args.output.mkdir(parents=True, exist_ok=True)
    support_files = ['index.html', 'launcher.js', 'launcher.css', 'rewards.js', 'text-input.js', 'save-catalog.json', 'serve.py',
                     'start-local.cmd', 'start-local.sh', 'README.txt']
    for name in support_files:
        shutil.copyfile(ROOT / 'tools/wasm/game' / name, args.output / name)
    (args.output / 'web-config.js').write_text('window.PVZWebConfig = ' + json.dumps(config, ensure_ascii=False, indent=2) + ';\n', encoding='utf-8')
    for name in ['pvz-perf.js', 'pvz-perf.data']:
        shutil.copyfile(args.build / name, args.output / name)
    (args.output / 'pvz-perf.wasm.gz').write_bytes(gzip.compress((args.build / 'pvz-perf.wasm').read_bytes(), compresslevel=9, mtime=0))
    files = support_files + ['web-config.js', 'pvz-perf.js', 'pvz-perf.data', 'pvz-perf.wasm.gz']
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
